#include "params.h"

#include <toml++/toml.hpp>

#include <charconv>
#include <climits>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// One line per parameter: TOML key, struct member, description.
// To add a parameter, add a struct member in params.h and one line here.
// ---------------------------------------------------------------------------

template <class F> void for_each_param(PolygonPruningParams &p, F &&f)
{
    f("collinearity_threshold_deg", p.collinearity_threshold_deg, "remove vertices whose angle is within this many degrees of 180");
    f("merge_distance", p.merge_distance, "merge consecutive vertices closer than this distance ...");
    f("merge_distance_rel_bbox", p.merge_distance_rel_bbox, "... or than this fraction of the bounding box diagonal (smaller threshold is used)");
    f("hole_vertex_offset", p.hole_vertex_offset, "distance by which hole vertices that touch the outer boundary are moved off it");
}

template <class F> void for_each_param(ValidityParams &p, F &&f)
{
    f("min_vertex_distance", p.min_vertex_distance, "minimum distance between any two vertices");
    f("collinearity_threshold_deg", p.collinearity_threshold_deg, "no vertex angle may be within this many degrees of 180");
}

template <class F> void for_each_param(PerturbationParams &p, F &&f)
{
    f("seed", p.seed, "seed of the random perturbations (results are reproducible)");
    f("max_displacement_rel_bbox", p.max_displacement_rel_bbox, "maximum displacement per coordinate, as a fraction of the bounding box diagonal");
}

template <class F> void for_each_param(MedialAxisParams &p, F &&f)
{
    f("reflex_tolerance_deg", p.reflex_tolerance_deg, "vertices within this many degrees of 180 are treated like reflex vertices");
}

template <class F> void for_each_param(MedialAxisPruningParams &p, F &&f)
{
    f("enabled", p.enabled, "prune the medial axis graph (false: keep the unpruned graph)");
    f("collinearity_threshold_deg", p.collinearity_threshold_deg, "remove degree-2 nodes whose angle is within this many degrees of 180");
    f("merge_distance", p.merge_distance, "merge adjacent nodes closer than this distance ...");
    f("merge_distance_rel_bbox", p.merge_distance_rel_bbox, "... or than this fraction of the bounding box diagonal (smaller threshold is used)");
}

// All sections in file order: TOML name, description, struct
template <class F> void for_each_section(Params &p, F &&f)
{
    f("polygon_pruning", "Simplification of the input polygon before the medial axis is computed", p.polygon_pruning);
    f("validity", "When a pruned polygon is valid (invalid polygons are perturbed and pruned again)", p.validity);
    f("perturbation", "Random perturbation of polygons that are invalid after pruning", p.perturbation);
    f("medial_axis", "Extraction of the medial axis graph from the Voronoi diagram", p.medial_axis);
    f("medial_axis_pruning", "Simplification of the medial axis graph", p.medial_axis_pruning);
}


// ---------------- reading and writing single values ----------------

void read_value(const toml::node &node, double &out, const std::string &name)
{
    // value<double>() also accepts integers, e.g. "2" for 2.0
    if (auto v = node.value<double>()) {
        out = *v;
        return;
    }
    throw ParamsError(name + " must be a number");
}

void read_value(const toml::node &node, unsigned &out, const std::string &name)
{
    if (auto v = node.value_exact<int64_t>(); v and *v >= 0 and *v <= UINT_MAX) {
        out = static_cast<unsigned>(*v);
        return;
    }
    throw ParamsError(name + " must be a non-negative integer");
}

void read_value(const toml::node &node, bool &out, const std::string &name)
{
    if (auto v = node.value_exact<bool>()) {
        out = *v;
        return;
    }
    throw ParamsError(name + " must be true or false");
}

// Shortest representation that reads back to exactly the same double, always as a TOML float
std::string format_value(double v)
{
    char buffer[64];
    auto result = std::to_chars(buffer, buffer + sizeof(buffer), v);
    std::string s(buffer, result.ptr);
    if (s.find_first_of(".eEn") == std::string::npos) {
        s += ".0";
    }
    return s;
}

std::string format_value(unsigned v)
{
    return std::to_string(v);
}

std::string format_value(bool v)
{
    return v ? "true" : "false";
}

// Comma-separated list of the valid keys of a section (for error messages)
template <class Section> std::string valid_keys(Section &section)
{
    std::string keys;
    for_each_param(section, [&](const char *name, auto &, const char *) {
        keys += (keys.empty() ? "" : ", ") + std::string(name);
    });
    return keys;
}

} // end of anonymous namespace


Params load_params(const std::string &filename)
{
    toml::table tbl;
    try {
        tbl = toml::parse_file(filename);
    }
    catch (const toml::parse_error &err) {
        std::ostringstream message;
        message << "could not read parameter file " << filename << " (line " << err.source().begin.line
                << "): " << err.description();
        throw ParamsError(message.str());
    }

    Params params;
    std::string valid_sections;
    for_each_section(params, [&](const char *section, const char *, auto &) {
        valid_sections += (valid_sections.empty() ? "" : ", ") + std::string(section);
    });

    for (auto &&[section_key, section_node] : tbl) {
        const std::string section_name(section_key.str());
        bool section_known = false;

        for_each_section(params, [&](const char *section, const char *, auto &values) {
            if (section_name != section) {
                return;
            }
            section_known = true;

            const toml::table *section_tbl = section_node.as_table();
            if (!section_tbl) {
                throw ParamsError("[" + section_name + "] in " + filename + " must be a section");
            }
            for (auto &&[key, node] : *section_tbl) {
                const std::string key_name(key.str());
                bool key_known = false;
                for_each_param(values, [&](const char *name, auto &member, const char *) {
                    if (key_name == name) {
                        key_known = true;
                        read_value(node, member, section_name + "." + key_name + " in " + filename);
                    }
                });
                if (not key_known) {
                    throw ParamsError("unknown key '" + key_name + "' in [" + section_name + "] of " + filename
                                      + "; valid keys: " + valid_keys(values));
                }
            }
        });

        if (not section_known) {
            throw ParamsError("unknown section [" + section_name + "] in " + filename
                              + "; valid sections: " + valid_sections);
        }
    }
    return params;
}


void write_params(const Params &params_in, std::ostream &out)
{
    Params params = params_in;  // for_each_section needs a non-const object

    out << "# Parameters of polygon-medial-axis-graph.\n"
        << "# All sections and keys are optional; missing ones keep their default values.\n"
        << "# Distances are in the units of the input coordinates (metres).\n";

    for_each_section(params, [&](const char *section, const char *description, auto &values) {
        out << "\n# " << description << "\n[" << section << "]\n";
        for_each_param(values, [&](const char *name, auto &member, const char *param_description) {
            std::string assignment = std::string(name) + " = " + format_value(member);
            out << std::left << std::setw(40) << assignment << " # " << param_description << "\n";
        });
    });
}
