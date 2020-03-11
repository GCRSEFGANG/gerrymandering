/*===============================================
 community.cpp:                        k-vernooy
 last modified:                     Fri, Feb 28

 Definition of the community-generation algorithm for
 quantifying gerrymandering and redistricting. This
 algorithm is the main result of the project
 hacking-the-election, and detailed techincal
 documents can be found in the /docs root folder of
 this repository.

 Visit https://hacking-the-election.github.io for
 more information on this project.
 
 Useful links for explanations, visualizations,
 results, and other information:

 Our data sources for actually running this
 algorithm can be seen at /data/ or on GitHub:
 https://github.com/hacking-the-election/data

 This algorithm has been implemented in C++ and in
 Python. If you're interested in contributing to the
 open source political movement, please contact us
 at: hacking.the.election@gmail.com
===============================================*/

#include "../../../include/shape.hpp"    // class definitions
#include "../../../include/util.hpp"     // array modification functions
#include "../../../include/geometry.hpp" // geometry modification, border functions
#include "../../../include/canvas.hpp" // geometry modification, border functions

#include <math.h>    // for rounding functions
#include <numeric>   // include std::iota
#include <algorithm> // sorting, seeking algorithms
#include <boost/filesystem.hpp>

using namespace std;
using namespace GeoGerry;
using namespace GeoDraw;
using namespace boost::filesystem;

#define VERBOSE 1
#define WRITE 0
#define DEBUG_COMMUNITIES 1

/*
    Define constants to be used in the algorithm.    
    These will not be passed to the algorithm
    as arguments, as they define things like stop
    conditions.
*/

const int CHANGED_PRECINT_TOLERANCE = 10; // percent of precincts that can change from iteration
const int MAX_ITERATIONS = 5; // max number of times we can change a community

// ids for processes:
const int PARTISANSHIP = 0;
const int COMPACTNESS = 1;
const int POPULATION = 2;

