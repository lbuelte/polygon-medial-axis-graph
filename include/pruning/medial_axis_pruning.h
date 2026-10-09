// Created by Laura Bülte on 2026-03-20.
//
// Sanity checks and pruning for medial axis graphs (used by compute_medial_axis_graph in
// src/pipeline.cpp). The pruning is greedy: its result depends on the node numbering,
// which is why MedialAxisGraph numbers its nodes by their coordinates.

#ifndef MEDIAL_AXIS_GRAPH_MEDIAL_AXIS_PRUNING_H
#define MEDIAL_AXIS_GRAPH_MEDIAL_AXIS_PRUNING_H

#include "medial_axis_graph.h"
#include "params.h"

#include <string>

// ---------- sanity checks for medial axis --------------

// True if the graph has at least one node and all nodes are connected
bool is_connected(MedialAxisGraph const &ma);

// True if the polygon has holes, or the graph is a tree (as for a polygon without holes)
bool has_hole_or_is_tree(MedialAxisGraph const &ma);

// True if the graph has as many independent cycles as the polygon has holes
bool holes_yield_cycles(MedialAxisGraph const &ma);

// All three checks above. If the graph is not valid and reason is given,
// the first failed check is stored in reason.
bool check_validity_of_medial_axis(MedialAxisGraph const &ma, std::string *reason = nullptr);


// --------- pruning helpers for medial axis -----------------

// Merges groups of adjacent nodes closer than distance_threshold into their mean point
void merge_close_adjacent_nodes(MedialAxisGraph &ma, double distance_threshold);

// Repeatedly removes degree-2 nodes whose two edges deviate from a straight line
// by at most degree_offset degrees, and connects their neighbours directly
void remove_nearly_collinear_nodes(MedialAxisGraph &ma, double degree_offset);

// Collinear removal, merging of close nodes, collinear removal again
void prune_medial_axis(MedialAxisGraph &ma, MedialAxisPruningParams const &pruning);

#endif // MEDIAL_AXIS_GRAPH_MEDIAL_AXIS_PRUNING_H
