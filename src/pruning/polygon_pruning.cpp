// Created by Laura Bülte on 2026-03-20.

#include "pruning/polygon_pruning.h"

#include "geometry_utils.h"
#include "io/logger.h"

#include <cassert>
#include <iostream>
#include <random>


bool check_validity_of_polygon(Polygon_with_holes const &polygon)
{
    constexpr double epsilon = 1e-4;
    constexpr double angle_in_degree_for_colinearity = 2;
    const double eps2 = epsilon * epsilon;

    if (polygon.outer_boundary().size() < 3) {
        log_error(std::cerr, "Outer boundary has less than 3 vertices!");
        return false;
    }

    auto norm = [](const Point_2& a, const Point_2& b) {
        return (a < b) ? std::make_pair(a,b)
                       : std::make_pair(b,a);
    };

    auto sqdist = [](const Point_2& a, const Point_2& b) {
        const double dx = CGAL::to_double(a.x()) - CGAL::to_double(b.x());
        const double dy = CGAL::to_double(a.y()) - CGAL::to_double(b.y());
        return dx*dx + dy*dy;
    };

    auto check_near_collinear = [&](const auto& poly, const std::string& name)
    {
        if (poly.size() < 3)
            return false;

        auto circ = poly.vertices_circulator();
        auto start = circ;

        do {
            auto prev = circ;
            auto curr = circ;
            auto next = circ;

            --prev;
            ++next;

            if (is_nearly_collinear_by_angle(*prev, *curr, *next,
                                             angle_in_degree_for_colinearity))
            {
                log_error(std::cerr, name + " has nearly collinear consecutive vertices!");
                return false;
            }

            ++circ;
        } while (circ != start);

        return true;
    };

    const auto& outer = polygon.outer_boundary();

    // ---- Orientation outer ----
    if (!outer.is_counterclockwise_oriented()) {
        log_error(std::cerr,"Outer boundary is not CCW oriented!");
        return false;
    }


    // ---- Outer: duplicate edges + zero-length ----
    {
        std::vector<std::pair<Point_2,Point_2>> seen_edges;
        for (auto e : outer.edges()) {

            if (e.source() == e.target()) {
                log_error(std::cerr,"Outer boundary has zero-length edge!");
                return false;
            }

            auto key = norm(e.source(), e.target());
            for (const auto& s : seen_edges)
                if (s.first == key.first && s.second == key.second) {
                    log_error(std::cerr,"Outer boundary has duplicate edge!");
                    return false;
                }

            seen_edges.push_back(key);
        }
    }

    // ---- Outer: duplicate vertices (epsilon) ----
    {
        std::vector<Point_2> seen;
        for (auto v : outer.vertices()) {
            for (const auto& s : seen)
                if (sqdist(v, s) < eps2) {
                    log_error(std::cerr,"Outer boundary has duplicate vertex (epsilon)!");
                    return false;
                }
            seen.push_back(v);
        }
    }

    if (!outer.is_simple()) {
        log_error(std::cerr,"Outer boundary is not simple!");
        std::cerr << "Outer boundary is not simple\n";
        return false;
    }
    // ---- Outer: consecutive checks ----
    {
        auto it = outer.vertices_begin();
        auto prev = it++;
        for (; it != outer.vertices_end(); ++it) {

            if (*prev == *it) {
                log_error(std::cerr,"Outer boundary has identical consecutive vertices (exact)!");
                return false;
            }

            if (sqdist(*prev, *it) < eps2) {
                log_error(std::cerr,"Outer boundary has consecutive vertices closer than epsilon!");
                return false;
            }

            prev = it;
        }

        if (*prev == *outer.vertices_begin()) {
            log_error(std::cerr,"Outer boundary closing edge identical vertices (exact)!");
            return false;
        }

        if (sqdist(*prev, *outer.vertices_begin()) < eps2) {
            log_error(std::cerr,"Outer boundary closing edge vertices closer than epsilon!");
            return false;
        }
    }

    if (!check_near_collinear(outer, "Outer boundary"))
        return false;

    // ---- Global duplicate check (outer + holes) ----
    std::vector<Point_2> global_seen;

    auto check_and_insert_global = [&](const Point_2& v)
    {
        for (const auto& s : global_seen)
        {
            if (v == s) {
                log_error(std::cerr,"Global duplicate vertex detected (exact)!");
                return false;
            }

            if (sqdist(v, s) < eps2) {
                log_error(std::cerr,"Global duplicate vertex detected (epsilon)!");
                return false;
            }
        }

        global_seen.push_back(v);
        return true;
    };

    for (auto v : outer.vertices())
        if (!check_and_insert_global(v))
            return false;

    // ---- Holes ----
    for (const auto& hole : polygon.holes())
    {
        if (hole.size() < 3) {
            log_error(std::cerr,"Hole has less than 3 vertices!");
            return false;
        }

        if (!hole.is_clockwise_oriented()) {
            log_error(std::cerr,"Hole is not CW oriented!");
            return false;
        }

        // duplicate vertices (epsilon)
        {
            std::vector<Point_2> seen;
            for (auto v : hole.vertices()) {
                for (const auto& s : seen)
                    if (sqdist(v, s) < eps2) {
                        log_error(std::cerr,"Hole has duplicate vertex (epsilon)!");
                        return false;
                    }
                seen.push_back(v);
            }
        }

        // duplicate edges + zero-length
        {
            std::vector<std::pair<Point_2,Point_2>> seen_edges;
            for (auto e : hole.edges()) {

                if (e.source() == e.target()) {
                    log_error(std::cerr,"Hole has zero-length edge!");
                    return false;
                }

                auto key = norm(e.source(), e.target());
                for (const auto& s : seen_edges)
                    if (s.first == key.first && s.second == key.second) {
                        log_error(std::cerr,"Hole has duplicate edge!");
                        return false;
                    }

                seen_edges.push_back(key);
            }
        }

        if (!hole.is_simple()) {
            log_error(std::cerr,"Hole is not simple!");
            return false;
        }

        // consecutive checks
        {
            auto it = hole.vertices_begin();
            auto prev = it++;
            for (; it != hole.vertices_end(); ++it) {

                if (*prev == *it) {
                    log_error(std::cerr,"Hole has identical consecutive vertices (exact)!");
                    return false;
                }

                if (sqdist(*prev, *it) < eps2) {
                    log_error(std::cerr,"Hole has consecutive vertices closer than epsilon!");
                    return false;
                }

                prev = it;
            }

            if (*prev == *hole.vertices_begin()) {
                log_error(std::cerr,"Hole closing edge identical vertices (exact)!");
                return false;
            }

            if (sqdist(*prev, *hole.vertices_begin()) < eps2) {
                log_error(std::cerr,"Hole closing edge vertices closer than epsilon!");
                return false;
            }
        }

        if (!check_near_collinear(hole, "Hole"))
            return false;

        // global check
        for (auto v : hole.vertices())
            if (!check_and_insert_global(v))
                return false;
    }

    return true;
}


