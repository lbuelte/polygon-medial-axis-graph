// Created by Laura Bülte on 2026-04-15.

#include "CGAL_includes.h"

#include "io/config.h"
#include "json/json.hpp"

#ifndef MEDIALAXISWITHHOLES_RUN_H
#define MEDIALAXISWITHHOLES_RUN_H

// TODO : Create a struct with something like a current config setting which can be changed 
nlohmann::json run_medial_axis_with_holes(
        Config const &config, 
        Polygon_with_holes polygon, 
        int shape_id,
        std::optional<std::string> svg_path = std::nullopt, 
        bool perturb_immediately = false, 
        double perturbation_factor = 1e-5);

#endif // MEDIALAXISWITHHOLES_RUN_H