void State::generate_initial_communities(int num_communities) {
    /*
        @desc:
            Updates states communities with initial random community configuration.
            Reserves precincts on islands to prevent bad island linking.
            Fractional islands are defined here as those that do not fit
            an even combination of communities, and have leftover precincts that
            must be added to a separate community.
    
        @params: `int` num_communities: the number of initial communities to generate
        @return: `void`
    */

    int num_precincts = precincts.size(); // total precinct amount

    vector<int> large_sizes; // number of communities of greater amount
    vector<int> base_sizes;

    int base = floor(num_precincts / num_communities); // the base num
    int rem = num_precincts % num_communities; // how many need to be increased by 1

    for (int i = 0; i < num_communities - rem; i++) base_sizes.push_back(base);
    for (int i = 0; i < rem; i++) large_sizes.push_back(base + 1);
    
    vector<p_index_set> available_precincts = islands;      // precincts that have yet to be used up
    p_index_set fractional_islands;                         // indices of islands that can't be made of base and large
    Communities c;                                          // Set of communities

    int island_index = 0;

    for (p_index_set island : islands) {
        /*
            Determine the amount of whole communities can
            be fit on each island. If an island contains fractional
            communities, add it to
        */

        cout << "island 1" << endl;
        map<int, array<int, 2> > vals; // to hold possible size combinations

        for (int x = 1; x <= large_sizes.size(); x++) {
            for (int y = 1; y <= base_sizes.size(); y++) {
                vals[(x * (base + 1)) + (y * base)] = {x, y};
            }
        }

        int x = 0; // number of base communities on island
        int y = 0; // number of base + 1 communities on island

        auto it = vals.find(island.size());
        
        if (it != vals.end()) {
            // this island can be made from whole communities
            x = vals[island.size()][1];
            y = vals[island.size()][0];
        }
        else {
            // this island must contain a fractional community
            fractional_islands.push_back(island_index);

            // find the number of whole communities it can contain
            // regardless, by rounding down to the nearest array element
            int round;
            
            for (auto it = vals.begin(); it != vals.end(); it++) {
                if (it->first > island.size()) break;
                round = it->first;
            }
            
            x = vals[round][1];
            y = vals[round][0];
        }

        for (int i = 0; i < x; i++) {
            base_sizes.pop_back();
            Community com;
            com.size.push_back(base);
            com.location.push_back(island_index);
            c.push_back(com);
            if (DEBUG_COMMUNITIES) cout << "full community of " << base << endl;
        }
        for (int j = 0; j < y; j++) {
            large_sizes.pop_back();
            Community com;
            com.size.push_back(base + 1);
            com.location.push_back(island_index);
            c.push_back(com);
            if (DEBUG_COMMUNITIES) cout << "full community of " << base + 1 << endl;
        }

        island_index++;
    }

    vector<p_index> ignore_fractionals; // fractional islands to be ignored, not removed

    for (p_index fractional_island_i = 0; fractional_island_i < fractional_islands.size(); fractional_island_i++) {
        /*
            Loop through all ƒractional islands - those that need precincts
            from other islands to create communities - and create community
            objects with links

            Have fun trying to understand this code tomorrow
        */

        if (VERBOSE) cout << "linking fractional communities... " << fractional_islands.size() << endl;
        int n_fractional_island_i = fractional_island_i;

        if (!(std::find(ignore_fractionals.begin(), ignore_fractionals.end(), fractional_islands[fractional_island_i]) != ignore_fractionals.end())) {
            // create community with location information
            Community community;
            community.location.push_back(fractional_islands[fractional_island_i]);
            p_index island_i = fractional_islands[fractional_island_i];
            p_index_set island = islands[island_i];

            // get average center of island from precinct centers
            array<long long int, 2> island_center = {0,0};

            for (p_index p : island) {
                coordinate p_center = precincts[p].get_center();
                island_center[0] += p_center[0];
                island_center[1] += p_center[1];
            }

            island_center[0] = island_center[0] / island.size();
            island_center[1] = island_center[1] / island.size();

            int island_leftover = islands[fractional_islands[fractional_island_i]].size();

            for (Community community_c : c) {
                // subtract precincts that are taken up by communities already
                auto it = find(community_c.location.begin(), community_c.location.end(), fractional_islands[fractional_island_i]);
                if (it != community_c.location.end())
                    island_leftover -= community_c.size[std::distance(community_c.location.begin(), it)]; // subtract whole communitites that are already added
            }

            community.size.push_back(island_leftover); // island leftover now contains # of available precincts
            int total_community_size; // the ultimate amount of precincts in the community

            // size the current community correctly
            if (large_sizes.size() > 0) {
                // there are still large sizes available
                total_community_size = base + 1;
                large_sizes.pop_back();
            }
            else {
                // there are only base sizes available
                total_community_size = base;
                base_sizes.pop_back();
            }

            if (DEBUG_COMMUNITIES) cout << "need to make community of " << total_community_size << endl;
            // total_leftover contains the amount of precincts still needed to be added
            int total_leftover = total_community_size - island_leftover;

            while (total_leftover > 0) {
                if (DEBUG_COMMUNITIES) cout << "need community to link with " << n_fractional_island_i << endl;
                // find the closest fractional island that can be linked
                double min_distance = pow(10, 80); // arbitrarily high number (easy min)
                p_index min_index = -1;
                array<long long int, 2> min_island_center = {0,0};

                for (int compare_island = 0; compare_island < fractional_islands.size(); compare_island++) {
                    // find closest fractional community to link
                    if (compare_island != n_fractional_island_i && 
                        !(std::find(ignore_fractionals.begin(), ignore_fractionals.end(), fractional_islands[compare_island]) != ignore_fractionals.end())) {
                        // get average center of island from precinct centers
                        p_index_set island_c = islands[fractional_islands[compare_island]];
                        array<long long int, 2> island_center_c = {0,0};  

                        for (p_index p : island_c) {
                            coordinate p_center = precincts[p].get_center();
                            island_center_c[0] += p_center[0];
                            island_center_c[1] += p_center[1];
                        }

                        // average coordinates
                        island_center_c[0] /= island_c.size();
                        island_center_c[1] /= island_c.size();

                        // get distance to current island
                        double dist = get_distance(island_center, island_center_c);
                        // if this is the lowest distance, then update values
                        if (dist < min_distance) {
                            min_distance = dist;
                            min_index = compare_island;
                            min_island_center = island_center_c;
                        }
                    }
                }

                int island_leftover_c = islands[fractional_islands[min_index]].size(); // size of island
                for (Community community_c : c) {
                    auto it = find(community_c.location.begin(), community_c.location.end(), fractional_islands[min_index]);
                    if (it != community_c.location.end())
                        island_leftover_c -= community_c.size[std::distance(community_c.location.begin(), it)]; // subtract sizes of communities on island
                }

                // island_leftover_c contains amount of available precincts on linking island
                if (DEBUG_COMMUNITIES) cout << "closeset community to link to is " << min_index << endl;
                p_index link, min_link;
                double min_p_distance = pow(10, 80); // arbitrarily high number (easy min)
                int i = 0;

                // find the precinct closest to the center of the island
                if (fractional_island_i == n_fractional_island_i) {
                    p_index_set ignore_p;
                    cout << "generating first link" << endl;
                    do {
                        for (p_index p : get_inner_boundary_precincts(islands[fractional_islands[min_index]], *this)) {
                        // for (p_index p : islands[fractional_islands[min_index]]) {
                            array<long long int, 2> p_center = {(long long int) precincts[p].get_center()[0], (long long int) precincts[p].get_center()[1]};
                            double dist = get_distance(p_center, island_center);

                            if (dist < min_p_distance && (std::find(ignore_p.begin(), ignore_p.end(), p) == ignore_p.end())) {
                                min_p_distance = dist;
                                link = i;
                            }
                            i++;
                        }

                        ignore_p.push_back(link);

                    } while (creates_island(islands[fractional_islands[min_index]], link, *this));
                }
                else {
                    link = community.link_position[community.link_position.size() - 1][1][1];
                }

                p_index_set ignore_p = {};
                min_p_distance = pow(10, 80);
                i = 0;

                do {
                    for (p_index p : get_inner_boundary_precincts(islands[fractional_islands[n_fractional_island_i]], *this)) {
                        array<long long int, 2> p_center = {(long long int) precincts[p].get_center()[0], (long long int) precincts[p].get_center()[1]};
                        double distc = get_distance(p_center, min_island_center);

                        if (distc < min_p_distance && (std::find(ignore_p.begin(), ignore_p.end(), p) == ignore_p.end())) {
                            min_p_distance = distc;
                            min_link = i;
                        }
                        i++;
                    }

                    ignore_p.push_back(link);

                } while (creates_island(islands[fractional_islands[n_fractional_island_i]], min_link, *this));


                if (DEBUG_COMMUNITIES) cout << "linking precinct " << link << " on " << n_fractional_island_i << " with " << min_link << " on " << min_index << endl;
                // cout << available_precincts.size() << endl;
                // cout << available_precincts[n_fractional_island_i].size() << endl;

                if (available_precincts[n_fractional_island_i].size() > link)
                    available_precincts[n_fractional_island_i].erase(available_precincts[n_fractional_island_i].begin() + link);
                if (available_precincts[min_index].size() > min_link)
                    available_precincts[min_index].erase(available_precincts[min_index].begin() + min_link);

                // set community meta information
                community.link_position.push_back({{n_fractional_island_i, link}, {min_index, min_link}});
                community.is_linked = true;
                community.location.push_back(fractional_islands[min_index]);

                
                if (total_leftover - island_leftover_c < 0) {
                    community.size.push_back(total_leftover);  
                }
                else {
                    community.size.push_back(island_leftover_c);
                    if (DEBUG_COMMUNITIES) cout << "Adding " << island_leftover_c << " precincts to community" << endl;
                }

                total_leftover -= island_leftover_c;

                // if used up a whole island, still have precincts you need to get from somewhere
                if (total_leftover >= 0) {
                    ignore_fractionals.push_back(fractional_islands[min_index]);
                    if (DEBUG_COMMUNITIES) cout << total_leftover << " precincts left" << endl;
                }

                ignore_fractionals.push_back(fractional_islands[n_fractional_island_i]);

                n_fractional_island_i = min_index;
                island_center = min_island_center;
            }

            int sum = 0;
            for (int s : community.size)
                sum += s;

            if (DEBUG_COMMUNITIES) cout << endl << endl;
            c.push_back(community);
        }

    }  // fractional linker

    if (VERBOSE) cout << "filling communities with real precincts..." << endl;

    for (int c_index = 0; c_index < c.size() - 1; c_index++) {
    // for (int c_index = 0; c_index < 1; c_index++) {
        // fill linked communities with generation method
        cout << "filling new community..." << endl;
        Community community = c[c_index];
        for (int i = 0; i < community.location.size(); i++) {
            cout << "on size " << community.size[i] << endl;
            // get information about the current community
            int size  = community.size[i];
            int island_i = community.location[i];
            p_index_set island_available_precincts = available_precincts[i]; 

            int start_precinct;
            
            if (community.link_position.size() > 0)
                // need to start on the linked precinct
                start_precinct = community.link_position[i][0][1];
            else {
                vector<p_index> tried_precincts;
                do {
                    start_precinct = rand_num(0, size - 1);
                } while (
                        creates_island(island_available_precincts, start_precinct, *this) 
                        && std::find(island_available_precincts.begin(), island_available_precincts.end(), start_precinct) == island_available_precincts.end()
                    );
            }

            cout << "adding precinct " << start_precinct << " to community " << c_index << endl;
            community.add_precinct(precincts[start_precinct]);

            island_available_precincts.erase(
                    std::remove(
                        island_available_precincts.begin(),
                        island_available_precincts.end(),
                        start_precinct
                    ),
                    island_available_precincts.end()
                );


            int precincts_to_add = size;
            int precincts_added = 0;

            while (precincts_added < precincts_to_add) {
                int precinct = -1;
                p_index_set tried_precincts; // precincts that will create islands
                p_index start; // random precinct in border thats not linked

                // calculate border, avoid multipoly
                cout << "calculating bordering precincts..." << endl;
                p_index_set bordering_precincts = get_ext_bordering_precincts(community, island_available_precincts, *this);
                for (p_index pre : bordering_precincts) cout << pre << ", ";
                cout << endl;

                bool can_do_one = false;

                for (p_index pre : bordering_precincts) {
                    if (!creates_island(island_available_precincts, pre, *this) && precincts_added < precincts_to_add) {
                        can_do_one = true;
                        cout << "adding precinct " << pre << endl;
                        island_available_precincts.erase(
                                std::remove(
                                    island_available_precincts.begin(),
                                    island_available_precincts.end(),
                                    pre
                                ),
                                island_available_precincts.end()
                            );

                        community.add_precinct(precincts[pre]);
                        precincts_added++;
                    }
                    else cout << "creates island, refraining..." << endl;
                }

                if (!can_do_one) cout << "No precinct exchanges work!!" << endl;
            }
            available_precincts[i] = island_available_precincts; 
        }
        c[c_index] = community;
    }

    for (p_index_set p : available_precincts)
        for (p_index pi : p )
            c[c.size() - 1].add_precinct(precincts[pi]);

    this->state_communities = c; // assign state communities to generated array
    save_communities("community_vt", this->state_communities);
    return;
}

