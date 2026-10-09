// Created by Laura Bülte

#include "medial_axis_graph.h"

#include "geometry_utils.h"
#include "io/logger.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <tuple>
#include <utility>


void MedialAxisGraph::create_medial_axis(double reflex_tolerance_deg, bool keep_parabolas)
{
    // The segment Delaunay graph and its dual Voronoi diagram are only needed here
    SDG sdg;
    insert_boundary_sites(sdg);
    VD vd(sdg);

    extract_medial_axis(vd, reflex_tolerance_deg, keep_parabolas);
    sort_nodes_and_edges();
    build_adjacency_list();
}

void MedialAxisGraph::set_nodes_and_edges(
    std::vector<Point_2> nodes,
    std::vector<std::pair<int,int>> edges)
{
    this->nodes = std::move(nodes);
    this->edges = std::move(edges);

    // The new edges are not known to be parabolas, so draw all of them as straight lines
    is_curve.assign(this->edges.size(), false);
    parabolas.assign(this->edges.size(), Parabola_segment_2());

    build_adjacency_list();
}


nlohmann::json MedialAxisGraph::to_json(int shape_id) const
{
    nlohmann::json j;
    j["id"] = shape_id;

    // ---- Polygon ----
    j["polygon"] = nlohmann::json::object();
    j["polygon"]["outer"] = nlohmann::json::array();
    for (auto vit = polygon.outer_boundary().vertices_begin();
        vit != polygon.outer_boundary().vertices_end(); ++vit)
    {
        j["polygon"]["outer"].push_back({{"x", vit->x()}, {"y", vit->y()}});
    }
    j["polygon"]["holes"] = nlohmann::json::array();
    for (const auto& h : polygon.holes())
    {
        nlohmann::json hole_json = nlohmann::json::array();
        for (auto vit = h.vertices_begin(); vit != h.vertices_end(); ++vit)
        {
            hole_json.push_back({{"x", vit->x()}, {"y", vit->y()}});
        }
        j["polygon"]["holes"].push_back(hole_json);
    }

    // ---- Medial axis graph: nodes (stored under the key "vertices") and edges ----
    j["medial_axis"] = nlohmann::json::object();
    j["medial_axis"]["vertices"] = nlohmann::json::array();
    for (const auto& v : nodes)
    {
        j["medial_axis"]["vertices"].push_back({{"x", v.x()}, {"y", v.y()}});
    }
    j["medial_axis"]["edges"] = nlohmann::json::array();
    for (const auto& e : edges)
    {
        j["medial_axis"]["edges"].push_back({{"source", e.first}, {"target", e.second}});
    }

    return j;
}


