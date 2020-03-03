"""
Functions specific to initial configuration step in communities algorithm
"""


import math
import pickle
import random
import threading

from shapely.geometry import MultiPolygon, Polygon

from .geometry import (
    clip,
    DIFFERENCE,
    get_area_intersection,
    get_distance,
    get_if_bordering,
    get_point_in_polygon,
    polygon_to_shapely,
    shapely_to_polygon,
    UNION
)
from funcs import print_time


# ===================================================
# custom exceptions


class LoopBreakException(Exception):
    """
    Used to break outer loops from within nested loops.
    """


class CommunityFillCompleteException(Exception):
    """
    Used to break out of all levels of recursion when a community is filled.
    """

    def __init__(self, added_precincts, unchosen_precincts_border):
        self.added_precincts = added_precincts
        self.unchosen_precincts_border = unchosen_precincts_border
        Exception.__init__(self)


# ===================================================
# Community algorithm-specifc functions and classes:


class Community:
    """
    A collection of precincts
    """
    
    def __init__(self, precincts, identifier, islands, coords=False):
        self.precincts = {precinct.vote_id: precinct for precinct in precincts}
        self.id = identifier
        if precincts != []:
            if coords:
                self.coords = coords
            else:
                self.coords = \
                    clip([p.coords for p in self.precincts.values()], UNION)
        else:
            self.coords = Polygon()
        self.partisanship = None
        self.standard_deviation = None
        self.population = None
        self.compactness = None

        # dict with keys as index and values as number of precincts.
        self.islands = islands

    def update_compactness(self):
        """
        Updates the `compactness` attribute.

        Implemenatation of this algorithm (schwartzberg):
        https://fisherzachary.github.io/public/r-output.html
        """
        area = self.coords.area
        circumference = 2 * math.pi * math.sqrt(area / math.pi)
        perimeter = self.coords.length
        self.compactness = circumference / perimeter

    def update_standard_deviation(self):
        """
        Updates the `standard_deviation` attribute.
        """

        rep_percentages = [
            p.r_election_sum / (p.r_election_sum + p.d_election_sum) * 100
            for p in self.precincts.values() if (p.r_election_sum + p.d_election_sum) != 0]
        try:
            mean = sum(rep_percentages) / len(rep_percentages)
        except ZeroDivisionError:
            self.standard_deviation = 0.0
        else:
            self.standard_deviation = math.sqrt(sum([(p - mean) ** 2 
                                      for p in rep_percentages]))

    def update_partisanship(self):
        """
        Updates the `partisanship` attribute
        """

        rep_sum = 0
        total_sum = 0
        for precinct in self.precincts.values():
            if (r_sum := precinct.r_election_sum) != -1:
                rep_sum += r_sum
                total_sum += r_sum + precinct.d_election_sum
        self.partisanship = rep_sum / total_sum

    def give_precinct(self, other, precinct_id, coords=True,
                      partisanship=True, standard_deviation=True,
                      population=True, compactness=True):
        """
        Gives precinct from self to other community.

        All parameters with default value of `True` indicate whether or
        not to update that attribute after the community is given.
        """

        if not isinstance(other, Community):
            raise TypeError(f"Invalid type {type(other)}.\n"
                             "Can only give precinct to community.")
        try:
            precinct = self.precincts[precinct_id]
        except KeyError:
            raise ValueError(
                f"No precinct in community with id '{precinct_id}.'")

        del self.precincts[precinct_id]
        other.precincts[precinct_id] = precinct
        # Update borders
        if coords:
            def update_self_coords():
                self.coords = clip([self.coords, precinct.coords],
                                   DIFFERENCE)
            def update_other_coords():
                other.coords = clip([p.coords for p in other.precincts.values()],
                                    UNION)
            thread_1 = threading.Thread(target=update_self_coords)
            thread_2 = threading.Thread(target=update_other_coords)
            thread_1.run()
            thread_2.run()

        # Update other attributes that are dependent on precincts attribute
        for community in [self, other]:
            if partisanship:
                community.update_partisanship
            if standard_deviation:
                community.update_standard_deviation
            if population:
                community.population = sum(
                    [precinct.population for precinct in
                     community.precincts.values()])
            if compactness:
                community.update_compactness

    def try_precinct(self, unchosen_precincts, tried_precincts,
                     linked_precincts, island_index, island_border):
        """
        Tries to select a random precinct. If that doesnt work, raises a 
        """
        try:
            # Give random precinct to `community`
            random_precinct = random.sample(
                self.get_bordering_precincts(unchosen_precincts) \
                - tried_precincts - linked_precincts, 1)[0]
            return random_precinct
        except ValueError:
            # No precincts can be added without creating island.
            print(f"restarting filling of community {self.id}")
            added_precincts, unchosen_precincts_border = \
                self.fill(precincts, linked_precincts,
                          island_index, Polygon(island_border))
            raise CommunityFillCompleteException(added_precincts,
                                                 unchosen_precincts_border)

    def fill(self, precincts, linked_precincts, island_index, island_border):
        """
        Fills a community up with precincts.

        Args:
            `precincts`:        List of Precinct objects in the island the
                                community is on.
            `linked_precincts`: Set of precincts that are meant to be part
                                of communities that span islands, therefore
                                making them untouchable during this step.
            `island_index`    : Index of island that corresponds to
                                a key in `self.islands`.
            `island_border`   : Border of island with index `island_index`

        Returns list of added precincts and Polygon that is outer border
        of group of precincts on island that have not been added to a
        community.
        """

        added_precincts = []
        kwargs = {
            "partisanship": False,
            "standard_deviation": False,
            "population": False, 
            "compactness": False, 
            "coords": True
        }
        unchosen_precincts = Community(
            precincts[:],  # copy
            0,
            {},
            Polygon(island_border)  # copy
        )

        for p in range(self.islands[island_index]):

            # Set of precincts that have been tried
            tried_precincts = set()

            if p == 1:
                # Choose precinct that is on outside border of island.
                border_precincts = {
                    precinct for precinct in self.precincts.values()
                    if get_if_bordering(precinct.coords, island_border)
                }
                if border_precincts != set():
                    eligible_precincts = \
                        (self.get_bordering_precincts(unchosen_precincts) \
                         - tried_precincts - linked_precincts) & border_precincts
                    random_precinct = random.sample(
                        eligible_precincts, 1)[0]
                else:
                    random_precinct = self.try_precinct(
                        unchosen_precincts,
                        tried_precincts,
                        linked_precincts,
                        island_index,
                        island_border
                    )
            else:
                random_precinct = self.try_precinct(
                    unchosen_precincts,
                    tried_precincts,
                    linked_precincts,
                    island_index,
                    island_border
                )

            unchosen_precincts.give_precinct(
                self, random_precinct, **kwargs
            )

            # Keep trying other precincts until one of them
            # doesn't make an island.
            while isinstance(unchosen_precincts.coords, MultiPolygon):
                # Give it back
                self.give_precinct(
                    unchosen_precincts, random_precinct, **kwargs)
                print(f"precinct {random_precinct} added to and removed "
                      f"from community {self.id} because it created an "
                      "island")
                # Random precinct that hasn't already been
                # tried and also borders community.
                random_precinct = self.try_precinct(
                    unchosen_precincts,
                    tried_precincts,
                    linked_precincts,
                    island_index,
                    island_border
                )
                unchosen_precincts.give_precinct(
                    self, random_precinct, **kwargs)
                tried_precincts.add(random_precinct)

            added_precincts.append(self.precincts[random_precinct])
            print(f"precinct {random_precinct} added to community {self.id}."
                  f"Number of precincts is now {len(self.precincts)}")

        raise CommunityFillCompleteException(added_precincts,
                                             unchosen_precincts.coords)

    def get_bordering_precincts(self, unchosen_precincts):
        """
        Returns list of precincts bordering `self`

        `unchosen_precincts` is a Community object that contains all
        the precincts in the state that haven't already been added to
        a community
        """
        bordering_precincts = set()
        if self.precincts != {}:
            # create 20 threads that will simeltaneously calculate
            # whether or not a certain number of precincts are
            # bordering self.coords

            precincts = list(unchosen_precincts.precincts.values())
            n_precincts = len(precincts)
            precincts_per_thread = n_precincts // 20
            # precincts that will be calculated in each thread.
            thread_precincts = [
                precincts[i:i + precincts_per_thread] for i in
                range(0, n_precincts - (n_precincts % 20), precincts_per_thread)
            ]
            thread_precincts.append(precincts[n_precincts % 20 : n_precincts])

            for precinct_group in thread_precincts:
                def thread_func():
                    for precinct in precinct_group:
                        if get_if_bordering(precinct.coords, self.coords):
                            bordering_precincts.add(precinct.vote_id)
                thread = threading.Thread(target=thread_func)
                thread.run()
        else:
            bordering_precincts = set(unchosen_precincts.precincts.keys())
        return bordering_precincts


