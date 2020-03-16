/*=======================================
 geometry.hpp                   k-vernooy
 last modified:               Fri, Feb 28
 
 Declarations of functions for geometrical
 manipulations and searching algorithms.
========================================*/

#pragma once // avoid multiple includes

#include "shape.hpp"
#include <math.h>  // for rounding functions
#include <algorithm> // for reverse, unique
#include <cmath>
#include <iostream>
#include <string>
#include "../lib/clipper/clipper.hpp"

#define PI 3.14159265358979323846

GeoGerry::segment coords_to_seg(GeoGerry::coordinate c1, GeoGerry::coordinate c2);

double get_distance(GeoGerry::coordinate c1, GeoGerry::coordinate c2);
double get_distance(std::array<long long int, 2> c1, std::array<long long int, 2> c2);
double get_distance(GeoGerry::segment s);

// coordinate manipulation for gui draw methods
GeoGerry::bounding_box normalize_coordinates(GeoGerry::Shape* shape);
GeoGerry::coordinate_set resize_coordinates(GeoGerry::bounding_box box, GeoGerry::coordinate_set shape, int screenX, int screenY);

// get outside border from a group of precincts
GeoGerry::Multi_Shape generate_exterior_border(GeoGerry::Precinct_Group p);

// get precincts on the inside border of a precinct group
GeoGerry::p_index_set get_inner_boundary_precincts(GeoGerry::Precinct_Group shape);
GeoGerry::p_index_set get_inner_boundary_precincts(GeoGerry::p_index_set precincts, GeoGerry::State state);

GeoGerry::p_index_set get_bordering_precincts(GeoGerry::Precinct_Group shape, int p_index);
GeoGerry::p_index_set get_ext_bordering_precincts(GeoGerry::Precinct_Group precincts, GeoGerry::State state);
GeoGerry::p_index_set get_ext_bordering_precincts(GeoGerry::Precinct_Group precincts, GeoGerry::p_index_set available_pre, GeoGerry::State state);

bool get_bordering(GeoGerry::Shape s0, GeoGerry::Shape s1);

// overload get_bordering_shapes for vector inheritance problem
GeoGerry::p_index_set get_bordering_shapes(std::vector<GeoGerry::Shape> shapes, GeoGerry::Shape shape);
GeoGerry::p_index_set get_bordering_shapes(std::vector<GeoGerry::Precinct_Group> shapes, GeoGerry::Shape shape);
GeoGerry::p_index_set get_bordering_shapes(std::vector<GeoGerry::Community> shapes, GeoGerry::Community shape);
GeoGerry::p_index_set get_bordering_shapes(std::vector<GeoGerry::Community> shapes, GeoGerry::Shape shape);

// for clipper conversions
ClipperLib::Path ring_to_path(GeoGerry::LinearRing ring);
GeoGerry::LinearRing path_to_ring(ClipperLib::Path path);
ClipperLib::Paths shape_to_paths(GeoGerry::Shape shape);
GeoGerry::Multi_Shape paths_to_multi_shape(ClipperLib::Paths paths);

GeoGerry::p_index_set get_giveable_precincts(GeoGerry::Community c, GeoGerry::Communities cs);
std::vector<std::array<int, 2>> get_takeable_precincts(GeoGerry::Community c, GeoGerry::Communities cs);

// geos::geom::LinearRing* create_linearring(GeoGerry::coordinate_set coords);
// geos::geom::Point* create_point(double x, double y);
// geos::geom::Geometry* shape_to_poly(GeoGerry::LinearRing shape);
// GeoGerry::Shape poly_to_shape(const geos::geom::Geometry* path);
// GeoGerry::Multi_Shape* multipoly_to_shape(geos::geom::MultiPolygon* paths);
// geos::geom::Geometry::NonConstVect multi_shape_to_poly(GeoGerry::Multi_Shape ms);

// for algorithm helper methods
double get_standard_deviation_partisanship(GeoGerry::Precinct_Group pg);
double get_standard_deviation_partisanship(GeoGerry::Communities cs);

double get_median_partisanship(GeoGerry::Precinct_Group pg);

bool point_in_ring(GeoGerry::coordinate coord, GeoGerry::LinearRing lr);
bool get_inside(GeoGerry::LinearRing s0, GeoGerry::LinearRing s1);
bool get_inside_first(GeoGerry::LinearRing s0, GeoGerry::LinearRing s1);

bool creates_island(GeoGerry::Precinct_Group set, GeoGerry::p_index remove);
bool creates_island(GeoGerry::p_index_set set, GeoGerry::p_index remove, GeoGerry::State precincts);
bool creates_island(GeoGerry::Precinct_Group set, GeoGerry::Precinct precinct);

GeoGerry::Shape generate_gon(GeoGerry::coordinate c, double radius, int n);