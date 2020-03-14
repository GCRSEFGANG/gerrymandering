"""
One of the refinement processes in the communities algorithm.

Alters a set of communities such that their precincts all have a
standard deviation below a certain threshold.
"""

import sys
import pickle
import json
from time import time
from hacking_the_election.utils.community import Community
from hacking_the_election.utils.partisanship import get_bordering_precincts
from hacking_the_election.utils.stats import average, stdev
from hacking_the_election.utils.geometry import shapely_to_polygon, polygon_to_shapely, get_if_bordering, communities_to_json, clip
from hacking_the_election.utils.exceptions import CreatesMultiPolygonException
from hacking_the_election.serialization import save_precincts
from shapely.geometry import MultiPolygon
sys.modules['save_precincts'] = save_precincts

def modify_for_population(communities_list, precinct_corridors, total_population, threshold, iterations):
    '''
    Takes list of community objects, and returns a different list with the modified communities.,
    as well as the # of precincts that changed hands during this step. 
    total_population - total population in the state
    threshold - e.g. 5%, should be point at which, if all communities are below, it automatically stops
    iterations - should be number of iterations before hard stop, regardless of if below threshold or not
    At the end, modify_for_partisanship should return iteration of communities with minimum stdev.
    precinct_corridors should be a list of lists of two precincts, which are "connected", i.e.
    should be considered bordering. The threshold is a decimal for maximum community partisanship 
    standard deviation, i.e. (0.05)
    '''
    communities_dict = {community.id : community for community in communities_list}
    communities_precincts = {community.id : community.precincts for community in communities_list}
    # create dictionary of ids and community partisanship standard deviations
    community_stdev = {community.id : community.standard_deviation for community in communities_list}
    # find out what the goal for population should be
    ideal_population = total_population/len(communities_list)
    # check if any communities are above the threshold
    # count the number of times the list has been checked
    count = 0
    num_of_changed_precincts = []
    success = 'no'
    communities_at_stages = {}
    for community in communities_list:
        community.update_population()
    # average stdev tracks the average_standard_deviation across all communities throughout iterations
    population_stdev = [stdev([community20.population for community20 in communities_list])]
    # populations will store comma seperated standard deviations for communities, with rows 
    # being iterations
    populations = []
    while success != "yes!":
        if count >= iterations:
            break
        # update attribute values (in case this hasn't already been done)
        for community in communities_list:
            community.update_standard_deviation()
            community.update_coords()
            community.update_partisanship()
            community.update_population()
            print(community.id, community.standard_deviation)
        print(time())
        count += 1

        # create number_of_changed_iteration variable to store # of precincts changed this iteration
        num_of_changed_iteration = 0
        # find the community that is furthest away from the ideal population
        worst_population = {}
        for community5 in communities_list:
            if len(worst_population) == 0: 
                worst_population[community5.id] = community5

            elif abs(community5.population - ideal_population) * 100/ideal_population > list(worst_population.keys())[0]:
                del worst_population[list(worst_population.keys())[0]]
                worst_population[abs(community5.population - ideal_population) * 100/ideal_population] = community5
        worst_population_community = list(worst_population.values())[0] 
        worst_population_id = worst_population_community.id
        if abs(worst_population_community.population - ideal_population) * 100/ideal_population < threshold:
            success = 'yes!'
            break  
        if len(num_of_changed_precincts) > 0:     
            if num_of_changed_precincts[-1] == 0:
                success = 'yes!'
                break
        # if all the code in this while loop below runs, then there must be a community
        # with greater than average standard deviation
        biggest_community_precincts = communities_precincts[worst_population_id]
        # relates border precinct to community border it's from (not including worst_population_community)
        border_precincts = {worst_population_community.id: []}
        other_communities_dict = {community9: [] for community9 in communities_list}

        # create dictionary of the precincts bordering it
        for id1, community1 in communities_dict.items():
            # if community is biggest one, skip
            if id1 == worst_population_id:
                continue
            print(worst_population_id, community1.id)
            if get_if_bordering(worst_population_community.coords, 
                                community1.coords):
                # the following result has first key: precincts ids inside most stdev community
                # second key: precinct ids in other community
                specific_border_precincts = get_bordering_precincts(worst_population_community, community1)
                for precinct in specific_border_precincts[worst_population_id]:
                    border_precincts[worst_population_id].append(precinct)
                    other_communities_dict[communities_dict[id1]].append(precinct)
                if len(specific_border_precincts[id1]) != 0: 
                    for precinct5 in specific_border_precincts[id1]:
                        other_communities_dict[worst_population_community].append(precinct5)
                    try:
                        border_precincts[id1].extend(specific_border_precincts[id1])
                    except KeyError:
                        border_precincts[id1] = specific_border_precincts[id1]
        # add precinct corridors that have precincts inside community, but only one
        for precinct6 in worst_population_community.precincts:
            for pair in precinct_corridors:
                if precinct6 in pair:
                    # if both sides of a precinct corridor are in the community, 
                    # no need to be added
                    if pair[:].remove(precinct6)[0] in community.precincts:
                        continue
                    else:
                        for community6 in communities_list:
                            if precinct in community6.precincts.values():
                                border_precincts[community6] = pair[:].remove(precinct6)
                                other_communities_dict[worst_population_community].append(precinct6)
        # remove duplicate border precincts from list to consider
        no_duplicate_list = []
        for id3, precinct_list in border_precincts.items():
            no_duplicate_precinct_list = []
            for precinct7 in precinct_list:
                if precinct7 not in no_duplicate_list:
                    no_duplicate_list.append(precinct7)
                    no_duplicate_precinct_list.append(precinct7)
            border_precincts[id3] = no_duplicate_precinct_list
        border_precincts_list = []
        for precinct_list1 in border_precincts.values():
            for precinct1 in precinct_list1:
                border_precincts_list.append(precinct1)
        z = '../../../../border' + str(count) + '.json'
        with open(z, 'w') as f:
            try:
                _ = shapely_to_polygon(clip([precinct8.coords for precinct8 in border_precincts_list], 1))[0][0][0][0]
                y = json.dumps({"type":"FeatureCollection", "features":[{"type": "Feature", "geometry": {"type":"MultiPolygon", "coordinates":shapely_to_polygon(clip([precinct.coords for precinct in border_precincts_list], 1)), "properties":{"ID":"border"}}}]})
            except:
                y = json.dumps({"type":"FeatureCollection", "features":[{"type": "Feature", "geometry": {"type":"Polygon", "coordinates":shapely_to_polygon(clip([precinct.coords for precinct in border_precincts_list], 1)), "properties":{"ID":"border"}}}]})
            f.write(y)
            print('hello, border json ready')


        border_communities = {(community.population - ideal_population)/ideal_population : community for community in communities_list if not community == worst_population_community 
                              if get_if_bordering(community.coords, worst_population_community.coords) == True}
        if len(border_communities) == 0:
            raise Exception('no communities bordering?')
        precinct_count = 0
        while (worst_population_community.population - ideal_population)* 100* 100/ideal_population > threshold:
            print(worst_population_id, abs(worst_population_community.population - ideal_population)* 100/ideal_population, threshold)
            # if there is only one precinct left, just stop
            if len(worst_population_community.precincts) == 1:
                break
            precinct_exchanges_dict = {}
            # find the community that's best for exchanging precincts with that borders the worst_pop community
            # i.e. if the worst_population_community has too many people, find the one bordering it with the least
            if worst_population_community.population - ideal_population > 0:
                best_community_to_exchange = min(border_communities)
                best_community = border_communities[best_community_to_exchange]
                print('best_community', best_community.id)
                # we need to reduce population, so we find precincts to give away
                bordering_worst_pop_precincts = other_communities_dict[best_community]
                for precinct30 in bordering_worst_pop_precincts:
                    if isinstance(clip([worst_population_community.coords, precinct30.coords], 2), MultiPolygon):
                        continue
                    modified_population = worst_population_community.population - precinct30.population
                    # removing this precinct pushes the population too far in the other direction
                    if (modified_population - ideal_population) * 100 /ideal_population < -1:
                        continue
                    else:
                        precinct_exchanges_dict[(abs(worst_population_community.population - ideal_population) * 100 /ideal_population) -
                                                (abs(modified_population - ideal_population) * 100 /ideal_population)] = precinct30
            else:
                best_community_to_exchange = max(border_communities)
                best_community = border_communities[best_community_to_exchange]
                print('best_community', best_community.id)
                # we need to increase population, so we find precincts to add
                bordering_worst_pop_precincts = other_communities_dict[worst_population_community]
                for precinct31 in bordering_worst_pop_precincts:
                    if isinstance(clip([best_community.coords, precinct31.coords], 2), MultiPolygon):
                        continue
                    modified_population = worst_population_community.population + precinct31.population
                    # if adding a precinct swings it too far
                    if (modified_population - ideal_population) * 100 /ideal_population > 1:
                        continue
                    else:
                        precinct_exchanges_dict[(abs(worst_population_community.population - ideal_population) * 100 /ideal_population) -
                                                (abs(modified_population - ideal_population) * 100 /ideal_population)] = precinct31
            print(len(precinct_exchanges_dict))
            best_precinct_exchange = max(precinct_exchanges_dict)
            best_precinct = precinct_exchanges_dict[best_precinct_exchange]
            # actually exchange the precinct
            if best_precinct in worst_population_community.precincts.values():
                worst_population_community.give_precinct(best_community, best_precinct.vote_id)
            else:
                best_community.give_precinct(worst_population_community, best_precinct.vote_id)
        # # find which precinct exchanges are the best
        # precinct_exchanges_dict = {}
        # # for border precincts within the highest stdev community, find stdev without that precinct
        # community_population_stat = worst_population_community.population
        # for num, precinct3 in enumerate(border_precincts[worst_population_community.id]):
        #     # only these two communities need to be considered because others aren't affected
        #     for community21, precinct_list13 in other_communities_dict.items():
        #         if precinct3 in precinct_list13:
        #             best_community = community21
        #     other_community_population_stat = best_community.population
        #     other_precinct_list = list(worst_population_community.precincts.values())[:]
        #     del other_precinct_list[num]
        #     added_precinct_list = list(best_community.precincts.values())[:]
        #     added_precinct_list.append(precinct3)
        #     other_modification_stdev = stdev([(precinct12.r_election_sum * 100) / (precinct12.r_election_sum + precinct12.d_election_sum)
        #                                         for precinct12 in added_precinct_list
        #                                         if (precinct12.r_election_sum + precinct12.d_election_sum) != 0])
        #     modification_stdev = stdev([(precinct3.r_election_sum * 100)/(precinct3.r_election_sum + precinct3.d_election_sum) 
        #                             for precinct3 in other_precinct_list
        #                             if (precinct3.r_election_sum + precinct3.d_election_sum) != 0])
        #     average_stdev1 = average([community_stdev_stat,other_community_stdev_stat])
        #     modified_average_stdev = average([modification_stdev, other_modification_stdev])
        #     precinct_exchanges_dict[(average_stdev1 - modified_average_stdev)] = precinct3 
        # print(len(precinct_exchanges_dict.keys()))
        # # for border precincts outside the highest stdev community, find stdev with that precinct
        # for key in list(border_precincts.keys())[1:]:
        #     for num, precinct4 in enumerate(border_precincts[key]):
        #         # check to make sure removing community from worst_population_community
        #         # does not lead to non-contiguous communities
        #         # find community this precinct in precinct_list is from
        #         for community2 in communities_list:
        #             if precinct4 in community2.precincts.values():
        #                 best_community = community2
        #         other_community_stdev_stat = best_community.standard_deviation

        #         other_precinct_list = list(best_community.precincts.values())
        #         del other_precinct_list[num]
        #         added_precinct_list = list(worst_population_community.precincts.values())[:]
        #         added_precinct_list.append(precinct4)
        #         modification_stdev = stdev([(precinct4.r_election_sum * 100)/(precinct4.r_election_sum + precinct4.d_election_sum) 
        #                                      for precinct4 in added_precinct_list
        #                                      if (precinct4.r_election_sum + precinct4.d_election_sum) != 0])
        #         other_modification_stdev = stdev([(precinct13.r_election_sum * 100)/(precinct13.r_election_sum + precinct13.d_election_sum)
        #                                            for precinct13 in other_precinct_list
        #                                            if (precinct13.r_election_sum + precinct13.d_election_sum) != 0])
        #         average_stdev1 = average([other_community_stdev_stat, community_stdev_stat])
        #         modification_average_stdev = average([modification_stdev, other_modification_stdev])
        #         precinct_exchanges_dict[(average_stdev1 - modification_average_stdev)] = precinct4
        # print('before duplicate removal, ', len(precinct_exchanges_dict.keys()))
        # to_remove = []
        # for sta_dev in precinct_exchanges_dict.keys():
        #     removed = list(precinct_exchanges_dict.keys())[:]
        #     removed.remove(sta_dev)
        #     if sta_dev in removed:
        #         to_remove.append(sta_dev)
        # for sta_dev1 in to_remove:
        #     del precinct_exchanges_dict[sta_dev1]
        # print(len(precinct_exchanges_dict.keys()))
        # add or remove precincts from border_precincts until there are no more beneficial exchanges,
        # or until the community's standard deviation is below the threshold
        # precinct_count = 1
        # changing_stdev = []
        # while worst_population_community.standard_deviation > threshold:
        #     print(worst_population_id, worst_population_community.standard_deviation, threshold)
        #     # if there is only one precinct left, just stop
        #     if len(worst_population_community.precincts) == 1:
        #         break
        #     try:
        #         highest_precinct_exchange = max(precinct_exchanges_dict.keys())
        #         high_precinct = precinct_exchanges_dict[highest_precinct_exchange]
        #     # no more precinct exchanges left
        #     except ValueError:
        #         break
        #     # if there are no beneficial precinct exchanges left, stop
        #     if highest_precinct_exchange <= 0:
        #         break
        #     for community16, precinct_list10 in other_communities_dict.items():
        #         if high_precinct in precinct_list10:
        #             if community16.id == worst_population_community.id:
        #                 for community19 in communities_list:
        #                     if high_precinct in community19.precincts.values():
        #                         best_community = community19
        #             else:
        #                 best_community = community16
        #     print('other community, ', best_community.id, 'highest stdev community, ', worst_population_id)
        #     # find precincts that can no longer be used now once a precinct has changed hands
        #     no_longer_applicable_precincts = []
        #     # if precinct is in biggest stdev community:
        #     if high_precinct in worst_population_community.precincts.values():
        #         print(worst_population_id, 'chosen')
        #         # give precinct from worst_population to other community
        #         # double check for contiguity
        #         if isinstance(clip([worst_population_community.coords, high_precinct.coords], 2), MultiPolygon):
        #             del precinct_exchanges_dict[highest_precinct_exchange]
        #             continue
        #         if isinstance(clip([best_community.coords, high_precinct.coords], 1), MultiPolygon):
        #             del precinct_exchanges_dict[highest_precinct_exchange]
        #             continue
        #         try:
        #             worst_population_community.give_precinct(best_community, high_precinct.vote_id)
        #         except CreatesMultiPolygonException:
        #             del precinct_exchanges_dict[highest_precinct_exchange]
        #             continue
        #         # update number of precincts changed on this iteration
        #         num_of_changed_iteration += 1
        #         del precinct_exchanges_dict[highest_precinct_exchange]
        #         for precinct_list3 in border_precincts.values():
        #             for precinct2 in precinct_list3:
        #                 try:
        #                     if not get_if_bordering(polygon_to_shapely(precinct2.coords), worst_population_community.coords):
        #                         no_longer_applicable_precincts.append(precinct2)
        #                 except AttributeError:
        #                     # precinct has no coordinates, which happens sometimes for some reason
        #                     pass
        #     # precinct is not in biggest stdev community
        #     elif high_precinct in best_community.precincts.values():
        #         print(best_community.id, 'chosen')
        #         if isinstance(clip([best_community.coords, high_precinct.coords], 2), MultiPolygon):
        #             del precinct_exchanges_dict[highest_precinct_exchange]
        #             continue
        #         if isinstance(clip([worst_population_community.coords, high_precinct.coords], 1), MultiPolygon):
        #             del precinct_exchanges_dict[highest_precinct_exchange]
        #             continue
        #         try:
        #             best_community.give_precinct(worst_population_community, high_precinct.vote_id)
        #         except CreatesMultiPolygonException:
        #             del precinct_exchanges_dict[highest_precinct_exchange]
        #             continue
        #         # update number of precincts changed on this iteration
        #         num_of_changed_iteration += 1
        #         del precinct_exchanges_dict[highest_precinct_exchange]
        #         for precinct_list5 in border_precincts.values():
        #             for precinct9 in precinct_list5:
        #                 try:
        #                     if not get_if_bordering(precinct9.coords, best_community.coords):
        #                         no_longer_applicable_precincts.append(precinct9)
        #                 except AttributeError:
        #                     # precinct has no coordinates, which happens sometimes for some reason
        #                     pass
        #     else:
        #         print(high_precinct.vote_id, high_precinct)
        #     # removes precincts that can't be added/removed now that a precinct has been added
        #     for id2, precinct_list6 in border_precincts.items():
        #         precinct_list_to_remove = []
        #         for precinct10 in precinct_list6:
        #             if precinct10 not in no_longer_applicable_precincts:
        #                 precinct_list_to_remove.append(precinct10)
        #         border_precincts[id2] = precinct_list_to_remove
        #     print(len(precinct_exchanges_dict))
        #     print(len(precinct_exchanges_dict))
        #     to_replace = {}
        #     community_stdev_stat = worst_population_community.standard_deviation
        #     other_community_stdev_stat = best_community.standard_deviation
        #     for sta_dev2, precinct18 in enumerate(precinct_exchanges_dict.values()):
        #         if precinct18 in worst_population_community.precincts.values():
        #             other_precinct_list = list(worst_population_community.precincts.values())[:]
        #             other_precinct_list.remove(precinct18)
        #             added_precinct_list = list(best_community.precincts.values())[:]
        #             added_precinct_list.append(precinct18)
        #             other_modification_stdev = stdev([(precinct19.r_election_sum * 100) / (precinct19.r_election_sum + precinct19.d_election_sum)
        #                                                 for precinct19 in added_precinct_list
        #                                                 if (precinct19.r_election_sum + precinct19.d_election_sum) != 0])
        #             modification_stdev = stdev([(precinct21.r_election_sum * 100)/(precinct21.r_election_sum + precinct21.d_election_sum) 
        #                                     for precinct21 in other_precinct_list
        #                                     if (precinct21.r_election_sum + precinct21.d_election_sum) != 0])
        #             average_stdev1 = average([community_stdev_stat,other_community_stdev_stat])
        #             modified_average_stdev = average([modification_stdev, other_modification_stdev])
        #             to_replace[(average_stdev1 - modified_average_stdev)] = precinct18
        #         else:
        #             for community26, precinct_list12 in other_communities_dict.items():
        #                 if precinct18 in precinct_list12:
        #                     if community26.id == worst_population_community.id:
        #                         for community27 in communities_list:
        #                             if precinct18 in community27.precincts.values():
        #                                 other_community1 = community27
        #                     else:
        #                         other_community1 = community16
        #         # for border precincts outside the highest stdev community, find stdev with that precinct
        #             # check to make sure removing community from worst_population_community
        #             # does not lead to non-contiguous communities
        #             other_precinct_list = list(other_community1.precincts.values())
        #             other_precinct_list.remove(precinct18)
        #             added_precinct_list = list(worst_population_community.precincts.values())[:]
        #             added_precinct_list.append(precinct18)
        #             modification_stdev = stdev([(precinct23.r_election_sum * 100)/(precinct23.r_election_sum + precinct23.d_election_sum) 
        #                                         for precinct23 in added_precinct_list
        #                                         if (precinct23.r_election_sum + precinct23.d_election_sum) != 0])
        #             other_modification_stdev = stdev([(precinct24.r_election_sum * 100)/(precinct24.r_election_sum + precinct24.d_election_sum)
        #                                             for precinct24 in other_precinct_list
        #                                             if (precinct24.r_election_sum + precinct24.d_election_sum) != 0])
        #             average_stdev1 = average([other_community_stdev_stat, community_stdev_stat])
        #             modification_average_stdev = average([modification_stdev, other_modification_stdev])
        #             to_replace[(average_stdev1 - modification_average_stdev)] = precinct18
        #     precinct_exchanges_dict = to_replace
            print(len(precinct_exchanges_dict))

            # not_involved_community_list = []
            # for community13 in not_involved_community_list:
            #     if community13 != best_community:
            #         if community13 != worst_population_community:
            #             not_involved_community_list.append(community13)
            # community_change_snapshot = [best_community, worst_population_community]
            # for community14 in not_involved_community_list:
            #     community_change_snapshot.append(community14)
            # communities_to_json(community_change_snapshot, str('../../../../partisanship_after' + str(precinct_count) + 'community'  + '.json'))
            print('HELLO', precinct_count)
            precinct_count += 1
            # changing_stdev.append(str(average([worst_population_community.standard_deviation, best_community.standard_deviation])) + '\n')
        print(stuff)
        population_stat = str([community10.population for community10 in communities_list])[1:-1]
        populations.append(population_stat)
        for community11 in communities_list:
            print(abs(community11.population - ideal_population) * 100 / ideal_population)
        # save number_of_changed_precincts_this_iteration to running list
        num_of_changed_precincts.append(precinct_count)
        # add to population_stdev
        population_stdev.append(average([community.standard_deviation for community in communities_list]))
        # create new communities list
        not_involved_community_list = []
        # find not involved community
        for community12 in communities_list:
            if community12 != best_community:
                if community12 != worst_population_community:
                    not_involved_community_list.append(community12)
        community_change_snapshot = [best_community, worst_population_community]
        for community15 in not_involved_community_list:
            community_change_snapshot.append(community15)
        # communities_at_stages[average([community.standard_deviation for community in communities_list])] = community_change_snapshot
        communities_to_json(community_change_snapshot, str('../../../../partisanship_after boi' + str(count) + '.json'))
        with open('../../../../population_average_stdevs.txt', 'w') as f:
            f.write(str(population_stdev))
        with open('../../../../population_precincts_changed.txt', 'w') as f:
            f.write(str(num_of_changed_precincts))   
    # find iteration with smallest population_stdev
    # minimized = min(communities_at_stages)
    # minimized_communities = communities_at_stages[minimized] 
    communities_to_json(communities_list, '../../../../population_after.json')
    print('completed!')
    return (communities_list, precinct_corridors), count, populations, num_of_changed_precincts, population_stdev
    
# just for testing, will delete later
with open('../../../../vermont_initial_configuration_new.pickle', 'rb') as f:
    x = pickle.load(f)
communities_to_json(x[0], '../../../../new_test_communities.json')
b, count1, standard_deviations1, num_of_changed_precincts1, average_stdev1 = modify_for_population(x[0], x[1], 625741, 5, 50)


print('# of iterations:', count1)
print('standard deviations:', standard_deviations1)
print('# of changed precincts per iteration:', num_of_changed_precincts1)
print('average standard deviation across iterations:', average_stdev1)


with open('../../../../end_of_partisanship.pickle', 'wb') as f:
    pickle.dump(b, f)