void MedialAxisGraph::write_svg_polygon_only(
    const std::string &filename, double padding, double canvas_size) const
{
    if (polygon.outer_boundary().is_empty()) return;

    double radius = 2.0;

    // ---- compute bounding box from outer boundary ----
    auto bbox = polygon.outer_boundary().bbox();
    double min_x = bbox.xmin();
    double min_y = bbox.ymin();
    double width  = bbox.xmax() - min_x;
    double height = bbox.ymax() - min_y;
    if (width == 0 || height == 0) {
        log_warning("polygon has zero width or height, no SVG written");
        return;
    }

    double scale_x = (canvas_size - 2*padding) / width;
    double scale_y = (canvas_size - 2*padding) / height;
    double scale = std::min(scale_x, scale_y);

    // map polygon coordinates to SVG coordinates (y axis points down in SVG)
    auto sx = [&](double x) { return (x - min_x) * scale + padding; };
    auto sy = [&](double y) { return canvas_size - ((y - min_y) * scale + padding); };

    std::ofstream svg(filename);

    svg << "<svg xmlns='http://www.w3.org/2000/svg' width='" << canvas_size
        << "' height='" << canvas_size
        << "' viewBox='0 0 " << canvas_size << " " << canvas_size << "'>\n";

    svg << "<rect width='100%' height='100%' fill='white'/>\n";

    // ---- outer boundary ----
    svg << "<polygon points='";
    for (auto vit = polygon.outer_boundary().vertices_begin();
         vit != polygon.outer_boundary().vertices_end(); ++vit)
        svg << sx(vit->x()) << "," << sy(vit->y()) << " ";
    svg << "' style='fill:lightgray;stroke:gray;stroke-width:2'/>\n";

    // ---- holes ----
    for (const auto& h : polygon.holes()) {
        svg << "<polygon points='";
        for (auto vit = h.vertices_begin(); vit != h.vertices_end(); ++vit)
            svg << sx(vit->x()) << "," << sy(vit->y()) << " ";
        svg << "' style='fill:white;stroke:gray;stroke-width:2'/>\n";
    }

    // ---- draw vertices ----
    for (auto vit = polygon.outer_boundary().vertices_begin();
         vit != polygon.outer_boundary().vertices_end(); ++vit)
    {
        svg << "<circle cx='" << sx(vit->x())
            << "' cy='" << sy(vit->y())
            << "' r='" << radius
            << "' fill='red'/>\n";
    }

    for (const auto& h : polygon.holes()) {
        for (auto vit = h.vertices_begin(); vit != h.vertices_end(); ++vit) {
            svg << "<circle cx='" << sx(vit->x())
                << "' cy='" << sy(vit->y())
                << "' r='" << radius
                << "' fill='orange'/>\n";
        }
    }

    // ---- polygon statistics ----
    std::size_t num_holes = polygon.number_of_holes();
    std::size_t num_outer_vertices = polygon.outer_boundary().size();
    std::size_t num_hole_vertices = 0;
    for (const auto& h : polygon.holes())
        num_hole_vertices += h.size();

    double legend_x = 15;
    double legend_y = 25;
    double line_step = 18;

    svg << "<text x='" << legend_x
        << "' y='" << legend_y
        << "' font-family='Arial' font-size='14' fill='black'>"
        << "Outer vertices: " << num_outer_vertices
        << "</text>\n";

    svg << "<text x='" << legend_x
        << "' y='" << (legend_y + line_step)
        << "' font-family='Arial' font-size='14' fill='black'>"
        << "Holes: " << num_holes
        << "</text>\n";

    svg << "<text x='" << legend_x
        << "' y='" << (legend_y + 2*line_step)
        << "' font-family='Arial' font-size='14' fill='black'>"
        << "Hole vertices: " << num_hole_vertices
        << "</text>\n";

    svg << "</svg>\n";
    svg.close();

    log_info("polygon-only SVG written to " + filename);
}


void MedialAxisGraph::write_svg(
        const std::string &filename, double padding, double canvas_size) const
{
    double radius = 2.0; // fixed small radius for visualization

    if (nodes.empty()) {
        write_svg_polygon_only(filename, padding, canvas_size);
        return;
    }

    // ---- bounding box of the polygon (contains the whole graph) ----
    auto bbox = polygon.outer_boundary().bbox();
    double min_x = bbox.xmin();
    double min_y = bbox.ymin();
    double width  = bbox.xmax() - min_x;
    double height = bbox.ymax() - min_y;

    if (width == 0 || height == 0) {
        log_warning("polygon has zero width or height, no SVG written");
        return;
    }

    // ---- compute scaling factor ----
    double scale_x = (canvas_size - 2*padding) / width;
    double scale_y = (canvas_size - 2*padding) / height;
    double scale = std::min(scale_x, scale_y);

    // map polygon coordinates to SVG coordinates (y axis points down in SVG)
    auto sx = [&](double x) { return (x - min_x) * scale + padding; };
    auto sy = [&](double y) { return canvas_size - ((y - min_y) * scale + padding); };

    std::ofstream svg(filename);
    svg << "<svg xmlns='http://www.w3.org/2000/svg' width='" << canvas_size
        << "' height='" << canvas_size
        << "' viewBox='0 0 " << canvas_size << " " << canvas_size << "'>\n";

    svg << "<rect x='0' y='0' width='" << canvas_size
        << "' height='" << canvas_size
        << "' style='fill:white'/>\n";

    // ---- draw outer polygon ----
    svg << "<polygon points='";
    for (auto vit = polygon.outer_boundary().vertices_begin(); vit != polygon.outer_boundary().vertices_end(); ++vit)
        svg << sx(vit->x()) << "," << sy(vit->y()) << " ";
    svg << "' style='fill:lightgray;stroke:lightgray;stroke-width:2'/>\n";

    // ---- draw holes ----
    for (const auto& h : polygon.holes()) {
        svg << "<polygon points='";
        for (auto vit = h.vertices_begin(); vit != h.vertices_end(); ++vit)
            svg << sx(vit->x()) << "," << sy(vit->y()) << " ";
        svg << "' style='fill:white;stroke:lightgray;stroke-width:2'/>\n";
    }

    // ---- draw outer polygon vertices ----
    for (auto vit = polygon.outer_boundary().vertices_begin();
        vit != polygon.outer_boundary().vertices_end(); ++vit)
    {
        svg << "<circle cx='" << sx(vit->x())
            << "' cy='" << sy(vit->y())
            << "' r='" << radius
            << "' fill='red' stroke='black' stroke-width='1'/>\n";
    }

    // ---- draw hole vertices ----
    for (const auto& h : polygon.holes()) {
        for (auto vit = h.vertices_begin(); vit != h.vertices_end(); ++vit) {
            svg << "<circle cx='" << sx(vit->x())
                << "' cy='" << sy(vit->y())
                << "' r='" << radius
                << "' fill='orange' stroke='black' stroke-width='1'/>\n";
        }
    }

    // ---- draw medial axis edges: straight lines in green, parabolas in blue ----
    for (size_t k = 0; k < edges.size(); ++k) {
        const auto& p1 = nodes[edges[k].first];
        const auto& p2 = nodes[edges[k].second];

        if (!is_curve[k]) {
            svg << "<line x1='" << sx(p1.x()) << "' y1='" << sy(p1.y())
                << "' x2='" << sx(p2.x()) << "' y2='" << sy(p2.y())
                << "' style='stroke:green;stroke-width:2'/>\n";
        } else {
            const Parabola_segment_2& par = parabolas[k];
            std::vector<Point_2> pts;
            par.generate_points(pts, 0.01);

            svg << "<polyline fill='none' stroke='blue' stroke-width='2' points='";
            for (const auto& p : pts)
                svg << sx(p.x()) << "," << sy(p.y()) << " ";
            svg << "'/>\n";
        }
    }

    // ---- draw medial axis nodes ----
    for (const auto& v : nodes) {
        svg << "<circle cx='" << sx(v.x()) << "' cy='" << sy(v.y())
            << "' r='" << radius << "' fill='black' stroke='black' stroke-width='2'/>\n";
    }

    // ---- legend with graph size ----
    double legend_x = 15;
    double legend_y = 25;

    svg << "<text x='" << legend_x
        << "' y='" << legend_y
        << "' font-family='Arial' font-size='14' fill='black'>"
        << "Nodes: " << nodes.size()
        << "</text>\n";

    svg << "<text x='" << legend_x
        << "' y='" << (legend_y + 18)
        << "' font-family='Arial' font-size='14' fill='black'>"
        << "Edges: " << edges.size()
        << "</text>\n";

    svg << "<text x='" << legend_x
        << "' y='" << (legend_y + 36)
        << "' font-family='Arial' font-size='14' fill='black'>"
        << "BBox Size: " << width << " x " << height
        << "</text>\n";

    svg << "</svg>\n";
    svg.close();

    log_info("SVG written to " + filename);
}