def group_by_islands(precincts):
    """
    Returns list of precincts sorted into
    lists by what island they are on.
    """
    state_border = clip([p.coords for p in precincts], UNION)

    if isinstance(state_border, MultiPolygon):
        # There are islands in the state.

        island_polygons = state_border.geoms
        islands = [[] for _ in range(len(island_polygons))]

        for precinct in precincts:
            for i, island in enumerate(island_polygons):
                # If the first point of the precinct is in this island,
                # all of them must be.
                if get_point_in_polygon(
                        island, list(precinct.coords.exterior.coords[1])):
                    islands[i].append(precinct)
        return islands
    else:
        return [[p for p in precincts]]


def get_closest_precinct_on_island(island_centroid,
                                   other_island_border_precincts,
                                   other_island_border):
    """
    Finds precinct on `other_island` that is closest to `island`

    Args:
        `island_centroid`:               Centroid of island.
        `other_island_border_precincts`: Precincts on border of other
                                         island.
        `other_island_border`:           Polygon that is outside border
                                         of other island

    Returns `save_precincts.Precinct` object.
    """
    closest_precinct = None
    closest_precinct_distance = 0
    for p in other_island_border_precincts:
        distance = get_distance(island_centroid,
                                 p.coords.centroid.coords[0])
        if (
                closest_precinct is None
                or distance < closest_precinct_distance):
            # If removing this precinct from the island
            # creates an isolated section within the island.
            if isinstance(clip([other_island_border, p.coords], DIFFERENCE),
                          MultiPolygon):
                continue
            else:
                closest_precinct = p
                closest_precinct_distance = distance
            closest_precinct = p
            closest_precinct_distance = distance

    return closest_precinct, closest_precinct_distance


