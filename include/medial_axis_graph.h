// Created by philip on 2/18/26.
// Modified by Laura Bülte

#ifndef MEDIAL_AXIS_GRAPH_MEDIAL_AXIS_GRAPH_H
#define MEDIAL_AXIS_GRAPH_MEDIAL_AXIS_GRAPH_H

#include "cgal_types.h"

#include "json/json.hpp"

#include <set>
#include <string>
#include <utility>
#include <vector>

// Medial axis graph of a polygon with holes.
//
// The medial axis is computed from the segment Voronoi diagram of the polygon boundary
// (sites: all boundary vertices and edges). Voronoi edges that lie inside the polygon and
// do not end at a reflex (or nearly straight) polygon vertex form the medial axis.
// Their endpoints are the nodes of the graph, and each (straight or parabolic) Voronoi edge
// becomes one straight-line edge between its two nodes.
class MedialAxisGraph {

public:

    explicit MedialAxisGraph(Polygon_with_holes const &pwh)
        : polygon(pwh)
    {}

    // Computes the Voronoi diagram and extracts the medial axis graph from it.
    // Polygon vertices within reflex_tolerance_deg of 180 degrees are treated like reflex vertices.
    // keep_parabolas only affects the SVG drawing: parabolic edges are then drawn as curves.
    void create_medial_axis(double reflex_tolerance_deg, bool keep_parabolas);

    // Replaces the graph (e.g. after pruning the medial axis). All edges are treated as straight.
    void set_nodes_and_edges(std::vector<Point_2> nodes, std::vector<std::pair<int,int>> edges);

    size_t num_nodes() const { return nodes.size(); }
    size_t num_edges() const { return edges.size(); }
    size_t num_polygon_holes() const { return polygon.number_of_holes(); }

    // JSON object with the polygon and the graph, see the README for the format.
    // The graph nodes are stored under the key "vertices" (format expected by geomGMD).
    nlohmann::json to_json(int shape_id) const;

    // Draws the polygon and the medial axis graph as an SVG of canvas_size x canvas_size pixels
    void write_svg(const std::string& filename, double padding = 10, double canvas_size = 1000.0) const;

    const std::vector<Point_2>& get_nodes() const { return nodes; }
    const std::vector<std::pair<int,int>>& get_edges() const { return edges; }
    const std::vector<std::vector<int>>& get_adjacency_list() const { return adjacency_list; }
    const Polygon_with_holes& get_polygon() const { return polygon; }


private:

    // Rebuilds adjacency_list from edges
    void build_adjacency_list();

    // Collects the reflex and nearly straight vertices of the outer boundary and all holes
    void collect_reflex_vertices(std::set<Point_2>& reflex, double tolerance_deg) const;

    // Inserts all boundary edges (segment sites) and vertices (point sites) into sdg
    void insert_boundary_sites(SDG &sdg) const;

    // Selects the medial axis edges from the Voronoi diagram and fills nodes and edges
    void extract_medial_axis(VD const &vd, double reflex_tolerance_deg, bool keep_parabolas);

    // Draws only the polygon (used if the graph has no nodes)
    void write_svg_polygon_only(const std::string &filename, double padding, double canvas_size) const;

    const Polygon_with_holes polygon;   // input polygon (outer boundary CCW, holes CW)

    std::vector<Point_2> nodes;                     // node coordinates
    std::vector<std::pair<int,int>> edges;          // edges as pairs of node indices
    std::vector<bool> is_curve;                     // per edge: drawn as a parabola in the SVG
    std::vector<Parabola_segment_2> parabolas;      // per edge: the parabola (only used if is_curve)
    std::vector<std::vector<int>> adjacency_list;   // per node: indices of adjacent nodes
};


#endif // MEDIAL_AXIS_GRAPH_MEDIAL_AXIS_GRAPH_H
