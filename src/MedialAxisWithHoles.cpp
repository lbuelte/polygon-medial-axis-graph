// Created by Laura Bülte

#include "MedialAxisWithHoles.h"

#include "geometry_utils.h"
#include "io/logger.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <utility>


void MedialAxisWithHoles::create_medial_axis(Config const &config) 
{
    initialize_voronoi();
    extract_medial_axis(config.ma.use_parabolas);
    build_adjacency_list();
}
\
void MedialAxisWithHoles::set_vertices_and_edges(
    std::vector<Point_2> vertices,
    std::vector<std::pair<int,int>> edges)
{
    // Move the data into the member variables
    this->vertices = std::move(vertices);
    this->edges = std::move(edges);

    build_adjacency_list(); // rebuild adjacency after updating edges
}


void MedialAxisWithHoles::write_svg_polygon_only(
    const std::string &filename, double padding, double canvas_size) const
{
    if (polygon.outer_boundary().is_empty()) return;

    double radius = 2.0;

    // ---- compute bounding box from outer boundary ----
    auto vit = polygon.outer_boundary().vertices_begin();

    double min_x = vit->x();
    double min_y = vit->y();
    double max_x = min_x;
    double max_y = min_y;

    for (; vit != polygon.outer_boundary().vertices_end(); ++vit) {
        min_x = std::min(min_x, vit->x());
        min_y = std::min(min_y, vit->y());
        max_x = std::max(max_x, vit->x());
        max_y = std::max(max_y, vit->y());
    }

    double width  = max_x - min_x;
    double height = max_y - min_y;
    if (width == 0 || height == 0) return;

    double scale_x = (canvas_size - 2*padding) / width;
    double scale_y = (canvas_size - 2*padding) / height;
    double scale = std::min(scale_x, scale_y);

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
    std::size_t num_holes = std::distance(polygon.holes_begin(),
                                        polygon.holes_end());

    std::size_t num_outer_vertices =
        polygon.outer_boundary().size();

    std::size_t num_hole_vertices = 0;
    for (const auto& h : polygon.holes())
        num_hole_vertices += h.size();

    std::size_t total_polygon_vertices =
        num_outer_vertices + num_hole_vertices;

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

    std::cout << "Polygon-only SVG written to " << filename << std::endl;
}