def get_closest_precinct(island, island_precinct_groups,
                         island_borders, island_border,
                         state_island_borders):
    """
    Finds precinct on border of any island in `island_precinct_groups`
    that is closest to `island`. Meant to be used as second precinct in
    a "linked pair" of precincts.
    
    Args:
        `island`:                 List of `save_precincts.Precinct`
                                  objects that is create the island
                                  you want to find the precinct that
                                  is closest to.
        `island_precinct_groups`: List of lists of
                                  `save_precincts.Precinct` objects
                                  grouped by islands to search for
                                  close precincts in.
        `island_borders`:         List of Polygons that are borders of
                                  all the islands in the state.
        `island_border`:          Polygon that is the border of
                                  `island`.
        `state_island_borders`:   Border of every island in state (so
                                  that the indices of islands can be
                                  found.)

    Returns vote_id attribute of closest precinct and index of island
    that contains that precinct.
    """
    # List of precincts that border the "coastline" of the each island.
    # Grouped by island.
    border_precincts = \
        [[precinct for precinct in il
          if get_if_bordering(precinct.coords, i_border)]
         for i_border, il in zip(island_borders, island_precinct_groups)]

    island_centroid = island_border.centroid.coords[0]
    
    closest_precinct = None
    closest_precinct_distance = 0
    closest_precinct_island_index = 0

    for border, island_border_precincts in zip(island_borders,
                                               border_precincts):
        closest_precinct_on_island, distance = \
            get_closest_precinct_on_island(
                island_centroid, island_border_precincts, island_border)
        if (
                closest_precinct is None
                or closest_precinct_distance > distance):
            closest_precinct_distance = distance
            closest_precinct = closest_precinct_on_island
            closest_precinct_island_index = state_island_borders.index(border)

    return closest_precinct, closest_precinct_island_index


def get_precinct_link_pair(island, island_precinct_groups,
                           island_border, island_borders,
                           state_island_borders):
    """
    Finds pair of two closest precincts on borders of `island` and any
    island in `island_precinct_groups`.

    Args:
        `island`:                 List of `save_precincts.Precinct`
                                  objects that is create the island
                                  you want to find the precinct that
                                  is closest to.
        `island_precinct_groups`: List of lists of
                                  `save_precincts.Precinct` objects
                                  grouped by islands to search for
                                  close precincts in.
        `island_border`:          Polygon that is the border of `island`
        `island_borders`:         List of Polygons that are borders of
                                  all the islands in the state.
        `state_island_borders`:   Border of every island in state (so
                                  that the indices of islands can be
                                  found.)

    Returns two strings (vote_ids of precinct on `island` and other
    precinct) and one integer (index of the island the other precinct
    is on).
    """

    precinct1, closest_precinct_island_index = get_closest_precinct(
        island, island_precinct_groups, island_borders,
        island_border, state_island_borders)
    
    island_border_precincts = \
        [p for p in island if get_if_bordering(island_border, p.coords)]

    precinct2, _ = get_closest_precinct_on_island(
            state_island_borders[
                closest_precinct_island_index].centroid.coords[0],
            island_border_precincts,
            island_border
        )

    return precinct1, precinct2, closest_precinct_island_index