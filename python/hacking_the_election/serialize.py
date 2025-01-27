"""
Script for serializing precinct-level election, geo, and population
data.
Data is serialized into:
 - .pickle file containing graph with nodes containing Precinct objects
Usage:
python3 serialize.py [election_file] [geo_file] [pop_file] [state] [output.pickle]


Note: 
state may need to have an underscore and the year attached, e.g. "georgia_2008" if applicable
"""


import json
from os.path import dirname, abspath
import pickle
import sys
from time import time
from itertools import combinations

sys.path.append(dirname(dirname(abspath(__file__))))
from pygraph.classes.graph import graph
from pygraph.classes.exceptions import AdditionError
from shapely.geometry import Polygon, MultiPolygon

from hacking_the_election.utils.precinct import Precinct
from hacking_the_election.utils.geometry import geojson_to_shapely, get_if_bordering
from hacking_the_election.utils.graph import get_components
from hacking_the_election.visualization.graph_visualization import visualize_graph
from hacking_the_election.utils.serialization import (
    compare_ids, 
    split_multipolygons, 
    combine_holypolygons, 
    remove_ring_intersections
)


SOURCE_DIR = abspath(dirname(__file__))


def convert_to_float(string):
    """
    Wrapped error handling for float().
    """
    try:
        return float(string)
    except ValueError:
        if "." in string:
            try:
                return float(string[:string.index(".")])
            except ValueError:
                print('Float Conversion Faliure:', string)
                return 0
        else:
            return 0


def tostring(string):
    """
    Removes redundant quotes or converts int to string.
    """
    try:
        if string[0] in ["\"", "'"] and string[-1] in ["\"", "'"]:
            return string[1:-1]
        else:
            return string
    except TypeError:  # most likely that `string` was of type `int`
        return str(string)


