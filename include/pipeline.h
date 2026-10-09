// Created by Laura Bülte on 2026-04-15.

#ifndef MEDIAL_AXIS_GRAPH_PIPELINE_H
#define MEDIAL_AXIS_GRAPH_PIPELINE_H

#include "cgal_types.h"
#include "medial_axis_graph.h"
#include "params.h"

// Computes the medial axis graph of one polygon:
//  1. prune the polygon; while it is not valid, perturb it randomly and prune again
//     (seeded with params.perturbation.seed, so results are reproducible)
//  2. compute the medial axis graph of the valid polygon and prune it
//     (if params.medial_axis_pruning.enabled)
//  3. if the graph is not valid (not connected, or not one cycle per hole), perturb the polygon
//     and repeat from 1, at most 10 times; after that the last graph is kept with a warning
// If perturb_immediately is true, the polygon is perturbed before the first pruning
// (used for instances on which CGAL got stuck or crashed before).
// keep_parabolas only affects SVG drawings of the graph (parabolic edges drawn as curves).
// Throws InvalidPolygonError if no valid polygon is found.
MedialAxisGraph compute_medial_axis_graph(
        Polygon_with_holes polygon,
        Params const &params,
        bool perturb_immediately = false,
        bool keep_parabolas = false);

#endif // MEDIAL_AXIS_GRAPH_PIPELINE_H