void MedialAxisGraph::build_adjacency_list()
{
    adjacency_list.clear();
    adjacency_list.resize(nodes.size());

    for (const auto& [i1, i2] : edges) {
        // Insert each neighbour only once, even if there are parallel edges
        if (std::find(adjacency_list[i1].begin(), adjacency_list[i1].end(), i2) == adjacency_list[i1].end()) {
            adjacency_list[i1].push_back(i2);
        }
        if (std::find(adjacency_list[i2].begin(), adjacency_list[i2].end(), i1) == adjacency_list[i2].end()) {
            adjacency_list[i2].push_back(i1);
        }
    }
}


// Collect set of reflex vertices on polygon boundary and holes
void MedialAxisGraph::collect_reflex_vertices(std::set<Point_2> &reflex, double tolerance_deg) const
{
    // Helper that adds the reflex (or nearly straight) vertices of one ring
    auto collect_from_ring = [&](const Polygon &ring) {
        size_t n = ring.size();
        for (size_t i = 0; i < n; ++i)
        {
            const Point_2& prev = ring[(i + n - 1) % n];
            const Point_2& curr = ring[i];
            const Point_2& next = ring[(i + 1) % n];

            if (is_reflex_or_nearly_straight(prev, curr, next, tolerance_deg))
                reflex.insert(curr);
        }
    };

    // Holes are oriented clockwise, so the same right-turn test finds their reflex vertices
    collect_from_ring(polygon.outer_boundary());
    for (const auto &hole : polygon.holes())
        collect_from_ring(hole);
}


// Each boundary edge is inserted as a segment site and each boundary vertex as a point site,
// so the Voronoi diagram separates edges from their endpoints (needed for parabolic arcs).
void MedialAxisGraph::insert_boundary_sites(SDG &sdg) const
{
    // Helper that inserts all edges and vertices of one ring
    auto insert_ring = [&](const Polygon &ring) {
        size_t n = ring.size();
        for (size_t v = 0; v < n; ++v){
            sdg.insert(ring[v], ring[(v + 1) % n]);
            sdg.insert(ring[v]);
        }
    };

    insert_ring(polygon.outer_boundary());
    for (const auto &hole : polygon.holes())
        insert_ring(hole);
}


