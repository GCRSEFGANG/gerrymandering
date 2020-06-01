/*=======================================
 quantification.cpp:            k-vernooy
 last modified:               Sat May 16
 
 Determines how gerrymandered a district
 is given a community map
========================================*/

#include "../include/quantification.hpp"
#include <iostream>

using namespace std;
using namespace hte;
using namespace Geometry;
using namespace Graphics;

double hte::Geometry::collapse_vals(double a, double b) {
    // where `a` is the republican ratio and b is the general quantification
    assert((a <= 1.0 && a >= 0.0));
    assert((b <= 1.0 && b >= 0.0));
    return (a * (2.0 * (0.5 - b)));
}


double hte::Geometry::get_population_from_mask(Precinct_Group pg, Multi_Polygon mp) {
    double pop = 0;
    bounding_box bound = pg.get_bounding_box();
    for (Precinct p : pg.precincts) {
        if (bound_overlap(p.get_bounding_box(), bound)) {
            // get the overlap between mp and p
            // create paths array from polygon
            ClipperLib::Path subj = ring_to_path(p.hull);

            ClipperLib::Paths clip;
            for (Polygon p : mp.border)
                clip.push_back(ring_to_path(p.hull));

            ClipperLib::Paths solutions;
            ClipperLib::Clipper c; // the executor

            // execute union on paths array
            c.AddPath(subj, ClipperLib::ptSubject, true);
            c.AddPaths(clip, ClipperLib::ptClip, true);
            c.Execute(ClipperLib::ctIntersection, solutions, ClipperLib::pftNonZero);
            Multi_Polygon intersection = paths_to_multi_shape(solutions);
            double ratio = (intersection.get_area() / p.get_area());
            pop += ((double)p.pop * ratio);
        }
    }

    return pop;
}


std::map<POLITICAL_PARTY, double> hte::Geometry::get_partisanship_from_mask(Precinct_Group pg, Multi_Polygon mp) {
    std::map<POLITICAL_PARTY, double> partisanships;
    bounding_box bound = pg.get_bounding_box();

    for (auto& pair : pg.precincts[0].voter_data) {
        partisanships[pair.first] = 0;
    }

    for (Precinct p : pg.precincts) {
        if (bound_overlap(p.get_bounding_box(), bound)) {
            // get the overlap between mp and p
            // create paths array from polygon
            ClipperLib::Path subj = ring_to_path(p.hull);

            ClipperLib::Paths clip;
            for (Polygon p : mp.border)
                clip.push_back(ring_to_path(p.hull));

            ClipperLib::Paths solutions;
            ClipperLib::Clipper c; // the executor

            // execute union on paths array
            c.AddPath(subj, ClipperLib::ptSubject, true);
            c.AddPaths(clip, ClipperLib::ptClip, true);
            c.Execute(ClipperLib::ctIntersection, solutions, ClipperLib::pftNonZero);
            Multi_Polygon intersection = paths_to_multi_shape(solutions);
            double ratio = (intersection.get_area() / p.get_area());

            for (auto& pair : p.voter_data) {
                partisanships[pair.first] += (pair.second * ratio);
            }
        }
    }

    return partisanships;
}


