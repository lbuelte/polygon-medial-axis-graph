// Created by philip on 2/18/26.
// Modified by Laura Bülte

#ifndef MEDIALAXISWITHHOLES_H
#define MEDIALAXISWITHHOLES_H

#include "CGAL_includes.h"

#include "io/config.h"
#include "json/json.hpp"

#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

class MedialAxisWithHoles {

public:
    
    MedialAxisWithHoles(Polygon_with_holes const &pwh) 
        : polygon(pwh)
    {}

    void create_medial_axis(Config const &config);

    void set_vertices_and_edges(std::vector<Point_2> vertices, std::vector<std::pair<int,int>> edges); 

    size_t num_vertices() const { return vertices.size(); }
    size_t num_edges() const { return edges.size(); }
    size_t num_polygon_holes() const { return polygon.number_of_holes(); }

    void write_svg_with_holes(const std::string& filename = "medial_axis_holes.svg",
                              double padding = 10, double canvas_size = 1000.0) const;
    void write_svg_polygon_only(const std::string &filename = "polygon_holes.svg",
                                double padding = 10, double canvas_size = 1000.0) const;

    nlohmann::json to_json_simple(int shape_id = -1) const
    {
        nlohmann::json j;

        // ---- Shape ID ----
        if (shape_id >= 0) {
            j["id"] = shape_id;
        }

        // ---- Polygon block ----
        j["polygon"] = nlohmann::json::object();
        // --- Outer boundary ---
        j["polygon"]["outer"] = nlohmann::json::array();
        for (auto vit = polygon.outer_boundary().vertices_begin();
            vit != polygon.outer_boundary().vertices_end(); ++vit)
        {
            j["polygon"]["outer"].push_back({
                {"x", vit->x()},
                {"y", vit->y()}
            });
        }
        // --- Holes ---
        j["polygon"]["holes"] = nlohmann::json::array();
        for (const auto& h : polygon.holes())
        {
            nlohmann::json hole_json = nlohmann::json::array();

            for (auto vit = h.vertices_begin();
                vit != h.vertices_end(); ++vit)
            {
                hole_json.push_back({
                    {"x", vit->x()},
                    {"y", vit->y()}
                });
            }

            j["polygon"]["holes"].push_back(hole_json);
        }

        // ---- Medial axis block ----
        j["medial_axis"] = nlohmann::json::object();
        // --- Vertices ---
        j["medial_axis"]["vertices"] = nlohmann::json::array();
        for (const auto& v : vertices)
        {
            j["medial_axis"]["vertices"].push_back({
                {"x", v.x()},
                {"y", v.y()}
            });
        }
        // --- Edges ---
        j["medial_axis"]["edges"] = nlohmann::json::array();
        for (const auto& e : edges)
        {
            j["medial_axis"]["edges"].push_back({
                {"source", e.first},
                {"target", e.second}
            });
        }

        return j;
    }

    void write_json_simple(const std::string& filename = "polygon_with_holes.json", int shape_id = -1) const
    {
        nlohmann::json j = to_json_simple(shape_id);

        // Write to file
        std::ofstream file(filename);
        file << j.dump(4);  // pretty print
        file.close();

        std::cout << "JSON written to " << filename << std::endl;
    }

    const std::vector<Point_2>& get_vertices() const { return vertices; }
    const std::vector<std::pair<int,int>>& get_edges() const { return edges; }
    const std::vector<std::vector<int>>& get_adjacency_list() const { return adjacency_list; }
    const Polygon_with_holes& get_polygon() const { return polygon; }


private:

    void build_adjacency_list();

    // ---------------- reflex handling ----------------

    void collect_reflex_vertices(std::set<Point_2>& reflex) const;


    // ---------------- geometry helpers ----------------

    void initialize_voronoi();

    void extract_medial_axis(bool use_parabolas);

    const Polygon_with_holes polygon;
    SDG sdg;
    VD vd;

    std::vector<bool> is_curve;
    std::vector<Parabola_segment_2> parabolas;
    std::vector<Point_2> vertices;
    std::vector<std::pair<int,int>> edges;
    std::vector<std::vector<int>> adjacency_list;
    std::vector<std::vector<int>> adjacency_list_edge_association;
};


#endif // MEDIALAXISWITHHOLES_H