p_index State::get_next_community(double tolerance, int process) {
    /*
        @desc:
            gets next candidate community depending on which process
            the algorithm is currently running. Used in algorithm to determine
            next community to optimize.
    
        @params:
            `double` tolerance: the tolerance for any process
            `int` process: the id of the current running process. These are
                           defined in the top of this file as constants

        @return: `p_index` community to modify next
    */

    p_index i = -1;

    if (process == PARTISANSHIP) {
        /*
            Find community with standard deviation of partisanship
            ratios that are most outside range of tolerance
        */

        double max = 0;
        p_index x = 0;

        for (Community c : state_communities) {
            double stdev = get_standard_deviation_partisanship(c);
            if (stdev > tolerance && stdev > max) {
                i = x;
                max = stdev;
            }
            x++;
        }
    }
    else if (process == COMPACTNESS) {
        unit_interval min = 1;
        p_index x = 0;

        for (Community c : state_communities) {
            unit_interval t_compactness = c.get_compactness();
            if (t_compactness < min && t_compactness < tolerance) {
                min = t_compactness;
                i = x;
            }
            x++;
        }

    }
    else if (process == POPULATION) {
        /*
            Find community that is farthest away from the
            average of (total_pop / number_districts)
        */
        int aim = get_population() / state_districts.size();
        vector<int> range = {aim - (int)(tolerance * aim), aim + (int)(tolerance * aim)};
        int max = 0; // largest difference
        p_index x = 0; // keep track of iteration
        
        for (Community c : state_communities) {
            int diff = abs(aim - c.get_population());
            if ((diff > max) && (diff > (int)(tolerance * aim))) {
                // community is outside range and more so than the previous one
                i = x;
                max = diff;
            }
            x++;
        }
    }

    return i;
}