// Reorient boundary and holes in the canonical way, 
// i.e., boundary is oriented counterclockwise, holes are oriented clockwise
void normalize_polygon_orientation(Polygon_with_holes& pwh)
{
    Polygon outer = pwh.outer_boundary();
    if (outer.is_clockwise_oriented())
        outer.reverse_orientation();

    std::vector<Polygon> holes;
    holes.reserve(std::distance(pwh.holes_begin(), pwh.holes_end()));

    for (auto hit = pwh.holes_begin();
         hit != pwh.holes_end(); ++hit)
    {
        Polygon hole = *hit;
        if (hole.is_counterclockwise_oriented())
            hole.reverse_orientation();
        holes.push_back(std::move(hole));
    }

    // Reassign the normalized polygon
    pwh = Polygon_with_holes(outer, holes.begin(), holes.end());

    // Assertions
    assert(pwh.outer_boundary().is_counterclockwise_oriented());
    for (auto hit = pwh.holes_begin();
         hit != pwh.holes_end(); ++hit)
        assert(hit->is_clockwise_oriented());
}


// Contract consecutive vertices that are epsilon-close and replace them by their mean point
void contract_covertices_from_polygon(Polygon_with_holes &polygon, double distance_threshold)
{
    // Helper to contract covertices of a given point vector
    auto contract_covertices = [&](std::vector<Point_2> const &poly_vertices){

        std::vector<Point_2> output_vertices;
        std::vector<Point_2> copoints;
        auto n = poly_vertices.size();
    
        for (int v = 0; v < n; ++v){
            const Point_2& curr = poly_vertices[v];
            const Point_2& next = poly_vertices[(v + 1) % n];

            // We collect copoints if the current has a follwoing covertex
            if (squared_dist(curr, next) < distance_threshold*distance_threshold){
                if (copoints.empty()){
                    copoints.push_back(curr);
                }
                copoints.push_back(next);
                continue;
            }

            // If the current vertex is unique, we add it
            else if (copoints.empty()){
                output_vertices.push_back(curr);
                continue;
            }

            // If the next vertex is unique, but the current a copoint, 
            // we add the mean of previous copoints and clear
            output_vertices.push_back(meanPoint(copoints));
            copoints.clear(); 
        }
        return output_vertices;
    };

    // Contract covertices of boundary and holes
    auto boundary_vs = contract_covertices(polygon.outer_boundary().vertices());
    Polygon new_boundary(boundary_vs.begin(), boundary_vs.end());

    std::vector<Polygon> new_holes;
    for (auto const &hole : polygon.holes()){
        auto hole_vs = contract_covertices(hole.vertices());
        new_holes.push_back(Polygon(hole_vs.begin(), hole_vs.end()));
    }

    // Update the polygon
    polygon = Polygon_with_holes(
        new_boundary,
        new_holes.begin(),
        new_holes.end());
}


