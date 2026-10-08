// Created by Laura Bülte on 2026-03-20.

#include "pruning/medial_axis_pruning.h"

#include "geometry_utils.h"
#include "io/logger.h"


bool is_connected(MedialAxisWithHoles const &ma)
{
    if (ma.num_edges() + 1 < ma.num_vertices()){
        return false;
    }

    std::vector<bool> visited(ma.num_vertices(), false);
    std::stack<int> to_visit;

    to_visit.push(0);
    while(not to_visit.empty()){
        int current = to_visit.top();
        to_visit.pop();
        visited[current] = true;

        for (auto neighbor : ma.get_adjacency_list()[current]){
            if (not visited[neighbor]){
                to_visit.push(neighbor);
            }
        }
    }
    for (int v = 0; v < ma.num_vertices(); ++v){
        if (not visited[v]){
            return false;
        }
    }
    return true;
}


bool has_hole_or_is_tree(MedialAxisWithHoles const &ma)
{
    if (ma.num_polygon_holes() > 0){
        return true;
    }
    if (is_connected(ma)){
        return ma.num_edges() + 1 == ma.num_vertices();
    }
    return false;
}


bool hole_yields_circle(MedialAxisWithHoles const &ma)
{
    if (ma.num_polygon_holes() == 0){
        return true;
    }
    if (is_connected(ma)){
        return ma.num_polygon_holes() == ma.num_edges() - ma.num_vertices() + 1;
    }
    return false;
}


bool check_validity_of_medial_axis(MedialAxisWithHoles const &ma)
{
    if (not is_connected(ma)){
        log_error(std::cerr, "Medial axis is not connected!");
        return false;
    }
    if (not has_hole_or_is_tree(ma)){
        log_error(std::cerr, "The polygon has no hole and the medial axis is not a tree!");
        return false;
    }
    if (not hole_yields_circle(ma)){
        log_error(std::cerr, "The number of circles does not match the number of holes!");
        return false;
    }
    return true;
}



// Contract connected components of epsilon-close vertices and replace them by their mean point
void contract_adjacent_covertices_from_medial_axis(MedialAxisWithHoles &ma, double distance_threshold)
{
    // Helper to collect all adjacent epsilon-close vertices (idx) for v
    auto collect_adjacent_covertices_of_v = [&](int v){

        std::vector<int> covertices;
        auto n = ma.num_vertices();

        const Point_2& curr = ma.get_vertices()[v];
    
        for (auto const &w : ma.get_adjacency_list()[v]){   
  
            const Point_2& other = ma.get_vertices()[w];

            // We collect copoints if the current has a follwoing covertex
            if (squared_dist(curr, other) < distance_threshold*distance_threshold){
                if (covertices.empty()){
                    covertices.push_back(v);
                }
                covertices.push_back(w);
                continue;
            }
        }
        return covertices;
    };

    // Collect all adjacent covertices 
    std::vector<std::vector<int>> vector_of_covertices;
    for (int v = 0; v < ma.num_vertices(); ++v){
        vector_of_covertices.push_back(collect_adjacent_covertices_of_v(v));
    }

    // Minimal UnionFind implementation
    struct DSU {
        std::vector<int> parent;
        std::vector<int> rank;

        DSU(int n) : parent(n), rank(n, 0) {
            for (int i = 0; i < n; ++i)
                parent[i] = i;
        }

        int find(int x) {
            if (parent[x] != x)
                parent[x] = find(parent[x]); // path compression
            return parent[x];
        }

        void unite(int x, int y) {
            int rootX = find(x);
            int rootY = find(y);

            if (rootX == rootY) return;

            if (rank[rootX] < rank[rootY])
                parent[rootX] = rootY;
            else if (rank[rootX] > rank[rootY])
                parent[rootY] = rootX;
            else {
                parent[rootY] = rootX;
                rank[rootX]++;
            }
        }
    };

    // Build covertex connected components via UnionFind
    DSU dsu(ma.num_vertices());
    for (int v = 0; v < ma.num_vertices(); ++v){
        for (int w : vector_of_covertices[v]){
            dsu.unite(v, w);
        }
    }
    std::unordered_map<int, std::vector<int>> components;
    for (int i = 0; i < ma.num_vertices(); ++i) {
        int root = dsu.find(i);
        components[root].push_back(i);
    }
    // Return if all components are singletons
    if (components.size() == ma.num_vertices()){
        return;
    }
    // Otherwise, build new vertex and edge set with meanPoint for covertices

    std::vector<int> idx_in_new_vertices(ma.num_vertices());

    std::vector<Point_2> new_vertices;
    std::vector<std::pair<int, int>> new_edges;

    for (auto [root, covertices] : components){
        // First handle components containing singletons
        if (covertices.size() == 1){
            idx_in_new_vertices[covertices.front()] = new_vertices.size();
            new_vertices.push_back(ma.get_vertices()[covertices.front()]);
            continue;
        }
        // For covertices, store new idx and push back mean
        for (auto const &v : covertices){
            idx_in_new_vertices[v] = new_vertices.size();
        }
        std::vector<Point_2> group;
        for (int idx : covertices){
            group.push_back(ma.get_vertices()[idx]);
        }        
        new_vertices.push_back(meanPoint(group));
    }

    // Initialize new edge list with new vertex indices
    for (auto const &edge : ma.get_edges()){
        auto new_source = idx_in_new_vertices[edge.first];
        auto new_target = idx_in_new_vertices[edge.second];

        if (new_source == new_target) {
            continue;
        }
        new_edges.emplace_back(new_source, new_target);
    }    
    
    // Update vertices, edges and adjacency list
    ma.set_vertices_and_edges(std::move(new_vertices), std::move(new_edges));
}


