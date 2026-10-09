// Created by Laura Bülte on 2026-04-15.

#include "pipeline.h"

#include "io/logger.h"
#include "pruning/polygon_pruning.h"
#include "pruning/medial_axis_pruning.h"

#include <iostream>
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

    // Create medial axis from polygon
    MedialAxisGraph ma(polygon);
    ma.create_medial_axis(params.medial_axis.reflex_tolerance_deg, keep_parabolas);
    return ma;

    // /* Old recursive medial axis pruning code: */

    // // Compute and prune medial axis, if it is not valid, perturb polygon and repeat
    // while (true) {

    //     // Create medial axis from polygon, and prune it
    //     MedialAxisGraph ma(polygon);
    //     ma.create_medial_axis(params.medial_axis.reflex_tolerance_deg, keep_parabolas);
    //     prune_medial_axis(ma, params.medial_axis_pruning);

    //     // If medial axis is valid, return it
    //     if (check_validity_of_medial_axis(ma)){
    //         return ma;
    //     }

    //     // If medial axis is not valid even after pruning, perturb and prune polygon and recompute ma
    //     make_polygon_valid(true);
    // }
}
