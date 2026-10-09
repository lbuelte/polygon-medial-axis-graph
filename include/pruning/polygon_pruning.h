// Created by Laura Bülte on 2026-03-20.
//
// Validity check, pruning (simplification) and random perturbation of polygons with holes.
// These steps make the input robust for the segment Voronoi diagram.

#ifndef MEDIAL_AXIS_GRAPH_POLYGON_PRUNING_H
#define MEDIAL_AXIS_GRAPH_POLYGON_PRUNING_H

#include "cgal_types.h"
#include "params.h"

#include <stdexcept>
#include <string>

// Thrown if no valid polygon can be obtained by pruning and perturbation
struct InvalidPolygonError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// ---------- sanity checks for polygon ------------

// Checks the assumptions of the medial axis computation: outer boundary CCW and holes CW,
// all rings simple, no (nearly) duplicate vertices or edges, no nearly collinear consecutive
// vertices. Not checked: holes lie inside the outer boundary and do not intersect each other.
// Thresholds: see ValidityParams. If the polygon is not valid and reason is given,
// the first problem found is stored in reason.
bool check_validity_of_polygon(Polygon_with_holes const &polygon, ValidityParams const &validity,
                               std::string *reason = nullptr);

// --------- pruning helpers for polygon --------------

// Orients the outer boundary counterclockwise and all holes clockwise
void normalize_polygon_orientation(Polygon_with_holes& pwh);

// Replaces each group of consecutive vertices closer than distance_threshold by their mean point
void merge_close_vertices(Polygon_with_holes &polygon, double distance_threshold);

// Repeatedly removes vertices whose angle deviates from 180 degrees by at most degree_offset
void remove_nearly_collinear_vertices(Polygon_with_holes &polygon, double degree_offset);

// Moves hole vertices that coincide with an outer boundary vertex by offset away from the
// outer boundary (halving the distance until the polygon is valid).
// Throws InvalidPolygonError if no valid position is found.
void move_hole_vertices_off_outer_boundary(Polygon_with_holes &polygon, double offset,
                                           ValidityParams const &validity);

// Pruning pipeline: remove nearly collinear vertices, merge close vertices (distance threshold:
// the smaller of the absolute and the bounding-box-relative threshold), remove nearly collinear
// vertices again, move hole vertices off the outer boundary
void prune_polygon(Polygon_with_holes &polygon, PolygonPruningParams const &pruning,
                   ValidityParams const &validity);

// Moves every vertex by a random offset in [-eps, eps] in x and y, where
// eps = relative_eps * bounding box diagonal. The seed makes the perturbation reproducible.
void perturb_polygon(Polygon_with_holes &polygon, double relative_eps, unsigned seed);


#endif // MEDIAL_AXIS_GRAPH_POLYGON_PRUNING_H