// Build the medial axis graph from the Voronoi diagram: keep the finite Voronoi edges
// that lie inside the polygon and do not end at a reflex (or nearly straight) polygon vertex
void MedialAxisGraph::extract_medial_axis(VD const &vd, double reflex_tolerance_deg, bool keep_parabolas)
{
    nodes.clear();
    edges.clear();
    is_curve.clear();
    parabolas.clear();

    // True if p lies strictly inside the outer boundary and outside all holes
    auto inside_domain = [&](const Point_2 &p){
        if (polygon.outer_boundary().bounded_side(p) != CGAL::ON_BOUNDED_SIDE)
            return false;

        for (const auto &hole : polygon.holes()){
            if (hole.bounded_side(p) == CGAL::ON_BOUNDED_SIDE)
                return false;
        }
        return true;
    };

    std::set<Point_2> reflex;
    collect_reflex_vertices(reflex, reflex_tolerance_deg);

    std::set<Vertex_handle> used;                   // Voronoi vertices that become graph nodes
    std::map<Vertex_handle,int> index;              // Voronoi vertex -> node index
    std::vector<Halfedge_handle> medial_axis_edges; // selected Voronoi edges

    // select valid edges
    for (auto eit = vd.edges_begin();
         eit != vd.edges_end(); ++eit)
    {
        Halfedge_handle h = eit;

        // skip infinite edges
        if (!h->has_source() || !h->has_target())
            continue;

        Point_2 p1 = h->source()->point();
        Point_2 p2 = h->target()->point();

        if (reflex.count(p1) || reflex.count(p2))
            continue;

        Point_2 mid = CGAL::midpoint(p1,p2);

        if (!inside_domain(mid))
            continue;

        used.insert(h->source());
        used.insert(h->target());
        medial_axis_edges.push_back(h);
    }

    // build node array
    for (auto vh : used) {
        index[vh] = nodes.size();
        nodes.push_back(vh->point());
    }

    // build edges from the selected Voronoi edges only
    for (Halfedge_handle h : medial_axis_edges) {

        // map Voronoi vertex handles to compact node indices
        int i1 = index[h->source()];
        int i2 = index[h->target()];

        // extract the primal object (segment or parabola)
        auto dual_obj = vd.dual().primal(h->dual());
        Segment_2 s;
        Parabola_segment_2 par;

        if (CGAL::assign(s, dual_obj)) {
            // straight segment
            edges.emplace_back(i1, i2);
            is_curve.push_back(false);
            parabolas.emplace_back(); // placeholder
        }
        else if (CGAL::assign(par, dual_obj)) {
            // parabolic arc: always a straight edge in the graph, optionally drawn as a curve
            edges.emplace_back(i1, i2);
            is_curve.push_back(keep_parabolas);
            parabolas.push_back(keep_parabolas ? par : Parabola_segment_2());
        }
        else {
            log_warning("unknown Voronoi edge type, edge skipped");
        }
    }
}


void MedialAxisGraph::sort_nodes_and_edges()
{
    // order[k] = old index of the node that gets the new index k
    std::vector<int> order(nodes.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b){ return nodes[a] < nodes[b]; });

    std::vector<int> new_index(nodes.size());
    std::vector<Point_2> sorted_nodes;
    for (size_t k = 0; k < order.size(); ++k) {
        new_index[order[k]] = k;
        sorted_nodes.push_back(nodes[order[k]]);
    }

    // Edges with new node indices (smaller one first), together with their index in edges
    std::vector<std::tuple<int, int, size_t>> sorted;
    for (size_t k = 0; k < edges.size(); ++k) {
        int u = new_index[edges[k].first];
        int v = new_index[edges[k].second];
        sorted.emplace_back(std::min(u, v), std::max(u, v), k);
    }
    std::sort(sorted.begin(), sorted.end());

    std::vector<std::pair<int,int>> sorted_edges;
    std::vector<bool> sorted_is_curve;
    std::vector<Parabola_segment_2> sorted_parabolas;
    for (const auto &[u, v, k] : sorted) {
        sorted_edges.emplace_back(u, v);
        sorted_is_curve.push_back(is_curve[k]);
        sorted_parabolas.push_back(parabolas[k]);
    }

    nodes = std::move(sorted_nodes);
    edges = std::move(sorted_edges);
    is_curve = std::move(sorted_is_curve);
    parabolas = std::move(sorted_parabolas);
}
