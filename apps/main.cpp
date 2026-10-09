// Command line tool: computes the medial axis graphs of all polygons of one or more datasets
// and writes them to <output>/<dataset>/<dataset>_graphs.json (and optionally SVGs).
// Run with --help for the usage.

#include "medial_axis_graph.h"
#include "params.h"
#include "pipeline.h"

#include "io/logger.h"
#include "io/io.h"
#include "pruning/polygon_pruning.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace{

const char *usage = R"(Usage: medial_axis_graph --input DIR --output DIR [options]

Computes the medial axis graph of every polygon in <DIR>/<dataset>/<dataset>.gpkg
and writes <output>/<dataset>/<dataset>_graphs.json.
Prints one summary line per dataset; warnings (e.g. skipped polygons) go to stderr.

Required:
  --input DIR                  folder containing one subfolder per dataset
  --output DIR                 folder for the results (created if needed)

Options:
  --dataset NAME               process only this dataset (can be repeated);
                               default: every subfolder of the input folder
  --params FILE                algorithm parameters (TOML); default: built-in defaults
  --perturb-instances FILE     perturb the listed instances ("<dataset> <index>" per line)
                               before computing them (can be repeated, see scripts/watchdog.sh)
  --no-svg                     do not write SVG drawings
  --svg-parabolas              draw parabolic medial axis edges as curves in the SVGs
  --verbose                    also print a message for every file written
  --print-default-params       print all parameters with their defaults and exit
  --help                       print this help and exit
)";

// Wrong command line usage
struct UsageError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Settings of one run, given on the command line
struct Options {
    std::filesystem::path input_path;
    std::filesystem::path output_path;
    std::vector<std::string> datasets;                  // empty: all subfolders of input_path
    std::optional<std::string> params_file;
    std::vector<std::string> perturb_instance_files;
    bool write_svgs = true;
    bool svg_parabolas = false;
    bool verbose = false;
    bool print_default_params = false;
    bool help = false;
};

Options parse_options(int argc, char *argv[])
{
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        // Helper that returns the value following an option
        auto value = [&]() -> std::string {
            if (i + 1 >= argc) {
                throw UsageError(arg + " needs a value");
            }
            return argv[++i];
        };

        if (arg == "--input")                       options.input_path = value();
        else if (arg == "--output")                 options.output_path = value();
        else if (arg == "--dataset")                options.datasets.push_back(value());
        else if (arg == "--params")                 options.params_file = value();
        else if (arg == "--perturb-instances")      options.perturb_instance_files.push_back(value());
        else if (arg == "--no-svg")                 options.write_svgs = false;
        else if (arg == "--svg-parabolas")          options.svg_parabolas = true;
        else if (arg == "--verbose")                options.verbose = true;
        else if (arg == "--print-default-params")   options.print_default_params = true;
        else if (arg == "--help" or arg == "-h")    options.help = true;
        else throw UsageError("unknown argument " + arg);
    }
    if (not options.help and not options.print_default_params) {
        if (options.input_path.empty())  throw UsageError("--input is required");
        if (options.output_path.empty()) throw UsageError("--output is required");
    }
    return options;
}

// Reads lists of instances with one "<dataset> <index>" per line, where index is the
// position of the polygon in the dataset (not its id). Returns dataset -> indices.
std::map<std::string, std::set<size_t>> load_instance_lists(const std::vector<std::string> &paths)
{
    std::map<std::string, std::set<size_t>> instances;
    for (const auto &path : paths) {
        std::ifstream f(path);
        if (!f.is_open()) {
            throw UsageError("could not open instance list " + path);
        }
        std::string dataset;
        size_t index;
        while (f >> dataset >> index)
            instances[dataset].insert(index);
    }
    return instances;
}

// Computes and writes the medial axis graphs of one dataset
void process_dataset(const std::string &dataset, const Options &options, const Params &params,
                     const std::set<size_t> &perturb_instances)
{
    std::filesystem::path output_path = options.output_path / dataset;
    std::filesystem::create_directories(output_path);

    std::vector<Polygon_with_holes> polygons;
    std::vector<int> shape_ids;
    set_log_context(dataset);
    size_t num_skipped = read_polygons_from_gpkg(options.input_path / dataset / (dataset + ".gpkg"),
                                                 polygons, shape_ids);

    // Collect the jsons for all medial axis graphs
    nlohmann::json all_graphs;
    all_graphs["graphs"] = nlohmann::json::array();
    for (size_t i = 0; i < polygons.size(); ++i){

        // Write temporary progress file, so that scripts/watchdog.sh can detect stuck instances
        std::ofstream progress("last_progress.tmp");
        progress << dataset << "|" << i << std::flush;
        progress.close();

        // Messages from inside the pipeline name the instance: dataset|index id=...
        set_log_context(dataset + "|" + std::to_string(i) + " id=" + std::to_string(shape_ids[i]));

        bool perturb_immediately = perturb_instances.count(i) > 0;
        try {
            MedialAxisGraph graph = compute_medial_axis_graph(polygons[i], params, perturb_immediately,
                                                              options.svg_parabolas);
            if (options.write_svgs){
                std::filesystem::path svg_output_path = output_path / "medial_axis_svgs";
                std::filesystem::create_directories(svg_output_path);
                graph.write_svg((svg_output_path / (dataset + "_" + std::to_string(i) + ".svg")).string());
            }
            all_graphs["graphs"].push_back(graph.to_json(shape_ids[i]));
        }
        // Skip polygons that cannot be made valid, they get no entry in the output
        catch (InvalidPolygonError const &e) {
            log_warning(std::string("skipping polygon: ") + e.what());
            ++num_skipped;
        }
    }
    clear_log_context();
    std::ofstream file(output_path / (dataset + "_graphs.json"));
    file << all_graphs.dump(2);
    file.close();

    // Record the parameters used, so that the results can be reproduced
    std::ofstream params_file(output_path / "params_used.toml");
    write_params(params, params_file);

    std::cout << dataset << ": " << all_graphs["graphs"].size() << " graphs written, "
              << num_skipped << " polygons skipped" << std::endl;
}

} // end of anonymous namespace


int main(int argc, char *argv[]) {

    // ---- command line and parameters ----
    Options options;
    Params params;
    std::map<std::string, std::set<size_t>> perturb_instances;
    try {
        options = parse_options(argc, argv);
        if (options.help) {
            std::cout << usage;
            return 0;
        }
        if (options.print_default_params) {
            write_params(Params{}, std::cout);
            return 0;
        }
        if (options.params_file) {
            params = load_params(*options.params_file);
        }
        perturb_instances = load_instance_lists(options.perturb_instance_files);
        set_verbose(options.verbose);
    }
    catch (UsageError const &e) {
        std::cerr << "Error: " << e.what() << "\n\n" << usage;
        return 2;
    }
    catch (ParamsError const &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 2;
    }

    // ---- datasets: given ones, or all subfolders of the input folder in alphabetical order ----
    try {
        std::vector<std::string> datasets = options.datasets;
        if (datasets.empty()) {
            for (const auto& entry : std::filesystem::directory_iterator(options.input_path)){
                if (entry.is_directory()) {
                    datasets.push_back(entry.path().filename().string());
                }
            }
            std::sort(datasets.begin(), datasets.end());
        }

        std::filesystem::create_directories(options.output_path);
        for (const auto &dataset : datasets) {
            process_dataset(dataset, options, params, perturb_instances[dataset]);
        }
    }
    catch (std::exception const &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
