// Created by Laura Bülte on 2026-03-20.

#ifndef MEDIALAXISWITHHOLES_POLYGON_PRUNING_H
#define MEDIALAXISWITHHOLES_POLYGON_PRUNING_H

#include "CGAL_includes.h"
#include "io/config.h"

#include <stdexcept>
#include <string>

    // Thrown if no valid polygon can be obtained by pruning and perturbation
    struct InvalidPolygonError : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    // ---------- sanity checks for polygon ------------

    // If the polygon is not valid and reason is given, the first problem found is stored in reason
    bool check_validity_of_polygon(Polygon_with_holes const &polygon, std::string *reason = nullptr);

    // --------- pruning helpers for polygon --------------

    void normalize_polygon_orientation(Polygon_with_holes& pwh);

    void contract_covertices_from_polygon(Polygon_with_holes &polygon, double distance_threshold);

    void prune_collinear_vertices_from_polygon(Polygon_with_holes &polygon, double degree_offset);

    void prune_hole_boundary_intersections(Polygon_with_holes &polygon);

    void prune_polygon(Polygon_with_holes &polygon, PolygonPruningConfig const &config);

    void perturb_polygon(Polygon_with_holes &polygon, double relative_eps, unsigned seed = 42);


#endif // MEDIALAXISWITHHOLES_POLYGON_PRUNING_H