// Iteratively prune adjacent collinear vertices
void prune_collinear_vertices_from_medial_axis(MedialAxisWithHoles &ma, double degree_offset) 
{
    auto prune_collinear_vertices_once = [&]() {
        // Collect vertices to contract
        std::vector<bool> contract_vertex(ma.num_vertices(), false);
        for (int v = 0; v < ma.num_vertices(); ++v) {
            if (ma.get_adjacency_list()[v].size() != 2){
                continue;
            }

            int n0 = ma.get_adjacency_list()[v][0];
            int n1 = ma.get_adjacency_list()[v][1];

            // Skip if one of the neighbors was already contracted
            if (contract_vertex[n0] or contract_vertex[n1]){
                continue;
            }

            if (is_nearly_collinear_by_angle(ma.get_vertices()[n0], ma.get_vertices()[v], ma.get_vertices()[n1], degree_offset)){
                contract_vertex[v] = true;
            }
        }

        // Collect uncontracted vertices
        std::vector<int> new_index(ma.num_vertices(), -1);
        std::vector<Point_2> new_vertices;
        for (int i = 0; i < ma.num_vertices(); ++i) {
            if (not contract_vertex[i]) {
                new_index[i] = new_vertices.size();
                new_vertices.push_back(ma.get_vertices()[i]);
            }
        }

        // If no vertices where contracted return false
        if (new_vertices.size() == ma.num_vertices()){
            return false;
        }

        std::set<std::pair<int,int>> new_edges;

        // Contract edges
        for (int v = 0; v < ma.num_vertices(); ++v) {
            if (not contract_vertex[v]) {
                continue;
            }
            int n0 = ma.get_adjacency_list()[v][0];
            int n1 = ma.get_adjacency_list()[v][1];

            int a = new_index[n0];
            int b = new_index[n1];
            if (a > b) std::swap(a, b);
            new_edges.insert({a, b});
        }

        // Copy existing edges that do not involve contracted vertices
        for (auto [u, v] : ma.get_edges()) {
            if (contract_vertex[u] || contract_vertex[v]) continue;
            int a = new_index[u];
            int b = new_index[v];
            if (a > b) std::swap(a, b);
            new_edges.insert({a, b});
        }


        // Convert set to vector
        std::vector<std::pair<int,int>> new_edges_vector(new_edges.begin(), new_edges.end());


        // Update vertices, edges and adjacency list
        ma.set_vertices_and_edges(std::move(new_vertices), std::move(new_edges_vector));
        return true;
    };

    bool changed = false;
    do {
        changed = prune_collinear_vertices_once();
    } while (changed);

}


void prune_medial_axis(MedialAxisWithHoles &ma, MedialAxisPruningConfig const &config)
{
    prune_collinear_vertices_from_medial_axis(ma, config.collinearity_threshold);
    auto dist_threshold = std::min(epsilon_fraction_of_bbox_diag(ma.get_polygon(), config.bbox_relative_covertex_threshold),
                                   config.absolute_covertex_threshold);
    contract_adjacent_covertices_from_medial_axis(ma, dist_threshold);
    prune_collinear_vertices_from_medial_axis(ma, config.collinearity_threshold);
}


