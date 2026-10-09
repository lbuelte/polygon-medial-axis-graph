// Created by Laura Bülte on 2026-03-20.

#include "pruning/medial_axis_pruning.h"

#include "geometry_utils.h"

#include <iostream>
#include <set>
#include <stack>
#include <unordered_map>


bool is_connected(MedialAxisGraph const &ma)
{
    if (ma.num_nodes() == 0){
        return false;
    }
    // A connected graph with n nodes has at least n - 1 edges
    if (ma.num_edges() + 1 < ma.num_nodes()){
        return false;
    }

    // Depth-first search from node 0
    std::vector<bool> visited(ma.num_nodes(), false);
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
    for (size_t v = 0; v < ma.num_nodes(); ++v){
        if (not visited[v]){
            return false;
        }
    }
    return true;
}


bool has_hole_or_is_tree(MedialAxisGraph const &ma)
{
    if (ma.num_polygon_holes() > 0){
        return true;
    }
    if (is_connected(ma)){
        return ma.num_edges() + 1 == ma.num_nodes();
    }
    return false;
}


bool holes_yield_cycles(MedialAxisGraph const &ma)
{
    if (ma.num_polygon_holes() == 0){
        return true;
    }
    // A connected graph has edges - nodes + 1 independent cycles
    if (is_connected(ma)){
        return ma.num_polygon_holes() + ma.num_nodes() == ma.num_edges() + 1;
    }
    return false;
}


bool check_validity_of_medial_axis(MedialAxisGraph const &ma, std::string *reason)
{
    // Store the problem in reason (if given) instead of printing it, since this check runs repeatedly
    auto report = [&](const std::string &message) {
        if (reason) *reason = message;
    };

    if (not is_connected(ma)){
        report("medial axis graph is not connected");
        return false;
    }
    if (not has_hole_or_is_tree(ma)){
        report("polygon has no hole but the medial axis graph is not a tree");
        return false;
    }
    if (not holes_yield_cycles(ma)){
        report("number of cycles does not match the number of holes");
        return false;
    }
    return true;
}