def create_graph(election_file, geo_file, pop_file, state):
    """
    Returns graph with precinct ids stored in nodes (string of XML)
    and list of Precinct objects.
    """
    start_time = time()
    with open(f"{SOURCE_DIR}/state_metadata.json", "r") as f:
        state_metadata = json.load(f)

    # This should be a race for president
    dem_key = state_metadata[state]["party_data"]["dem_keys"][0]
    rep_key = state_metadata[state]["party_data"]["rep_keys"][0]
    try:
        total_key = state_metadata[state]["party_data"]["total_keys"][0]
    except:
        total_key = None
    try:
        other_key = state_metadata[state]["party_data"]["other_keys"][0]
    except:
        other_key = None

    try:
        green_key = state_metadata[state]["party_data"]["green_keys"][0]
    except:
        green_key = None
    try:
        lib_key = state_metadata[state]["party_data"]["libertarian_keys"][0]
    except:
        lib_key = None
    try:
        reform_key = state_metadata[state]["party_data"]["reform_keys"][0]
    except:
        reform_key = None
    try:
        ind_key = state_metadata[state]["party_data"]["independent_keys"][0]
    except:
        ind_key = None
    try:
        const_key = state_metadata[state]["party_data"]["constitution_keys"][0]
    except:
        const_key = None 

    json_ids  = state_metadata[state]["geo_id"]
    json_pops = state_metadata[state]["pop_key"]
    ele_ids   = state_metadata[state]["ele_id"]

    # Water precincts in geodata that should be excluded have this id 
    # for whatever json_id should normally be there.
    non_precinct_ids = state_metadata[state]["non_precinct_ids"]

    # Read election and geo data files.
    with open(geo_file, "r") as f:
        geodata = json.load(f)

    if election_file != "none":
        # Find election precincts not in geodata and vice versa

        if election_file[-5:] == ".json":
            election_data_type = "json"
            with open(election_file, "r") as f:
                election_data = json.load(f)

            election_data_ids = [tostring("".join([precinct["properties"][ele_id] for ele_id in ele_ids]))
                                 for precinct in election_data["features"]]
            geodata_ids = [tostring("".join([precinct["properties"][json_id] for json_id in json_ids]))
                           for precinct in geodata["features"]]
            
            election_data_to_geodata = compare_ids(election_data_ids, geodata_ids)

        elif election_file[-4:] == ".tab":
            election_data_type = "tab"
            with open(election_file, "r") as f:
                election_file_contents = f.read().strip()
            # create a list of rows that are a list of values in that row
            election_data = [line.split("\t") for line in
                            election_file_contents.split("\n")]
            ele_id_col_indices = [i for i, header in enumerate(election_data[0]) if header in ele_ids]
            election_data_ids = [tostring("".join([election_data_row[ele_id_index] for ele_id_index in ele_id_col_indices]))
                                 for election_data_row in election_data[1:]]
            geodata_ids = [tostring("".join([precinct["properties"][json_id] for json_id in json_ids])) 
                           for precinct in geodata["features"]]
            election_data_to_geodata = compare_ids(election_data_ids, geodata_ids)
        else:
            # Election Data can only be .tab or .json files
            raise ValueError('.json or .tab file required')

    else:
        election_data_type = False

    if pop_file != "none":
        # Finds population data, any population_ids to geodata_ids will be done when finding pop
        if pop_file[-4:] == ".json":
            pop_data_type = "json"
            with open(pop_file, "r") as f:
                popdata = json.load(f)
        elif pop_file[-4:] == ".tab":
            pop_data_type = "tab"
            with open(pop_file, 'r') as f:
                popdata = f.read().strip()
        else:
            # Population Data can only be .tab or .json files
            raise ValueError('.json or .tab file required')
    else:
        pop_data_type = False

    unordered_precinct_graph = graph()


    # Get election data. If needed, converts election data ids to geodata ids 
    # using election_data_to_geodata, thus all keys are those of geodata.
    # {precinct_id: [{dem:data}, {rep:data}], [{green:data}, {lib:data}] as neeeded}
    precinct_election_data = {}
    # If there is an election data file...
    if election_data_type:
        # If the election data file is a .json file
        if election_data_type == "json":
             # Fill precinct_election_data
            for precinct1 in election_data["features"]:
                properties1 = precinct1["properties"]
                # Convert election data id to geodata id
                try:
                    precinct_id1 = election_data_to_geodata[
                        tostring("".join(properties1[ele_id] for ele_id in ele_ids))
                    ]
                except KeyError:
                    continue
                party_data = [
                    {'dem': convert_to_float(properties1[dem_key])},
                    {'rep': convert_to_float(properties1[rep_key])}
                ]
                if total_key:
                    remainder = {'other': convert_to_float(properties1[total_key]) - sum(
                        [properties1[key] for key in [dem_key, rep_key, green_key, lib_key, reform_key, ind_key, const_key] if key != None]
                    )}
                    if list(remainder.values())[0] < 0:
                        if list(remainder.values())[0] < -1:
                            raise Exception(f'Negative other votes: {list(remainder.values())[0]} from precinct {precinct_id1}')
                        else:
                            print(f'Note: Negative other votes: {list(remainder.values())[0]} from precinct {precinct_id1}, rounded up to 0')
                    else:
                        party_data.append(remainder)
                # If there is already a total_key, the other_key id is not needed.
                elif other_key:
                    party_data.append({'other': convert_to_float(properties1[other_key])})


                if green_key:
                    party_data.append({'green': convert_to_float(properties1[green_key])})
                if lib_key:
                    party_data.append({'lib': convert_to_float(properties1[lib_key])})
                if reform_key:
                    party_data.append({'reform': convert_to_float(properties1[reform_key])})
                if ind_key:
                    party_data.append({'ind': convert_to_float(properties1[ind_key])})
                if const_key:
                    party_data.append({'const': convert_to_float(properties1[const_key])})

                precinct_election_data[precinct_id1] = party_data
        # If the election data file is a .tab file
        elif election_data_type == "tab":
            total_sum = 0
            # headers for different categories
            election_column_names = election_data[0]
            # Get the index of each of the relevant columns.
            dem_key_col_index = \
                [i for i, col in enumerate(election_column_names)
                if col == dem_key][0]

            rep_key_col_index = \
                [i for i, col in enumerate(election_column_names)
                if col == rep_key][0]

            ele_id_col_indices = [i for i, header in enumerate(election_column_names) if header in ele_ids]

            # Fill precinct_election_data
            for precinct in election_data[1:]:
                # convert election data id to geodata id
                try:
                    election_data_id = election_data_to_geodata[
                        tostring("".join([precinct[ele_id_col_index] for ele_id_col_index in ele_id_col_indices]))
                    ]
                except KeyError:
                    continue
                party_data = [
                    {'dem': precinct[dem_key_col_index]
                },
                    {'rep': precinct[rep_key_col_index]
                },
                ]
                if total_key:
                    # I apologize. This basically finds indexes, then subtracts values from total, even
                    # if it doesn't look like it.
                    total_key_col_index = \
                        [i for i, col in enumerate(election_column_names)
                        if col == total_key][0]
                    total_sum += convert_to_float(precinct[total_key_col_index])
                    remainder = {'other': convert_to_float(precinct[total_key_col_index]) 
                        - sum([convert_to_float(precinct[[i for i, col in enumerate(election_column_names) if col == key][0]]) 
                        for key in [dem_key, rep_key, green_key, lib_key, reform_key, ind_key, const_key] if key != None]
                    )}
                    if list(remainder.values())[0] < 0:
                        # if list(remainder.values())[0] < -1:
                        #     raise Exception(f'Negative other votes: {list(remainder.values())[0]} from precinct {election_data_id}')
                        # else:
                        print(f'Note: Negative other votes: {list(remainder.values())[0]} from precinct {election_data_id}, rounded up to 0')
                    else:
                        party_data.append(remainder)
                # If there is already a total_key, the other_key id is not needed.
                elif other_key:
                    other_key_col_index = \
                        [i for i, col in enumerate(election_column_names)
                        if col == other_key][0]
                    party_data.append({'other': convert_to_float(precinct[other_key_col_index])})


                if green_key:
                    key_index = [i for i, col in enumerate(election_column_names)
                        if col == green_key][0]
                    party_data.append({'green': convert_to_float(precinct[key_index])})
                if lib_key:
                    key_index = [i for i, col in enumerate(election_column_names)
                        if col == lib_key][0]
                    party_data.append({'lib': convert_to_float(precinct[key_index])})
                if reform_key:
                    key_index = [i for i, col in enumerate(election_column_names)
                        if col == reform_key][0]
                    party_data.append({'reform': convert_to_float(precinct[key_index])})
                if ind_key:
                    key_index = [i for i, col in enumerate(election_column_names)
                        if col == ind_key][0]
                    party_data.append({'ind': convert_to_float(precinct[key_index])})
                if const_key:
                    key_index = [i for i, col in enumerate(election_column_names)
                        if col == const_key][0]
                    party_data.append({'const': convert_to_float(precinct[key_index])})

                precinct_election_data[election_data_id] = party_data
    else:
        # Fill precinct_election_data
        for precinct in geodata["features"]:
            properties = precinct["properties"]
            precinct_id = tostring("".join(properties[json_id] for json_id in json_ids))
            party_data = [
                {'dem': convert_to_float(properties[dem_key])},
                {'rep': convert_to_float(properties[rep_key])}
            ]
            if total_key:
                remainder = {'other': convert_to_float(properties[total_key]) - sum(
                        [properties[key] for key in [dem_key, rep_key, green_key, lib_key, reform_key, ind_key, const_key] if key != None]
                    )}
                if list(remainder.values())[0] < 0:
                    # if list(remainder.values())[0] < -1:
                    #     raise Exception(f'Negative other votes: {list(remainder.values())[0]} from precinct {precinct_id}')
                    # else:
                    print(f'Note: Negative other votes: {list(remainder.values())[0]} from precinct {precinct_id}, rounded up to 0')
                else:
                    party_data.append(remainder)
            # If there is already a total_key, the other_key id is not needed.
            elif other_key:
                party_data.append({'other': convert_to_float(properties[other_key])})


            if green_key:
                party_data.append({'green': convert_to_float(properties[green_key])})
            if lib_key:
                party_data.append({'lib': convert_to_float(properties[lib_key])})
            if reform_key:
                party_data.append({'reform': convert_to_float(properties[reform_key])})
            if ind_key:
                party_data.append({'ind': convert_to_float(properties[ind_key])})
            if const_key:
                party_data.append({'const': convert_to_float(properties[const_key])})
            precinct_election_data[precinct_id] = party_data


    # Then find population. pop should have keys that use geodata ids
    # {precinct_id : population}
    pop = {}
    if pop_data_type:
        if pop_data_type == "json":
            for pop_precinct in popdata["features"]:
                pop_properties = pop_precinct["properties"]
                # Assumes the same Ids are used in geodata as in population data
                pop_precinct_id = tostring("".join(pop_properties[json_id] for json_id in json_ids))
                if sum([pop_properties[key] for key in json_pops]) < 0:
                    raise Exception(f'Negative population: {sum([pop_properties[key] for key in json_pops])} from precinct {pop_precinct_id}')
                pop[pop_precinct_id] = sum([pop_properties[key] for key in json_pops])
        # individual conditionals for states, they should go here
        # elif pop_data_type == "tab":
            # In these condtionals, pop should have keys that match geodata. this is done here. 
            # compare_ids() from the serialization file in utils can be used for population
        #     if state == "south carolina":
        #         for precinct in geodata["features"]:
        #             precinct_id = tostring("".join(str(precinct["properties"][json_id]) for json_id in json_ids)) 
        #             pop_dict = {row.split("\t")[0] :
        #                 row.split("\t")[1]
        #                 for row in popdata.split("\n")}
        #             try:
        #                 pop[precinct_id] = pop_dict[precinct_id]
        #             except KeyError:
        #                 print(f"Failed to find value of {precinct_id}")
        # else:
            # raise AttributeError
    else:
        # find where population is stored, either election or geodata
        try:
            _ = geodata["features"][0]["properties"][json_pops[0]]
        except (ValueError or KeyError):
            pop_data_type = "tab"
            # population is in election data
            pop_col_indices = [i for i, header in enumerate(election_data[0]) if header in json_pops]
            # find which indexes have population data
            for row in election_data[1:]:
                pop_precinct_id = election_data_to_geodata[
                    tostring("".join([row[ele_id_col_index] for ele_id_col_index in ele_id_col_indices]))
                ]
                precinct_populations = [convert_to_float(row[i]) for i in pop_col_indices]
                if sum(precinct_populations) < 0:
                    raise Exception(f'Negative population: {precinct_populations} from precinct {pop_precinct_id}')
                pop[pop_precinct_id] = sum(precinct_populations)
        else:
            # population is in geodata
            for geodata_pop_precinct in geodata["features"]:
                geodata_pop_properties = geodata_pop_precinct["properties"]
                geodata_pop_precinct_id = tostring("".join(str(geodata_pop_properties[json_id]) for json_id in json_ids))
                precinct_populations = [convert_to_float(geodata_pop_properties[key]) for key in json_pops]
                if sum(precinct_populations) < 0:
                    raise Exception(f'Negative population: {precinct_populations} from precinct {pop_precinct_id}')
                pop[geodata_pop_precinct_id] = sum(precinct_populations)

    # Create a geodata dictionary
    geodata_dict = {
        tostring("".join(str(precinct["properties"][json_id]) for json_id in json_ids)) :
        precinct["geometry"]["coordinates"]
        for precinct in geodata["features"]
        if not any([non_precinct_id in 
        tostring("".join(precinct["properties"][json_id] for json_id in json_ids))
        for non_precinct_id in non_precinct_ids])
    }
    with open('./maryland_election_data.tab', 'a') as f:
        for precinct_id, data in precinct_election_data.items():
            f.write(str(precinct_id))
            for party in data:
                f.write('\t')
                f.write(str(list(party.values())[0]))
            f.write('\n')
    # Remove multipolygons from our dictionaries. (This is so our districts/communities stay contiguous)
    split_multipolygons(geodata_dict, pop, precinct_election_data)
    # Combine precincts with holes in them
    combine_holypolygons(geodata_dict, pop, precinct_election_data)
    # Change geometry to prevent invalid Shapely polygons
    remove_ring_intersections(geodata_dict)
    # Modify geodata_dict to use 'shapely.geometry.Polygon's
    geodata_dict = {
        precinct :
        geojson_to_shapely(coords)
        for precinct, coords in geodata_dict.items()
    }

    # Create list of Precinct objects
    precinct_list = []
    for precinct_id, coordinate_data in geodata_dict.items():
        try:
            precinct_pop = pop[precinct_id]
            precinct_election = {
                (list(party_dict.keys())[0] + '_data') :
                (list(party_dict.values())[0])
                for party_dict in precinct_election_data[precinct_id]
            }
        # Precincts in geodata that are not in election or population data, just skip
        except KeyError:
            continue
        else:
            precinct_list.append(
                Precinct(precinct_pop, coordinate_data, state, precinct_id, **precinct_election)
        )
    print(f"{state} dem votes: {sum([precinct.total_dem for precinct in precinct_list])}")
    print(f"{state} rep votes: {sum([precinct.total_rep for precinct in precinct_list])}")
    if precinct_list[0].total_green != 0:
        print(f"{state} green votes: {sum([precinct.total_green for precinct in precinct_list])}")
    if precinct_list[0].total_lib != 0:
        print(f"{state} libertarian votes: {sum([precinct.total_lib for precinct in precinct_list])}")
    if precinct_list[0].total_reform != 0:
        print(f"{state} reform votes: {sum([precinct.total_reform for precinct in precinct_list])}")
    if precinct_list[0].total_const != 0:
        print(f"{state} constitution votes: {sum([precinct.total_const for precinct in precinct_list])}")
    if precinct_list[0].total_ind != 0:
        print(f"{state} independent votes: {sum([precinct.total_ind for precinct in precinct_list])}")
    if precinct_list[0].total_other != 0:
        print(f"{state} other votes: {sum([precinct.total_other for precinct in precinct_list])}")


    # Add nodes to our unordered graph
    for i, precinct in enumerate(precinct_list):
        unordered_precinct_graph.add_node(i, attrs=[precinct])
    node_num = len(unordered_precinct_graph.nodes())
    print('Nodes: ', node_num)
    print(time() - start_time)

    # Add edges to our graph
    completed_precincts = []
    for node in unordered_precinct_graph.nodes():
        coordinate_data = unordered_precinct_graph.node_attributes(node)[0].coords
        min_x, min_y, max_x, max_y = coordinate_data.bounds
        x_length = max_x - min_x
        y_length = max_y - min_y
        # Given a bounding box, how much it should be scaled by for precincts to consider
        scale_factor = 0.02

        min_x -= (x_length * scale_factor)
        max_x += (x_length * scale_factor)
        min_y -= (y_length * scale_factor)
        max_y += (y_length * scale_factor)
        precincts_to_check = []
        for check_node in unordered_precinct_graph.nodes():
            # If the node being checked is the node we checking to
            if check_node == node:
                continue
            # If the node being checked already has edges, then no point in checking
            if check_node in completed_precincts:
                continue
            # If there is already an edge between the two
            if unordered_precinct_graph.has_edge((node, check_node)):
                continue
            check_coordinate_data = unordered_precinct_graph.node_attributes(check_node)[0].coords
            try:
                for point in check_coordinate_data.exterior.coords:
                    if min_x <= point[0] <= max_x:
                        if min_y <= point[1] <= max_y:
                            precincts_to_check.append(check_node)
                            break
            except:
                for geom in check_coordinate_data:
                    for point in geom.exterior.coords:
                        if min_x <= point[0] <= max_x:
                            if min_y <= point[1] <= max_y:
                                precincts_to_check.append(check_node)
                                break
        sys.stdout.write("\r                                ")
        sys.stdout.write(f"\rAdding edges progress: {round(100 * node/node_num, 2)}%")
        sys.stdout.flush()
        for precinct10 in precincts_to_check: 
            try:        
                if get_if_bordering(coordinate_data, unordered_precinct_graph.node_attributes(precinct10)[0].coords):
                    unordered_precinct_graph.add_edge((node, precinct10))
            except AdditionError:
                pass
            except:
                node_precinct_id = unordered_precinct_graph.node_attributes(node)[0].id
                precinct10_precinct_id = unordered_precinct_graph.node_attributes(precinct10)[0].id
                raise Exception(f'Failed intersection check, precincts being checked were {node_precinct_id}, {precinct10_precinct_id} of nodes {node} and {precinct10}, respectively')
        completed_precincts.append(node)

    print(time() - start_time)

    # The graph will have multiple representations, i.e. an "edge" going both ways, 
    # but it's only considered one edge in has_edge(), add_edge(), delete_edge(), etc.
    print('Edges: ', len(unordered_precinct_graph.edges())/2)

    print(time() - start_time)
    original_graph_components_num = len(get_components(unordered_precinct_graph))
    # Repeat until entire graph is contiguous
    # Get components of graph (islands of precincts)
    if not len(get_components(unordered_precinct_graph)) == 1:
        while len(get_components(unordered_precinct_graph)) != 1:
            graph_components = get_components(unordered_precinct_graph)
            # print(graph_components, '# of graph components')
            # Create list with dictionary containing keys as precincts, 
            # values as centroids for each component
            centroid_list = []
            for component in graph_components:
                component_list = {}
                for precinct in component:
                    component_list[precinct] = unordered_precinct_graph.node_attributes(precinct)[0].centroid
                centroid_list.append(component_list)
                
            # Keys are minimum distances to and from different islands, 
            # values are tuples of nodes to add edges between
            min_distances = {}
            # print(len(list(combinations([num for num in range(0, len(graph_components))], 2))), 'list of stuffs')
            # For nonrepeating combinations of components, add to list of edges
            for combo in list(combinations([num for num in range(0, (len(graph_components) - 1))], 2)):
                # Create list of centroids to compare
                centroids_1 = centroid_list[combo[0]]
                centroids_2 = centroid_list[combo[1]]
                min_distance = 0
                min_tuple = None
                for precinct_1, centroid_1 in centroids_1.items():
                    for precinct_2, centroid_2 in centroids_2.items():
                        x_distance = centroid_1[0] - centroid_2[0]
                        y_distance = centroid_1[1] - centroid_2[1]
                        # No need to sqrt unnecessarily
                        distance = (x_distance ** 2) + (y_distance ** 2)
                        if min_distance == 0:
                            min_distance = distance
                            min_tuple = (precinct_1, precinct_2)
                        elif distance < min_distance:
                            # print(precinct_1, precinct_2)
                            min_distance = distance
                            min_tuple = (precinct_1, precinct_2)
                min_distances[min_distance] = min_tuple
            # combinations() fails once the graph is one edge away from completion, so this is manual
            if len(graph_components) == 2:
                min_distance = 0
                for precinct_1 in graph_components[0]:
                    centroid_1 = unordered_precinct_graph.node_attributes(precinct_1)[0].centroid
                    for precinct_2 in graph_components[1]:
                        centroid_2 = unordered_precinct_graph.node_attributes(precinct_2)[0].centroid
                        x_distance = centroid_1[0] - centroid_2[0]
                        y_distance = centroid_1[1] - centroid_2[1]
                        # No need to sqrt unnecessarily
                        distance = (x_distance ** 2) + (y_distance ** 2)
                        if min_distance == 0:
                            min_distance = distance
                            min_tuple = (precinct_1, precinct_2)
                        elif distance < min_distance:
                            # print(precinct_1, precinct_2)
                            min_distance = distance
                            min_tuple = (precinct_1, precinct_2)
                min_distances[min_distance] = min_tuple

            # Find edge to add
            try:
                edge_to_add = min_distances[min(min_distances)]
            except ValueError:
                break
            print(f"\rConnecting islands progress: {100 - round(100 * len(graph_components)/original_graph_components_num, 2)}%")
            unordered_precinct_graph.add_edge(edge_to_add)
            print('yo', unordered_precinct_graph.node_attributes(edge_to_add[0])[0].id, unordered_precinct_graph.node_attributes(edge_to_add[1])[0].id)
    print(f'Number of islands (including mainland if applicable): {original_graph_components_num}')
    print('Edges after island linking: ', round(len(unordered_precinct_graph.edges())/2))

    # Create list of nodes in ascending order by degree
    ordered_nodes = sorted(
        unordered_precinct_graph.nodes(),
        key=lambda n: unordered_precinct_graph.node_attributes(n)[0].centroid[0]) 
    print(time() - start_time)
    # Create ordered graph
    ordered_precinct_graph = graph()
    # Add nodes from unordered graph to ordered
    for i, node in enumerate(ordered_nodes):
        ordered_precinct_graph.add_node(i, attrs=[unordered_precinct_graph.node_attributes(node)[0]])
    # Then, add EDGES.
    for node in ordered_nodes:
        neighbors = unordered_precinct_graph.neighbors(node)
        for neighbor in neighbors:
            if ordered_precinct_graph.has_edge((ordered_nodes.index(node), ordered_nodes.index(neighbor))):
                continue
            else:
                ordered_precinct_graph.add_edge((ordered_nodes.index(node), ordered_nodes.index(neighbor)))
    print("Total time taken:", time() - start_time)
    return ordered_precinct_graph


if __name__ == "__main__":
    ordered_precinct_graph = create_graph(*sys.argv[1:5])

    # Save graph as pickle
    with open(sys.argv[5], "wb+") as f:
        pickle.dump(ordered_precinct_graph, f)

    # Visualize graph
    visualize_graph(
        ordered_precinct_graph,
        f'./{sys.argv[4]}_graph.jpg',
        lambda n : ordered_precinct_graph.node_attributes(n)[0].centroid,
        # sizes=(lambda n : ordered_precinct_graph.node_attributes(n)[0].pop/500),
        show=True
    )
