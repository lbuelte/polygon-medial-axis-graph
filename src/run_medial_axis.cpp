// Created by Laura Bülte on 2026-04-15.

#include "run_medial_axis.h"

#include "MedialAxisWithHoles.h"

#include "pruning/polygon_pruning.h"
#include "pruning/medial_axis_pruning.h"

#include <random>

nlohmann::json run_medial_axis_with_holes(Config const &config, Polygon_with_holes polygon, int shape_id,
        std::optional<std::string> svg_path, bool perturb_immediately, double perturbation_factor)
{
    std::mt19937 master_gen(config.polypert_init.master_seed);

    // Helper that prunes polygon and if pruned polygon is not valid, perturbs and prunes polygon until it is
    // If input argument is true, polygon is perturbed immediately before first pruning
    auto make_polygon_valid = [&](bool perturb){
        bool is_valid_polygon = true;
        do {
            normalize_polygon_orientation(polygon);
            unsigned loop_seed = master_gen();
            // Perturb polygon vertices and restart if the previous polygon or ma is not valid
            if (perturb or not is_valid_polygon){
                perturb_polygon(polygon, perturbation_factor, loop_seed);
            }
            prune_polygon(polygon, config.polyprune_init);
            is_valid_polygon = check_validity_of_polygon(polygon);

        } while (not is_valid_polygon);
    };

    // (Possibly perturb and) prune polygon
    make_polygon_valid(perturb_immediately);

    // Create medial axis from polygon
    auto ma = MedialAxisWithHoles(polygon);    
    ma.create_medial_axis(config);
    if (svg_path.has_value()){
        ma.write_svg_with_holes(svg_path.value());
    }
    return ma.to_json_simple(shape_id);  

    // /* Old recursive medial axis pruning code: */

    // // Compute and prune medial axis, if it is not valid, perturb polygon and repeat
    // while (true) {
        
    //     // Create medial axis from polygon, and prune it    
    //     auto ma = MedialAxisWithHoles(polygon);    
    //     ma.create_medial_axis(config);
    //     prune_medial_axis(ma, config.maprune_init);

    //     // If medial axis is valid, write svg and return json
    //     if (check_validity_of_medial_axis){
    //         if (svg_path.has_value()){
    //             ma.write_svg_with_holes(svg_path.value());
    //         }
    //         return ma.to_json_simple(shape_id);            
    //     }

    //     // If medial axis is not valid even after pruning, perturb and prune polygon and recompute ma
    //     make_polygon_valid(true);
    // }
}