void MedialAxisWithHoles::write_svg_with_holes(
        const std::string &filename, double padding, double canvas_size) const 
{
    double radius = 2.0; // fixed small radius for visualization

    if (vertices.empty()) {
        write_svg_polygon_only(filename, padding, canvas_size);
        return;
    }

    // ---- bounding box ----
    double min_x = vertices[0].x();
    double min_y = vertices[0].y();
    double max_x = min_x;
    double max_y = min_y;

    for (const auto& v : vertices) {
        min_x = std::min(min_x, v.x());
        min_y = std::min(min_y, v.y());
        max_x = std::max(max_x, v.x());
        max_y = std::max(max_y, v.y());
    }

    double width  = max_x - min_x;
    double height = max_y - min_y;

    if (width == 0 || height == 0) return;

    // ---- compute scaling factor ----
    double scale_x = (canvas_size - 2*padding) / width;
    double scale_y = (canvas_size - 2*padding) / height;
    double scale = std::min(scale_x, scale_y);

    // ---- helper lambda to scale coordinates ----
    auto sx = [&](double x) { return (x - min_x) * scale + padding; };
    auto sy = [&](double y) { return canvas_size - ((y - min_y) * scale + padding); }; // flip Y

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

    // ---- draw medial axis edges ----
    for (size_t k = 0; k < edges.size(); ++k) {
        int i1 = edges[k].first;
        int i2 = edges[k].second;
        const auto& p1 = vertices[i1];
        const auto& p2 = vertices[i2];

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

    // ---- draw medial axis vertices ----
    for (const auto& v : vertices) {
        svg << "<circle cx='" << sx(v.x()) << "' cy='" << sy(v.y())
            << "' r='" << radius << "' fill='black' stroke='black' stroke-width='2'/>\n";
    }

    // ---- legend with graph size ----
    std::size_t num_vertices = vertices.size();
    std::size_t num_edges    = edges.size();

    double legend_x = 15;
    double legend_y = 25;

    svg << "<text x='" << legend_x
        << "' y='" << legend_y
        << "' font-family='Arial' font-size='14' fill='black'>"
        << "Vertices: " << num_vertices
        << "</text>\n";

    svg << "<text x='" << legend_x
        << "' y='" << (legend_y + 18)
        << "' font-family='Arial' font-size='14' fill='black'>"
        << "Edges: " << num_edges
        << "</text>\n";

        svg << "<text x='" << legend_x
        << "' y='" << (legend_y + 36)
        << "' font-family='Arial' font-size='14' fill='black'>"
        << "BBox Size: " << width << " x " << height
        << "</text>\n";

        svg << "</svg>\n";
        svg.close();

        std::cout << "SVG written to " << filename << " (scaled to " << canvas_size << "x" << canvas_size << ")" << std::endl;
    }


void MedialAxisWithHoles::build_adjacency_list() 
{
    adjacency_list.clear();
    adjacency_list.resize(vertices.size());

    adjacency_list_edge_association.clear();
    adjacency_list_edge_association.resize(vertices.size());

    for (size_t k = 0; k < edges.size(); ++k) {
        int i1 = edges[k].first;
        int i2 = edges[k].second;

        // Forward insertion: i1 -> i2
        if (std::find(adjacency_list[i1].begin(), adjacency_list[i1].end(), i2) == adjacency_list[i1].end()) {
            adjacency_list[i1].push_back(i2);
            adjacency_list_edge_association[i1].push_back(k);
        }

        // Backward insertion: i2 -> i1
        if (std::find(adjacency_list[i2].begin(), adjacency_list[i2].end(), i1) == adjacency_list[i2].end()) {
            adjacency_list[i2].push_back(i1);
            adjacency_list_edge_association[i2].push_back(k);
        }
    }

    // Optional: sanity check
    for (size_t k = 0; k < edges.size(); ++k) {
        int i1 = edges[k].first;
        int i2 = edges[k].second;
        assert(std::find(adjacency_list[i1].begin(), adjacency_list[i1].end(), i2) != adjacency_list[i1].end());
        assert(std::find(adjacency_list[i2].begin(), adjacency_list[i2].end(), i1) != adjacency_list[i2].end());
    }
}


// Collect set of reflex vertices on polygon boundary and holes
void MedialAxisWithHoles::collect_reflex_vertices(std::set<Point_2> &reflex) const 
{
    // outer
    const Polygon& outer = polygon.outer_boundary();
    int n = outer.size();

    for (int i=0;i<n;++i)
    {
        const Point_2& prev = outer[(i-1+n)%n];
        const Point_2& curr = outer[i];
        const Point_2& next = outer[(i+1)%n];

        if (is_reflex_vertex(prev,curr,next))
            reflex.insert(curr);
    }

    // holes
    for (auto hit = polygon.holes_begin();
         hit != polygon.holes_end(); ++hit)
    {
        const Polygon& hole = *hit;
        int m = hole.size();

        for (int i=0;i<m;++i)
        {
            const Point_2& prev = hole[(i-1+m)%m];
            const Point_2& curr = hole[i];
            const Point_2& next = hole[(i+1)%m];

            if (is_reflex_vertex(prev,curr,next))
                reflex.insert(curr);
        }
    }
}


// Build voronoi diagram of polygon with holes
void MedialAxisWithHoles::initialize_voronoi() 
{
    sdg.clear();
    vd.clear();

    // outer
    for (int v = 0; v < polygon.outer_boundary().size(); ++v){
        sdg.insert(polygon.outer_boundary().vertices()[v],
                   polygon.outer_boundary().vertices()[(v + 1) % polygon.outer_boundary().size()]);
        sdg.insert(polygon.outer_boundary().vertices()[v]);
    }

    // holes
    for (int h = 0; h < polygon.number_of_holes(); ++h){
        for (int w = 0; w < polygon.holes()[h].size(); ++w){
            sdg.insert(polygon.holes()[h][w],
                       polygon.holes()[h][(w + 1) % polygon.holes()[h].size()]);
            sdg.insert(polygon.holes()[h][w]);
        }
    }
    vd = VD(sdg);
}


// Build medial axis from Voronoi diagram by deleting edges to reflex vertices
void MedialAxisWithHoles::extract_medial_axis(bool use_parabolas) 
{
    vertices.clear();
    edges.clear();

    auto inside_domain = [&](const Point_2 &p){
        if (polygon.outer_boundary().bounded_side(p) != CGAL::ON_BOUNDED_SIDE)
            return false;

        for (auto hit = polygon.holes_begin();
            hit != polygon.holes_end(); ++hit){
            if (hit->bounded_side(p) == CGAL::ON_BOUNDED_SIDE)
                return false;
        }
        return true;
    };

    std::set<Point_2> reflex;
    collect_reflex_vertices(reflex);

    std::map<Vertex_handle,int> index;
    std::set<Vertex_handle> used;

    // select valid edges
    for (auto eit = vd.edges_begin();
         eit != vd.edges_end(); ++eit)
    {
        Halfedge_handle h = eit;

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
    }

    // build vertex array
    for (auto vh : used) {
        index[vh] = vertices.size();
        vertices.push_back(vh->point());
    }

    for (auto eit = vd.edges_begin(); eit != vd.edges_end(); ++eit) {
        Halfedge_handle h = eit;

        // skip infinite edges
        if (!h->has_source() || !h->has_target())
            continue;

        // skip edges ending at unused vertices (e.g., reflex / outside)
        if (!used.count(h->source()) || !used.count(h->target()))
            continue;

        // map vertex handles to compact indices
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
        // Here we have an option to consider the edges as parabolas
        else if (CGAL::assign(par, dual_obj)) {
            if (use_parabolas){
                // parabola segment
                edges.emplace_back(i1, i2);
                is_curve.push_back(true);
                parabolas.push_back(par);
            } else {
                // treat parabolas as straight parts as well
                edges.emplace_back(i1, i2);
                is_curve.push_back(false);
                parabolas.emplace_back(); // placeholder
            }
        }          
        else {
            std::cout << "Warning: unknown Voronoi edge type" << std::endl;
        }
    }
}