void State::give_precinct(p_index precinct, p_index community, int t_type) {
    /*
        @desc: 
            performs a precinct transaction by giving `precinct` from `community` to
            a possible other community (dependent on which function it's being used for).
            This is the only way community borders can change.

        @params:
            `p_index` precinct: The position of the precinct to give in the community
            `p_index` community: The position of the community in the state array
            `int` t_type: the currently running process (consts defined in community.cpp)

        @return: void
    */

    Precinct precinct_shape = this->state_communities[community].precincts[precinct];

    // Canvas canvas(900, 900);
    // canvas.add_shape(this->state_communities);
    // canvas.add_shape(this->state_communities[community], true, Color(100,255,0), 1);
    // canvas.add_shape(precinct_shape, true, Color(0,100,255), 2);
    // canvas.draw();
    
    // get communities that border the current community
    p_index_set bordering_communities_i = get_bordering_shapes(this->state_communities, this->state_communities[community]);

    // convert to actual shape array
    Communities bordering_communities;
    for (p_index i : bordering_communities_i)
        bordering_communities.push_back(this->state_communities[i]);
    
    // of those communities, get the ones that also border the precinct
    p_index_set exchangeable_communities_i = get_bordering_shapes(bordering_communities, precinct_shape);
    Communities exchangeable_communities;
    for (p_index i : exchangeable_communities_i)
        exchangeable_communities.push_back(this->state_communities[i]);
    p_index exchange_choice;

    if (t_type == PARTISANSHIP) {
        // get closest average to precinct
        double min = abs(get_median_partisanship(exchangeable_communities[0]) - precinct_shape.get_ratio());
        p_index choice = 0;
        p_index index = 0;

        for (int i = 1; i < exchangeable_communities.size(); i++) {
            index++;
            Community c = exchangeable_communities[i];
            double diff = abs(get_median_partisanship(c) - precinct_shape.get_ratio());
            if (diff < min) {
                min = diff;
                choice = index;
            }
        }
        exchange_choice = choice;
    }
    else if (t_type == COMPACTNESS) {
        // get highest compactness score
        double min = exchangeable_communities[0].get_compactness();
        p_index choice = 0;
        p_index index = 0;

        for (int i = 1; i < exchangeable_communities.size(); i++) {
            index++;
            Community c = exchangeable_communities[i];
            double n_compactness = c.get_compactness();
            if (n_compactness < min) {
                min = n_compactness;
                choice = index;
            }
        }

        exchange_choice = choice;
    }

    Community chosen_c = exchangeable_communities[exchange_choice];
    // cout << "exchange c " << exchange_choice << endl;

    // add precinct to new community
    this->state_communities[exchange_choice].add_precinct(precinct_shape);

    // remove precinct from previous community
    this->state_communities[community].precincts.erase(this->state_communities[community].precincts.begin() + precinct);

    // update relevant borders after transactions
    for (p_index i : exchangeable_communities_i) {
        this->state_communities[i].border = generate_exterior_border(this->state_communities[i]).border;
    }

    this->state_communities[community].border = generate_exterior_border(this->state_communities[community]).border;
    
    return;
}