// Iteratively contract consecutive collinear vertices
void prune_collinear_vertices_from_polygon(Polygon_with_holes &polygon, double degree_offset)
{
    // Helper to collect collinear points
    auto collect_collinear = [&](std::vector<Point_2> const &poly_vertices){
        auto n = poly_vertices.size();
        std::vector<bool> is_collinear(n, false);

        for (int v = 0; v < n; ++v){
            const Point_2& prev = poly_vertices[(v - 1 + n) % n];
            const Point_2& curr = poly_vertices[v];
            const Point_2& next = poly_vertices[(v + 1) % n];

            if (is_collinear[(v - 1 + n) % n]){                
                continue;
            }
            if (is_nearly_collinear_by_angle(prev, curr, next, degree_offset)){
                is_collinear[v] = true;
            }
        }
        return is_collinear;
    };

    // Helper to prune collinear points from a polygon once
    auto prune_collinear_once = [&](Polygon &poly){
        auto n = poly.vertices().size();
        std::vector<bool> is_collinear = collect_collinear(poly.vertices());

        // Collect uncontracted vertices
        std::vector<Point_2> new_vertices;
        for (int v = 0; v < n; ++v) {
            if (not is_collinear[v]) {
                new_vertices.push_back(poly.vertices()[v]);
            }
        }
        // If no vertices where contracted return false
        if (new_vertices.size() == n){
            return false;
        }
        // Update polygon
        poly = Polygon(new_vertices.begin(), new_vertices.end());
        return true;
    };

    // Helper to iteratively prune collinear points until nothing changes
    auto prune_collinear_iterative = [&](Polygon &poly){
        bool changed = false;
        do {
            changed = prune_collinear_once(poly);
        } while (changed);
    };

    // Prune the boundary and the holes
    prune_collinear_iterative(polygon.outer_boundary());
    for (auto &hole : polygon.holes()){
        prune_collinear_iterative(hole);
    }
}


