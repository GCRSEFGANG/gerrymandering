"""
Functions for partisanship refinement step in communities algorithm
"""


from hacking_the_election.utils.compactness import (
    GIVE_PRECINCTS_COMPACTNESS_KWARGS
)
from hacking_the_election.utils.exceptions import (
    CreatesMultiPolygonException,
    ZeroPrecinctCommunityException
)


GIVE_PRECINCTS_COMPACTNESS_KWARGS = {
    "partisanship": False,
    "standard_deviation": False,
    "population": False, 
    "compactness": True, 
    "coords": True
}


def get_average_compactness(communities):
    """
    Returns average schwartzberg compactness value of community in
    `communities`. Does not automatically update values.
    """
    compactness_values = [c.compactness for c in communities]
    return sum(compactness_values) / len(compactness_values)


def format_float(n):
    """
    Returns float with 3 places to the left and 3 to the right of the
    decimal point.
    """

    n_chars = list(str(round(n, 3)))

    # Add zeroes on right side.
    n_right_digits = len("".join(n_chars).split(".")[-1])
    if n_right_digits < 3:
        for i in range(3 - n_right_digits):
            n_chars.append("0")
    
    # Add zeroes on left side.
    n_left_digits = len("".join(n_chars).split(".")[0])
    if n_left_digits < 3:
        for i in range(3 - n_left_digits):
            n_chars.insert(0, "0")

    return "".join(n_chars)


def add_precinct(communities, community, precinct):
    """
    This function probably does something.

    I don't really care,
    because nobody except myself will read this code anyway.
    """
    for other_community in communities:
        if precinct in other_community.precincts.values():
            try:
                other_community.give_precinct(
                    community,
                    precinct.vote_id,
                    **GIVE_PRECINCTS_COMPACTNESS_KWARGS
                )
                community.update_compactness()
                other_community.update_compactness()
            except (CreatesMultiPolygonException,
                    ZeroPrecinctCommunityException):
                pass