void State::refine_compactness(double compactness_tolerance) {
    /* 
        @desc:
            Optimize the state's communities for population. Attempts
            to minimize difference in population across the state
            with a tolerance for acceptable +- percent difference

        @params: `double` compactness_tolerance: tolerance for compactness
        @return: void
    */

    p_index worst_community = get_next_community(compactness_tolerance, COMPACTNESS);
    bool is_done = (worst_community == -1);
    vector<int> num_changes(state_communities.size());
    
    cout << "refining for compactness..." << endl;

    while (!is_done) {
        cout << "modifying community " << worst_community << endl;
        cout << "current worst compactness is " << state_communities[worst_community].get_compactness() << endl;

        coordinate center = state_communities[worst_community].get_center();
        Shape circle = generate_gon(center, sqrt(state_communities[worst_community].get_area() / PI), 30);
        
        p_index_set boundaries = get_exchangeable_precincts(state_communities[worst_community], state_communities);
        // p_index_set boundaries = (community);

        // for  each precinct in edge of community;
        for (int x = 0; x < boundaries.size(); x++) {
            Precinct pre = state_communities[worst_community].precincts[boundaries[x]];
            if (state_communities[worst_community].get_compactness() < compactness_tolerance && !get_inside(pre.hull, circle.hull) ) {
                cout << "Precinct outside circle, removing..." << endl;

                Canvas canvas(900, 900);
                canvas.add_shape(this->state_communities, true, Color(0,0,0), 1);
                canvas.draw();

                give_precinct(boundaries[x], worst_community, COMPACTNESS);
                for (int i = 0; i < boundaries.size(); i++) boundaries[i] = boundaries[i] - 1;
            }
        }

        num_changes[worst_community] += 1; // update the changelist
        // update worst_community, check stop condition
        worst_community = get_next_community(compactness_tolerance, COMPACTNESS);
        // if the community is within the tolerance, or if it has been modified too many times
        is_done = (worst_community == -1 || num_changes[worst_community] == MAX_ITERATIONS);
    }
}


