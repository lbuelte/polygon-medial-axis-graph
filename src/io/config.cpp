// Created by Laura Bülte on 2026-03-20.

#include "io/config.h"

#include "tomlplusplus/include/toml++/toml.h"
#include <stdexcept>
#include <fstream>
#include <ostream>

namespace {

template<typename T>
void read_required(const toml::table& t, const char* key, T& out)
{
    if (auto v = t[key].value<T>())
        out = *v;
    else
        throw std::runtime_error(
            std::string("Missing required key: ") + key);
}

template<typename T>
void read_value_if(const toml::table& t, const char* key, T& out)
{
    if (auto v = t[key].value<T>())
        out = *v;
    // else: do nothing → keep struct default
}

template<typename T>
void read_optional_if(const toml::table& t, const char* key, std::optional<T>& out)
{
    if (auto v = t[key].value<T>())
        out = *v;
    // else: remains std::nullopt
}

template<typename Enum, typename Parser>
void read_enum_if(const toml::table& t,
                  const char* key,
                  Enum& out,
                  Parser parse_fn)
{
    if (auto s = t[key].value<std::string>())
        out = parse_fn(*s);
}

// Helper to print booleans as true/false
const char* tf(bool b) { return b ? "true" : "false"; }

} // end of anonymous namespace



Config load_config(const std::string& filename)
{
    toml::table tbl;

    try {
        tbl = toml::parse_file(filename);
    }    
    catch (const toml::parse_error& err) {
        throw std::runtime_error(
            std::string("Failed to parse config file: ") +
            std::string(err.description()));
    }

    Config cfg;

    // =========================
    // IO
    // =========================
    const auto* io_tbl = tbl["IO"].as_table();
    if (!io_tbl)
        throw std::runtime_error("Missing [IO] section");
    
    read_required(*io_tbl, "input_path", cfg.io.input_path);
    read_required(*io_tbl, "output_path", cfg.io.output_path);
    read_optional_if(*io_tbl, "dataset", cfg.io.dataset);
    read_optional_if(*io_tbl, "error_log_file", cfg.io.error_log_file);
    read_value_if(*io_tbl, "generate_plots", cfg.io.generate_plots);


    // =========================
    // Polygon Pruning
    // =========================
    const auto* pp_tbl = tbl["PolygonPruning"].as_table();
    if (!pp_tbl)
        throw std::runtime_error("Missing [PolygonPruning] section");

    read_value_if(*pp_tbl, "collinearity_threshold",
                  cfg.polyprune_init.collinearity_threshold);

    read_value_if(*pp_tbl, "absolute_covertex_threshold",
                  cfg.polyprune_init.absolute_covertex_threshold);

    read_value_if(*pp_tbl, "bbox_relative_covertex_threshold",
                  cfg.polyprune_init.bbox_relative_covertex_threshold);


    // =========================
    // Polygon Perturbation
    // =========================
    const auto* pert_tbl = tbl["PolygonPerturbation"].as_table();
    if (!pert_tbl)
        throw std::runtime_error("Missing [PolygonPerturbation] section");

    read_value_if(*pert_tbl, "master_seed",
                  cfg.polypert_init.master_seed);

    read_value_if(*pert_tbl, "default_perturbation_factor",
                  cfg.polypert_init.default_perturbation_factor);


    // =========================
    // Medial Axis Pruning
    // =========================
    const auto* map_tbl = tbl["MedialAxisPruning"].as_table();
    if (!map_tbl)
        throw std::runtime_error("Missing [MedialAxisPruning] section");

    read_value_if(*map_tbl, "collinearity_threshold",
                  cfg.maprune_init.collinearity_threshold);

    read_value_if(*map_tbl, "absolute_covertex_threshold",
                  cfg.maprune_init.absolute_covertex_threshold);

    read_value_if(*map_tbl, "bbox_relative_covertex_threshold",
                  cfg.maprune_init.bbox_relative_covertex_threshold);

    // =========================
    // Medial Axis
    // =========================
    const auto* ma_tbl = tbl["MedialAxis"].as_table();
    if (!map_tbl)
        throw std::runtime_error("Missing [MedialAxis] section");

    read_value_if(*ma_tbl, "use_parabolas",
                  cfg.ma.use_parabolas);

    return cfg;
}

void log_config(const Config& cfg, std::ostream& out)
{
    out << "================ CONFIG ================\n\n";

    // =========================
    // IO
    // =========================
    out << "[IO]\n";
    out << "input_path = " << cfg.io.input_path << "\n";
    out << "output_path = " << cfg.io.output_path << "\n";
    out << "dataset = " 
        << (cfg.io.dataset.has_value() ?  *cfg.io.dataset : "all") 
        << "\n";
    out << "generate_plots = " << tf(cfg.io.generate_plots) << "\n";

    out << "error_log_file = ";
    if (cfg.io.error_log_file)
        out << *cfg.io.error_log_file << "\n";
    else
        out << "nullopt\n";

    out << "\n";


    // =========================
    // Polygon Pruning
    // =========================
    out << "[PolygonPruning]\n";
    out << "collinearity_threshold = "
        << cfg.polyprune_init.collinearity_threshold << "\n";
    out << "absolute_covertex_threshold = "
        << cfg.polyprune_init.absolute_covertex_threshold << "\n";
    out << "bbox_relative_covertex_threshold = "
        << cfg.polyprune_init.bbox_relative_covertex_threshold << "\n";

    out << "\n";


    // =========================
    // Polygon Perturbation
    // =========================
    out << "[PolygonPerturbation]\n";
    out << "master_seed = "
        << cfg.polypert_init.master_seed << "\n";
    out << "default_perturbation_factor = "
        << cfg.polypert_init.default_perturbation_factor << "\n";

    out << "\n";


    // =========================
    // Medial Axis Pruning
    // =========================
    out << "[MedialAxisPruning]\n";
    out << "collinearity_threshold = "
        << cfg.maprune_init.collinearity_threshold << "\n";
    out << "absolute_covertex_threshold = "
        << cfg.maprune_init.absolute_covertex_threshold << "\n";
    out << "bbox_relative_covertex_threshold = "
        << cfg.maprune_init.bbox_relative_covertex_threshold << "\n";

    out << "\n";

    
    // =========================
    // Medial Axiz
    // =========================
    out << "[MedialAxis]\n";
    out << "use_parabolas = " << tf(cfg.ma.use_parabolas) << "\n";

    out << "\n";

    out << "========================================\n";
}