std::map<POLITICAL_PARTY, double> Geometry::get_quantification(Graph& graph, Communities& communities, Multi_Polygon district) {
    // determines how gerrymandered `district` is with the `communities` map.
    // communities had better already be updated.
    double val = 0.0;
    bounding_box db = district.get_bounding_box();
    // get all communities that overlap the current district
    Precinct_Group state = Precinct_Group::from_graph(graph);
    double district_population = get_population_from_mask(state, district);

    int largest_index = -1;
    int largest_pop = -1;

    for (int i = 0; i < communities.size(); i++) {
        communities[i].update_shape(graph);
        if (bound_overlap(communities[i].shape.get_bounding_box(), db)) {
            // get intersection between the two shapes
            double pop = get_population_from_mask(communities[i].shape, district);
            if (pop > largest_pop) {
                largest_pop = pop;
                largest_index = i;
            }
        }
    }

    cout << largest_index << endl;

    // get the difference of the two shapes
    ClipperLib::Paths subj;
    for (Precinct p : communities[largest_index].shape.precincts) {
        subj.push_back(ring_to_path(p.hull));
    }

    ClipperLib::Paths clip;
    for (Polygon p : district.border)
        clip.push_back(ring_to_path(p.hull));

    ClipperLib::Paths solutions;
    ClipperLib::Clipper c; // the executor

    // execute union on paths array
    c.AddPaths(subj, ClipperLib::ptSubject, true);
    c.AddPaths(clip, ClipperLib::ptClip, true);
    c.Execute(ClipperLib::ctDifference, solutions, ClipperLib::pftNonZero);
    Multi_Polygon pop_not_in_district = paths_to_multi_shape(solutions);
    // cout << "got pop outside" << endl;
    // Canvas canvas(900, 900);
    // for (Polygon p : district.border) {
    //     Outline o = to_outline(p.hull);
    //     o.style().fill(RGB_Color(0,0,0));
    //     canvas.add_outline(o);
    // }
    // for (Precinct p : communities[largest_index].shape.precincts) {
    //     Outline o = to_outline(p.hull);
    //     o.style().fill(RGB_Color(100,100,100));
    //     canvas.add_outline(o);
    // }
    // for (Polygon p : pop_not_in_district.border) {
    //     Outline o = to_outline(p.hull);
    //     o.style().fill(RGB_Color(190,190,190)).outline(RGB_Color(0,0,0)).thickness(1);
    //     canvas.add_outline(o);
    // }
    // canvas.draw_to_window();

    std::map<POLITICAL_PARTY, double> partisanships = get_partisanship_from_mask(communities[largest_index].shape, pop_not_in_district);

    double sum = 0;
    for (auto& pair : partisanships) {
        sum += pair.second;
    }

    for (auto& pair : partisanships) {
        if (sum != 0) {
            partisanships[pair.first] /= sum;
        }
        else {
            partisanships[pair.first] = 0.5;   
        }
    }

    partisanships[POLITICAL_PARTY::ABSOLUTE_QUANTIFICATION] = get_population_from_mask(communities[largest_index].shape, pop_not_in_district) / communities[largest_index].get_population();
    return partisanships;
}


class Community_District_Link {
    public:
        int community;
        int district;
        double distance;
        friend bool operator< (const Community_District_Link& l1, const Community_District_Link& l2);
};

bool operator< (const Community_District_Link& l1, const Community_District_Link& l2) {
    return l1.distance < l2.distance;
}


std::array<double, 2> hte::Geometry::get_state_quantification(State& state, Communities& communities, std::vector<Multi_Polygon> districts) {
    // link each community with a respective district
    map<int, int> i_links;
    vector<Community_District_Link> links;
    for (int i = 0; i < districts.size(); i++) {
        for (int j = 0; j < communities.size(); j++) {
            // get distance to centroid
            Community_District_Link link;
            link.community = j;
            link.district = i;
            link.distance = get_distance(districts[i].get_centroid(), communities[j].shape.get_centroid());
            links.push_back(link);
        }
    }

    sort(links.begin(), links.end());

    while (links.size() > 0) {
        // add the shortest link to the map
        i_links[links[0].district] = links[0].community;
        // remove all other potential links to that district
        int d = links[0].district;
        int c = links[0].community;

        links.erase(
            std::remove_if(
                links.begin(), links.end(),
                [d, c](const Community_District_Link & o) { return (o.district == d || o.community == c); }
            ),
            links.end()
        );
    }

    // Canvas canvas(900, 900);
    // for (Multi_Polygon p : districts) {
    //     for (Polygon pl : p.border) {
    //         Outline o(pl.hull);
    //         o.style().fill(RGB_Color(-1,-1,-1)).outline(RGB_Color(255,0,0)).thickness(1);
    //         canvas.add_outline(o);
    //     }
    // }

    // for (Community c : communities) {
    //     for (Polygon pl : generate_exterior_border(c.shape).border) {
    //         Outline o(pl.hull);
    //         o.style().fill(RGB_Color(-1,-1,-1)).outline(RGB_Color(0,255,0)).thickness(1);
    //         canvas.add_outline(o);
    //     }
    // }
    
    for (auto& pair : i_links) {
        Canvas canvas(900, 900);
        for (Polygon pl : districts[pair.first].border) {
            Outline o(pl.hull);
            o.style().fill(RGB_Color(-1,-1,-1)).outline(RGB_Color(255,0,0)).thickness(1);
            canvas.add_outline(o);
        }
        for (Polygon pl : generate_exterior_border(communities[pair.second].shape).border) {
            Outline o(pl.hull);
            o.style().fill(RGB_Color(-1,-1,-1)).outline(RGB_Color(0,255,0)).thickness(1);
            canvas.add_outline(o);
        }
        canvas.draw_to_window();
        cout << "district " << pair.first << " linked to " << pair.second << endl;
    }

    // canvas.draw_to_window();
    return {0,0};
}