void State::refine_partisan(double partisanship_tolerance) {
    /*
        @desc:
            optimize the partisanship of a community - attempts
            to minimize the stdev of partisanship of precincts
            within each community.

        @params: `double` partisanship_tolerance: tolerance for partisanship
        @return: void
    */

    cout << "Refining for partisanship..." << endl;

    p_index worst_community = get_next_community(partisanship_tolerance, PARTISANSHIP);
    bool is_done = (worst_community == -1);
    vector<int> num_changes(state_communities.size());

    int iter = 0;

    while (!is_done) {
        Community c = state_communities[worst_community];
        cout << "Current stdev is " << get_standard_deviation_partisanship(c) << endl;

        double median = get_median_partisanship(c);
        p_index worst_precinct = 0;
        double diff = 0;
        p_index_set exchangeable_precincts = get_exchangeable_precincts(c, state_communities);

        for (p_index p : exchangeable_precincts) {
            // if precinct exchange decreases stdev, exchange
            // if (get_standard_deviation_partisanship());

            c = state_communities[worst_community];

            Precinct_Group without;    
            for (int i = 0; i < c.precincts.size(); i++)
                if (i != p) without.precincts.push_back(c.precincts[i]);

            if (get_standard_deviation_partisanship(without) < get_standard_deviation_partisanship(c)) {
                cout << get_standard_deviation_partisanship(without) << " < " << get_standard_deviation_partisanship(c) << endl;
                cout << "need to give precinct " << p << endl;
                give_precinct(p, worst_community, PARTISANSHIP);
            }
        }

        num_changes[worst_community] += 1; // update the changelist
        // update worst_community, check stop condition
        worst_community = get_next_community(partisanship_tolerance, PARTISANSHIP);
        // if the community is within the tolerance, or if it has been modified too many times
        is_done = (worst_community == -1 || num_changes[worst_community] == MAX_ITERATIONS);
        iter++;
    }
}

void State::refine_population(double population_tolerance) {
    /* 
        @desc:
            optimize the state's communities for population. Attempts
            to minimize difference in population across the state
            with a tolerance for acceptable +- percent difference

        @params: `double` population_tolerance: tolerance for population
        @return: void
    */

    // find the first community to be optimized
    p_index worst_community = get_next_community(population_tolerance, POPULATION);
    bool is_done = (worst_community == -1); // initialize stop condition
    vector<int> num_changes(state_communities.size());

    // calculate the range each community's population should be within
    int aim = get_population() / state_districts.size();
    vector<int> ideal_range = {aim - (int)(population_tolerance * aim), aim + (int)(population_tolerance * aim)};
    
    // begin main iterative loop
    while (!is_done) {
        Community c = state_communities[worst_community];
        while (c.get_population() < ideal_range[0] || c.get_population() > ideal_range[1]) {
            //! Figure out which method we're using for precinct exchange
        }

        // update the changelist
        num_changes[worst_community] += 1; 
        
        // check stop condition, update the community that needs to be optimized
        worst_community = get_next_community(population_tolerance, POPULATION);
        is_done = (worst_community == -1 || num_changes[worst_community] == MAX_ITERATIONS);
    }
}

