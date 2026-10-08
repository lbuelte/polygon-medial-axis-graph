#include "MedialAxisWithHoles.h"
#include "run_medial_axis.h"

#include "io/logger.h"
#include "io/io.h"
#include "io/config.h"
#include "pruning/polygon_pruning.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>

namespace{

std::map<std::string, std::set<int>> load_faulty_instances(const std::string& path) {
    std::map<std::string, std::set<int>> stuck;
    std::ifstream f(path);
    if (!f.is_open()) return stuck;
    std::string dataset;
    int instance;
    while (f >> dataset >> instance)
        stuck[dataset].insert(instance);
    return stuck;
}

} // end of anonymous namespace


int main(int argc, char *argv[]) {

    if (argc > 2) {
        std::cerr << "Usage: " << argv[0] << " [config.toml]" << std::endl;
        return 1;
    }
    const std::string config_file = (argc == 2) ? argv[1] : "config.toml";
    const Config config = load_config(config_file);

    // load instances where CGAl gets stuck or runs into a segfault.
    // Later, these will be perturbed before run.
    auto stuck_instances = load_faulty_instances(config.io.stuck_instances_file);
    auto segfault_instances = load_faulty_instances(config.io.segfault_instances_file);

    std::filesystem::path global_input_path = config.io.input_path;

    std::filesystem::path global_output_path = config.io.output_path;
    std::filesystem::create_directories(global_output_path);

    auto process_dataset = [&](std::string dataset)
    {
        std::cout << "Processing dataset: " << dataset << std::endl;

        // Stuck and segfaulted instances for this dataset
        const auto &stuck_instances_dataset = stuck_instances[dataset]; // empty set if not present
        const auto &segfault_instances_dataset = segfault_instances[dataset]; // empty set if not present

        std::filesystem::path input_path = global_input_path / dataset;
        std::filesystem::path output_path = global_output_path / dataset;
        std::filesystem::create_directories(output_path);

        std::vector<Polygon_with_holes> polygons_wh;
        std::vector<int> shape_ids;
        read_gpkg_to_polygons_with_holes(input_path / dataset, polygons_wh, shape_ids);

        // Collect the jsons for all medial axis graphs
        nlohmann::json all_graphs;
        all_graphs["graphs"] = nlohmann::json::array();
        for (size_t i = 0; i < polygons_wh.size(); ++i){

            // Write temporary progress file to catch stuck instances
            std::ofstream progress("last_progress.tmp");
            progress << dataset << "|" << i << std::flush;
            progress.close();

            // For logging instance specific errors (e.g. in pruning methods)
            set_log_context(dataset, i);

            auto pwh = &polygons_wh[i];
            nlohmann::json graph_json;

            auto svg_string = [&](){
                if (config.io.generate_plots){
                    std::filesystem::path svg_output_path = output_path / "medial_axis_svgs";
                    std::filesystem::create_directories(svg_output_path);
                    return std::optional<std::string>(svg_output_path.string() + "/" + dataset + "_" + std::to_string(i) + ".svg");
                }
                return std::optional<std::string>(std::nullopt);
            }();

            try {
                if (stuck_instances_dataset.count(i)){
                    graph_json = run_medial_axis_with_holes(config, *pwh, shape_ids[i], svg_string, true);
                }
                else if (segfault_instances_dataset.count(i)){
                    graph_json = run_medial_axis_with_holes(config, *pwh, shape_ids[i], svg_string, true);
                }
                else {
                    graph_json = run_medial_axis_with_holes(config, *pwh, shape_ids[i], svg_string);
                }
            }
            // Skip polygons that cannot be made valid, they get no entry in the output
            catch (InvalidPolygonError const &e) {
                std::cerr << "Warning: skipping instance " << dataset << "|" << i
                          << " (id " << shape_ids[i] << "): " << e.what() << std::endl;
                clear_log_context();
                continue;
            }
            all_graphs["graphs"].push_back(graph_json);
            clear_log_context();

        }
        std::ofstream file(output_path.string() + "/" + dataset + "_graphs.json");
        file << all_graphs.dump(2);
        file.close();
    };

    // If it i set in config, process a single dataset
    if (config.io.dataset.has_value()){
        process_dataset(config.io.dataset.value());
    }   
    // Otherwise process all datasets in the input path
    else {
        int counter = 0;
        for (const auto& entry : std::filesystem::directory_iterator(global_input_path)){
            if (entry.is_directory()) {
                std::string dataset = entry.path().filename().string();
                process_dataset(dataset);
                counter++;
            }
        }
    }
	return 0;
}