// If boundary and hole share a point, we move the hole point away from the 
// boundary by some epsilon along the angle bisector of the hole corner
void prune_hole_boundary_intersections(Polygon_with_holes &polygon)
{
    if (not polygon.has_holes()){
        return;
    }

    struct IntersectionPoint {
            int boundary_point;
            int hole_point;
        };

    // Helper to find intersection points if they exist
    auto collect_hole_boundary_intersection = [&](Polygon const &hole){       
        
        std::vector<IntersectionPoint> intersections;
        for (int v = 0; v < polygon.outer_boundary().size(); ++v){
            for (int w = 0; w < hole.size(); ++w){
                //TODO: maybe make this more numerically robust
                if (polygon.outer_boundary().vertices()[v] == hole.vertices()[w]){
                    intersections.push_back({v, w});
                }
            }
        }
        return intersections;
    };

    // Helper that returns point that is epsilon away (along angle bisector)
    // from the corner prev - curr - next (and in direction away from outer)
    auto epsilon_along_angle_bisector = [](
            Point_2 const &prev, 
            Point_2 const &curr, 
            Point_2 const &next, 
            Point_2 const &outer, 
            double epsilon){

        // Edge vectors
        Vector_2 v1 = prev - curr;
        Vector_2 v2 = next - curr;

        // Normalize v1
        double len1 = std::sqrt(v1.squared_length());
        if (len1 == 0.0) return curr;
        v1 = v1 / len1;

        // Normalize v2
        double len2 = std::sqrt(v2.squared_length());
        if (len2 == 0.0) return curr;
        v2 = v2 / len2;

        // Angle bisector direction
        Vector_2 bis = v1 + v2;
        double len_bis = std::sqrt(bis.squared_length());
        if (len_bis == 0.0) return curr;  // Straight angle case
        bis = bis / len_bis;

        // Ensure bisector points away from outer
        Vector_2 away = curr - outer;
        if (bis * away < 0)  // dot product
            bis = -bis;

        return curr + epsilon * bis;
    };

    // Helper that returns point that is curr moved by epsilon towrds outer
    auto epsilon_towards_outer = [](Point_2 const &curr, Point_2 const &outer, double epsilon){
        Vector_2 dir = outer - curr;

        double len = std::sqrt(dir.squared_length());
        if (len == 0.0)
            return curr;  // points coincide

        dir = dir / len;

        return curr + epsilon * dir;
    };

    // Helper that moved point of hole (given by idx) to new_point
    auto move_hole_point = [](Polygon &hole, int point_idx, Point_2 new_point){
        auto it = hole.vertices_begin();
        std::advance(it, point_idx);
        *it = new_point;      
    };

    // Helper that moves an intersection point of a hole away from the intersected boundary point
    auto move_hole_point_away_from_intersection = [&](Polygon &hole, IntersectionPoint const &intersection){
        auto n = hole.size();
        auto prev = hole.vertex((intersection.hole_point - 1 + n) % n);
        auto curr = hole.vertex(intersection.hole_point);
        auto next = hole.vertex((intersection.hole_point + 1) % n);
        auto outer = polygon.outer_boundary().vertex(intersection.boundary_point);

        // Move point 
        double epsilon = 0.01;
        auto new_point = epsilon_along_angle_bisector(prev, curr, next, outer, epsilon);
        move_hole_point(hole, intersection.hole_point, new_point);

        while (not check_validity_of_polygon(polygon)) {
            epsilon = epsilon / 2.0;
            auto new_point = epsilon_towards_outer(curr, outer, epsilon);
            move_hole_point(hole, intersection.hole_point, new_point);
        } 
    };

    // Clear up all hole - boundary intersections
    for (auto &hole : polygon.holes()){
        auto intersections = collect_hole_boundary_intersection(hole);
        for (auto const &intersection : intersections){
            move_hole_point_away_from_intersection(hole, intersection);
        }        
    }
}


void prune_polygon(Polygon_with_holes &polygon, PolygonPruningConfig const &config)
{
    prune_collinear_vertices_from_polygon(polygon, config.collinearity_threshold);
    auto dist_threshold = std::min(epsilon_fraction_of_bbox_diag(polygon, config.bbox_relative_covertex_threshold), 
                                   config.absolute_covertex_threshold);
    contract_covertices_from_polygon(polygon, dist_threshold);    
    prune_collinear_vertices_from_polygon(polygon, config.collinearity_threshold);

    prune_hole_boundary_intersections(polygon);
}


// Perturb all polygon vertices by an epsilon relative to bbox diagonal
void perturb_polygon(Polygon_with_holes &polygon, double relative_eps, unsigned seed)
{
   double eps = epsilon_fraction_of_bbox_diag(polygon, relative_eps);
    
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> dist(-eps, eps);

    // Outer boundary
    for (auto vit = polygon.outer_boundary().vertices_begin();
         vit != polygon.outer_boundary().vertices_end(); ++vit)
    {
        *vit = Point_2(vit->x() + dist(gen),
                       vit->y() + dist(gen));
    }

    // Holes
    for (auto& hole : polygon.holes())
    {
        for (auto vit = hole.vertices_begin();
             vit != hole.vertices_end(); ++vit)
        {
            *vit = Point_2(vit->x() + dist(gen),
                           vit->y() + dist(gen));
        }
    }
}

