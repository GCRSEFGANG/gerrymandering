"""
A class representing the smallest unit of voter
data in our project's data files: the precinct.
"""


class Precinct:
    """Class containing voter, population, and geo data for a single precinct.
    Should be pointed to by node in `pygraph.classes.graph.graph` object.

    :param rep_data: Republican voter counts corresponding to elections.
    :type rep_data: dict

    Example value for `rep_data`: 
    ``{"ush_r_06": 832, "uss_r_06": 123}``

    :param dem_data: Democrat voter counts corresponding to elections.
    :type dem_data: dict

    :param pop: Population of the precinct.
    :type pop: int

    :param coords: Geometric shape of precinct.
    :type coords: `shapely.geometry.Polygon`

    :param geoid: GEOID or similar identifier string for precinct.
    :type geoid: str
    """

    def __init__(self, pop, coords, state, precinct_id, rep_data, dem_data, 
                 green_data=None, lib_data=None, reform_data=None, ind_data=None, const_data=None):

        self.pop = pop
        # should be shapely polygon
        self.coords = coords
        # centroid will be in list form, i.e. [35.274923, 71.47102]
        self.centroid = list(self.coords.centroid.coords[0])
        self.state = state
        self.id = precinct_id
        # used for data visualization
        self.community = None
        
        self.rep_data = rep_data
        self.dem_data = dem_data
        # Many of the following attributes may be none
        # Green Party of the United States
        self.green_data = green_data
        # Libertarian Party
        self.lib_data = lib_data
        # Reform Party of the United States of America
        self.reform_data = reform_data
        # Votes for Independents (and writeins, if applicable)
        self.ind_data = ind_data
        # Constitution Party
        self.const_data = const_data
        
        total_rep = sum(self.rep_data.values())
        total_dem = sum(self.dem_data.values())
        if green_data:
            total_green = sum(self.green_data.values())
        else:
            total_green = 0

        if lib_data:
            total_lib = sum(self.lib_data.values())
        else:
            total_lib = 0

        if reform_data:
            total_reform = sum(self.reform_data.values())
        else:
            total_reform = 0

        if ind_data:
            total_ind = sum(self.ind_data.values())
        else:
            total_ind = 0

        if const_data:
            total_const = sum(self.const_data.values())
        else:
            total_const = 0
            
        try:
            self.partisanship = total_rep / (total_rep + total_dem)
        except ZeroDivisionError:
            # No voters in precinct.
            self.partisanship = 0