// Contract connected components of epsilon-close nodes and replace them by their mean point
void merge_close_adjacent_nodes(MedialAxisGraph &ma, double distance_threshold)
{
    // Helper to collect all adjacent epsilon-close nodes (idx) for v
    auto collect_adjacent_close_nodes_of_v = [&](int v){

        std::vector<int> close_nodes;
        const Point_2& curr = ma.get_nodes()[v];
    
        for (auto const &w : ma.get_adjacency_list()[v]){   
  
            const Point_2& other = ma.get_nodes()[w];

            // We collect close nodes if the current node has an adjacent close node
            if (squared_dist(curr, other) < distance_threshold*distance_threshold){
                if (close_nodes.empty()){
                    close_nodes.push_back(v);
                }
                close_nodes.push_back(w);
            }
        }
        return close_nodes;
    };

    // Collect all adjacent close nodes
    std::vector<std::vector<int>> vector_of_close_nodes;
    for (size_t v = 0; v < ma.num_nodes(); ++v){
        vector_of_close_nodes.push_back(collect_adjacent_close_nodes_of_v(v));
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

    // Build connected components of close nodes via UnionFind
    DSU dsu(ma.num_nodes());
    for (size_t v = 0; v < ma.num_nodes(); ++v){
        for (int w : vector_of_close_nodes[v]){
            dsu.unite(v, w);
        }
    }
    std::unordered_map<int, std::vector<int>> components;
    for (size_t i = 0; i < ma.num_nodes(); ++i) {
        int root = dsu.find(i);
        components[root].push_back(i);
    }
    // Return if all components are singletons
    if (components.size() == ma.num_nodes()){
        return;
    }
    // Otherwise, build new node and edge set with the mean point for close nodes

    std::vector<int> idx_in_new_nodes(ma.num_nodes());

    std::vector<Point_2> new_nodes;
    std::vector<std::pair<int, int>> new_edges;

    for (auto [root, close_nodes] : components){
        // First handle components containing singletons
        if (close_nodes.size() == 1){
            idx_in_new_nodes[close_nodes.front()] = new_nodes.size();
            new_nodes.push_back(ma.get_nodes()[close_nodes.front()]);
            continue;
        }
        // For close nodes, store new idx and push back mean
        for (auto const &v : close_nodes){
            idx_in_new_nodes[v] = new_nodes.size();
        }
        std::vector<Point_2> group;
        for (int idx : close_nodes){
            group.push_back(ma.get_nodes()[idx]);
        }        
        new_nodes.push_back(mean_point(group));
    }

    // Initialize new edge list with new node indices
    for (auto const &edge : ma.get_edges()){
        auto new_source = idx_in_new_nodes[edge.first];
        auto new_target = idx_in_new_nodes[edge.second];

        if (new_source == new_target) {
            continue;
        }
        new_edges.emplace_back(new_source, new_target);
    }    
    
    // Update nodes, edges and adjacency list
    ma.set_nodes_and_edges(std::move(new_nodes), std::move(new_edges));
}


// Iteratively prune adjacent collinear nodes
void remove_nearly_collinear_nodes(MedialAxisGraph &ma, double degree_offset) 
{
    auto remove_nearly_collinear_nodes_once = [&]() {
        // Collect nodes to contract
        std::vector<bool> contract_node(ma.num_nodes(), false);
        for (size_t v = 0; v < ma.num_nodes(); ++v) {
            if (ma.get_adjacency_list()[v].size() != 2){
                continue;
            }

            int n0 = ma.get_adjacency_list()[v][0];
            int n1 = ma.get_adjacency_list()[v][1];

            // Skip if one of the neighbors was already contracted
            if (contract_node[n0] or contract_node[n1]){
                continue;
            }

            if (is_nearly_collinear_by_angle(ma.get_nodes()[n0], ma.get_nodes()[v], ma.get_nodes()[n1], degree_offset)){
                contract_node[v] = true;
            }
        }

        // Collect uncontracted nodes
        std::vector<int> new_index(ma.num_nodes(), -1);
        std::vector<Point_2> new_nodes;
        for (size_t i = 0; i < ma.num_nodes(); ++i) {
            if (not contract_node[i]) {
                new_index[i] = new_nodes.size();
                new_nodes.push_back(ma.get_nodes()[i]);
            }
        }

        // If no nodes were contracted return false
        if (new_nodes.size() == ma.num_nodes()){
            return false;
        }

        std::set<std::pair<int,int>> new_edges;

        // Contract edges
        for (size_t v = 0; v < ma.num_nodes(); ++v) {
            if (not contract_node[v]) {
                continue;
            }
            int n0 = ma.get_adjacency_list()[v][0];
            int n1 = ma.get_adjacency_list()[v][1];

            int a = new_index[n0];
            int b = new_index[n1];
            if (a > b) std::swap(a, b);
            new_edges.insert({a, b});
        }

        // Copy existing edges that do not involve contracted nodes
        for (auto [u, v] : ma.get_edges()) {
            if (contract_node[u] || contract_node[v]) continue;
            int a = new_index[u];
            int b = new_index[v];
            if (a > b) std::swap(a, b);
            new_edges.insert({a, b});
        }


        // Convert set to vector
        std::vector<std::pair<int,int>> new_edges_vector(new_edges.begin(), new_edges.end());


        // Update nodes, edges and adjacency list
        ma.set_nodes_and_edges(std::move(new_nodes), std::move(new_edges_vector));
        return true;
    };

    bool changed = false;
    do {
        changed = remove_nearly_collinear_nodes_once();
    } while (changed);

}


void prune_medial_axis(MedialAxisGraph &ma, MedialAxisPruningParams const &pruning)
{
    remove_nearly_collinear_nodes(ma, pruning.collinearity_threshold_deg);
    auto dist_threshold = std::min(epsilon_fraction_of_bbox_diag(ma.get_polygon(), pruning.merge_distance_rel_bbox),
                                   pruning.merge_distance);
    merge_close_adjacent_nodes(ma, dist_threshold);
    remove_nearly_collinear_nodes(ma, pruning.collinearity_threshold_deg);
}


