// Created by Laura Bülte on 2026-04-15.

#include "pipeline.h"

#include "io/logger.h"
#include "pruning/polygon_pruning.h"
#include "pruning/medial_axis_pruning.h"

#include <iostream>
#include <optional>
#include <random>
#include <string>

MedialAxisGraph compute_medial_axis_graph(Polygon_with_holes polygon, Params const &params,
                                          bool perturb_immediately, bool keep_parabolas)
{
    // Generates one seed per attempt, so every perturbation is different but reproducible
    std::mt19937 master_gen(params.perturbation.seed);

    // Helper that prunes polygon and if pruned polygon is not valid, perturbs and prunes polygon until it is
    // If input argument is true, polygon is perturbed immediately before first pruning
    // Problems are reported once at the end instead of on every attempt
    auto make_polygon_valid = [&](bool perturb){
        const int max_perturbations = 1000;
        int perturbations = 0;
        std::string first_reason, reason;
        bool is_valid_polygon = true;
        do {
            normalize_polygon_orientation(polygon);
            unsigned loop_seed = master_gen();
            // Perturb polygon vertices and restart if the previous polygon or ma is not valid
            if (perturb or not is_valid_polygon){
                if (++perturbations > max_perturbations) {
                    throw InvalidPolygonError("no valid polygon after " + std::to_string(max_perturbations)
                        + " perturbations (first problem: " + first_reason + "; last problem: " + reason + ")");
                }
                perturb_polygon(polygon, params.perturbation.max_displacement_rel_bbox, loop_seed);
            }
            prune_polygon(polygon, params.polygon_pruning, params.validity);
            is_valid_polygon = check_validity_of_polygon(polygon, params.validity, &reason);
            if (not is_valid_polygon and first_reason.empty()) {
                first_reason = reason;
            }

        } while (not is_valid_polygon);

        if (not first_reason.empty()) {
            log_warning("polygon was invalid (" + first_reason + "), valid after "
                + std::to_string(perturbations) + " perturbation(s)");
        }
    };

    // (Possibly perturb and) prune polygon
    make_polygon_valid(perturb_immediately);

    // Helper that computes the medial axis graph of the current polygon and prunes it
    auto compute_graph = [&](){
        MedialAxisGraph graph(polygon);
        graph.create_medial_axis(params.medial_axis.reflex_tolerance_deg, keep_parabolas);
        if (params.medial_axis_pruning.enabled) {
            prune_medial_axis(graph, params.medial_axis_pruning);
        }
        return graph;
    };

    // Compute the graph. If it is not valid (not connected, or the number of cycles does not
    // match the number of holes), perturb and prune the polygon and compute it again.
    // After max_retries unsuccessful perturbations the last graph is kept with a warning.
    const int max_retries = 10;
    std::optional<MedialAxisGraph> ma;
    ma.emplace(compute_graph());

    std::string first_reason, reason;
    int retries = 0;
    while (not check_validity_of_medial_axis(*ma, &reason)) {
        if (first_reason.empty()) {
            first_reason = reason;
        }
        if (retries == max_retries) {
            log_warning("medial axis graph not valid after " + std::to_string(max_retries)
                + " perturbation(s) (" + reason + "), keeping the last graph");
            return *ma;
        }
        ++retries;
        try {
            make_polygon_valid(true);
        }
        catch (InvalidPolygonError const &e) {
            log_warning("medial axis graph not valid (" + reason + ") and the perturbed polygon could not be"
                " made valid (" + e.what() + "), keeping the last graph");
            return *ma;
        }
        ma.emplace(compute_graph());
    }

    if (retries > 0) {
        log_warning("medial axis graph was not valid (" + first_reason + "), valid after "
            + std::to_string(retries) + " perturbation(s)");
    }
    return *ma;
}
