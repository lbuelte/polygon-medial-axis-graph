// Algorithm parameters, read from an optional TOML file.
//
// Every parameter is defined in exactly one place: its struct member (with default value)
// below, plus one line in the matching for_each_param function in src/params.cpp, which
// gives its TOML key and description. Reading, the default template
// (--print-default-params) and the params_used.toml written next to the output
// are all generated from these functions.
//
// Distances are in the units of the input coordinates (metres for the intended input).

#ifndef MEDIAL_AXIS_GRAPH_PARAMS_H
#define MEDIAL_AXIS_GRAPH_PARAMS_H

#include <ostream>
#include <stdexcept>
#include <string>

// ---------------- polygon ----------------

// [polygon_pruning]: simplification of the input polygon before the medial axis is computed
struct PolygonPruningParams {
    double collinearity_threshold_deg = 2.0;    // remove vertices whose angle is within this of 180 degrees
    double merge_distance = 0.01;               // merge consecutive vertices closer than this ...
    double merge_distance_rel_bbox = 0.001;     // ... or than this fraction of the bbox diagonal (smaller one is used)
    double hole_vertex_offset = 0.01;           // distance by which hole vertices are moved off the outer boundary
};

// [validity]: when a pruned polygon counts as valid (otherwise it is perturbed and pruned again)
struct ValidityParams {
    double min_vertex_distance = 1e-4;          // minimum distance between any two vertices
    double collinearity_threshold_deg = 2.0;    // no vertex angle may be within this of 180 degrees
};

// [perturbation]: random perturbation of polygons that are invalid after pruning
struct PerturbationParams {
    unsigned seed = 42;                         // seed of the random perturbations (results are reproducible)
    double max_displacement_rel_bbox = 1e-5;    // maximum displacement per coordinate, as a fraction of the bbox diagonal
};

// ---------------- medial axis ----------------

// [medial_axis]: extraction of the medial axis graph from the Voronoi diagram
struct MedialAxisParams {
    double reflex_tolerance_deg = 2.0;          // vertices within this of 180 degrees are treated like reflex vertices
};

// [medial_axis_pruning]: pruning of the medial axis graph (not used by the pipeline yet)
struct MedialAxisPruningParams {
    double collinearity_threshold_deg = 10.0;   // remove degree-2 nodes whose angle is within this of 180 degrees
    double merge_distance = 0.01;               // merge adjacent nodes closer than this ...
    double merge_distance_rel_bbox = 0.005;     // ... or than this fraction of the bbox diagonal (smaller one is used)
};

struct Params {
    PolygonPruningParams polygon_pruning;
    ValidityParams validity;
    PerturbationParams perturbation;
    MedialAxisParams medial_axis;
    MedialAxisPruningParams medial_axis_pruning;
};

// Thrown for unreadable files, unknown sections or keys, and values of the wrong type
struct ParamsError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Reads parameters from a TOML file. Sections and keys that are missing keep their defaults.
Params load_params(const std::string &filename);

// Writes all parameters as a commented TOML file (that load_params can read again)
void write_params(const Params &params, std::ostream &out);

#endif // MEDIAL_AXIS_GRAPH_PARAMS_H
