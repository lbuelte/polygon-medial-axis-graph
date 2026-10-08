// Created by Laura Bülte on 2026-03-20.

#include "MedialAxisWithHoles.h"
#include "../io/config.h"

// ---------- sanity checks for medial axis --------------

bool is_connected(MedialAxisWithHoles const &ma);

bool has_hole_or_is_tree(MedialAxisWithHoles const &ma);

bool hole_yields_circle(MedialAxisWithHoles const &ma);

bool check_validity_of_medial_axis(MedialAxisWithHoles const &ma);


// --------- pruning helpers for medial axis -----------------

void contract_adjacent_covertices_from_medial_axis(MedialAxisWithHoles &ma, double distance_threshold);

void prune_collinear_vertices_from_medial_axis(MedialAxisWithHoles &ma, double degree_offset);

void prune_medial_axis(MedialAxisWithHoles &ma, MedialAxisPruningConfig const &config);
