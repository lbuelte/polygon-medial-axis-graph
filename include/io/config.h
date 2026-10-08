// Created by Laura Bülte on 2026-03-20.

#ifndef MEDIALAXISWITHHOLES_CONFIG_H
#define MEDIALAXISWITHHOLES_CONFIG_H

#include <string>
#include <optional>


struct IOConfig {
    std::string input_path;
    std::string output_path;
    std::optional<std::string> dataset = std::nullopt;
    std::optional<std::string> error_log_file = std::nullopt;
    bool generate_plots = true;
    // Instances (dataset name + index) that are perturbed before the medial axis is computed,
    // because CGAL got stuck or crashed on them (see scripts/watchdog.sh)
    std::string stuck_instances_file = "scripts/stuck_instances.txt";
    std::string segfault_instances_file = "scripts/segfault_instances.txt";
};

struct PolygonPruningConfig {
    double collinearity_threshold = 2;
    double absolute_covertex_threshold = 0.01;
    double bbox_relative_covertex_threshold = 0.001;
};

struct PolygonPerturbationConfig {
    unsigned master_seed = 42;
    double default_perturbation_factor = 1e-5;
};

struct MedialAxisPruningConfig {
    double collinearity_threshold = 10;
    double absolute_covertex_threshold = 0.01;
    double bbox_relative_covertex_threshold = 0.005;
};

struct MedialAxisConfig {
    bool use_parabolas = false;
};

struct Config {
    IOConfig io;
    PolygonPruningConfig polyprune_init;
    PolygonPerturbationConfig polypert_init;
    MedialAxisPruningConfig maprune_init;
    MedialAxisConfig ma;
};

Config load_config(const std::string& filename);
void log_config(const Config& cfg, std::ostream& out);


#endif // MEDIALAXISWITHHOLES_CONFIG_H