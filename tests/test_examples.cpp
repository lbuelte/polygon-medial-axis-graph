// Checks that the medial axis graphs of the example polygons (computed with the default
// parameters) are connected and simple (no loops, no parallel edges, no isolated nodes).
// Run from the repository root (ctest does this automatically).

#include "medial_axis_graph.h"
#include "params.h"
#include "pipeline.h"

#include "io/io.h"
#include "pruning/medial_axis_pruning.h"

#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {

// Returns an empty string if the graph is connected and simple, otherwise a description of the problem
std::string check_graph(MedialAxisGraph const &graph)
{
    if (graph.num_nodes() == 0) {
        return "graph has no nodes";
    }

    std::set<std::pair<int,int>> seen_edges;
    for (auto [u, v] : graph.get_edges()) {
        if (u == v) {
            return "graph has a loop at node " + std::to_string(u);
        }
        if (not seen_edges.insert({std::min(u, v), std::max(u, v)}).second) {
            return "graph has parallel edges between nodes " + std::to_string(u) + " and " + std::to_string(v);
        }
    }

    // Connectivity also rules out isolated nodes, since the graph has more than one node
    if (not is_connected(graph)) {
        return "graph is not connected";
    }
    return "";
}

} // end of anonymous namespace


int main()
{
    const Params params;

    int num_failures = 0;
    for (std::string dataset : {"synthetic", "osm_poppelsdorf"}) {
        std::vector<Polygon_with_holes> polygons;
        std::vector<int> shape_ids;
        read_polygons_from_gpkg("examples/data/" + dataset + "/" + dataset + ".gpkg", polygons, shape_ids);

        if (polygons.empty()) {
            std::cerr << "FAIL " << dataset << ": no polygons read" << std::endl;
            ++num_failures;
            continue;
        }

        for (size_t i = 0; i < polygons.size(); ++i) {
            MedialAxisGraph graph = compute_medial_axis_graph(polygons[i], params);
            std::string problem = check_graph(graph);
            if (not problem.empty()) {
                std::cerr << "FAIL " << dataset << " id " << shape_ids[i] << ": " << problem << std::endl;
                ++num_failures;
            }
        }
        std::cout << dataset << ": checked " << polygons.size() << " graphs" << std::endl;
    }

    if (num_failures > 0) {
        std::cerr << num_failures << " graph(s) failed" << std::endl;
        return 1;
    }
    std::cout << "All graphs are connected and simple" << std::endl;
    return 0;
}