int measure_difference(Communities communities, Communities new_communities) {
    
    /*
        @desc:
            measures and returns how many precincts have changed communities
            in a given list of old and new communities. Used for checking when
            to stop the algorithm.

        @params:
            `Communities` communities, new_communities: community arrays to measure difference of

        @return: `int` difference in communities
    */
    
    int changed_precincts;

    // loop through each community
    for (int i = 0; i < communities.size(); i++) {
        // for each precinct in the community
        for (int x = 0; x < communities[i].precincts.size(); x++) {
            // check for precinct in corresponding new_community array
            Precinct old_p = communities[i].precincts[x];
            vector<Precinct> plist = new_communities[i].precincts;
            bool found = false;
            int index = 0;

            while (!found && index < plist.size()) {
                // loop through precincts in new community,
                // compare to precinct you're checking
                Precinct p = plist[index];
                if (old_p == p) found = true;
                index++;
            }

            // this precinct was not found, so add a changed precinct 
            if (!found) changed_precincts++;
        }
    }

    return changed_precincts;
}

void State::generate_communities(int num_communities, double compactness_tolerance, double partisanship_tolerance, double population_tolerance) {
    /*
        @desc:
            The driver method for the communities algorithm. The general process for
            running the political-community generation algorithm is calling void state
            methods that modify state variables, so not much passing around is done. 
            
            At the start, `generate_initial_communities` generates
            a random configuration. Then, it uses the iterative method to
            refine for a variable until the number of precincts that change is
            within a tolerance, set above (see CHANGED_PRECINCT_TOLERANCE).

            This State method returns nothing - to access results, check
            the state.state_communities property.

            The #defined WRITE determines whether or not to write binary
            community objects to a predefined directory. These can then be loaded
            in order to visualize algorithms. See the `community_playback` function
        
        @params:
            `int` num_communities: number of districts to generate
            `double` compactness_tolerance,
                     partisanship_tolerance,
                     population_tolerance: tolerances for processes
    
        @return: void
    */

    generate_initial_communities(num_communities);
    
    int changed_precincts = 0, i = 0;
    int precinct_change_tolerance = // the acceptable number of precincts that can change each iteration
        (CHANGED_PRECINT_TOLERANCE / 100) * this->precincts.size();

    Communities old_communities; // to store communities at the beginning of the iteration

    /*
        Do 30 iterations, and see how many precincts change each iteration
        ! This is only until we have a good idea for a stop condition. We
           first will plot results on a graph and regress to see the optimal
           point of minimal change.

        At some point this will be changed to:
           while (changed_precincts > precinct_change_tolerance)
    */
   
    // while (i < 30) {
    //     cout << "On iteration " << i << endl;
    //     old_communities = this->state_communities;

    //     if (VERBOSE) cout << "refining compacntess..." << endl;
    //     refine_compactness(compactness_tolerance);
        
    //     if (VERBOSE) cout << "refining partisanship..." << endl;
    //     refine_partisan(partisanship_tolerance);
        
    //     if (VERBOSE) cout << "refining population..." << endl;
    //     refine_population(population_tolerance);
  
    //     if (VERBOSE) cout << "measuring precincts changed..." << endl;      
    //     changed_precincts = measure_difference(old_communities, this->state_communities);
    //     if (VERBOSE) cout << changed_precincts << " precincts changed." << endl;
        
    //     i++;
    // }
}

string Community::save_frame() {
    string line;
    for (Precinct p : precincts) {
        line += "\"" + p.shape_id + "\", ";
    }

    line = line.substr(0, line.size() - 2);
    // cout << line << endl;
    return line;
}

Communities Community::load_frame(std::string read_path, State precinct_list) {
    Communities cs;
    string file = readf(read_path);
    std::stringstream fs(file);
    std::string line;

    while (getline(fs, line)) {
        Community c;
        vector<string> vals = split(line, "\"");

        for (string v : vals) {
            if (v != ", ") { // v contains a precinct id
                for (Precinct p : precinct_list.precincts) {
                    if (p.shape_id == v) {
                        c.add_precinct(p);
                    }
                } 
            }
        }

        c.border = generate_exterior_border(c).border;
        cs.push_back(c);
    }

    return cs;
}