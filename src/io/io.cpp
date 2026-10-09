#include "io/io.h"

#include "io/logger.h"

#include <ogrsf_frmts.h>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

// Returns the polygon parts of a feature's geometry. Returns no parts, and sets skip_reason,
// if the feature is skipped: no geometry, not a (Multi)Polygon, or a MultiPolygon with
// several parts (all parts would share one id).
std::vector<OGRPolygon*> polygon_parts(OGRGeometry *geom, std::string &skip_reason)
{
    if (!geom) {
        skip_reason = "no geometry";
        return {};
    }
    OGRwkbGeometryType geom_type = wkbFlatten(geom->getGeometryType());
    if (geom_type == wkbPolygon) {
        return {geom->toPolygon()};
    }
    if (geom_type == wkbMultiPolygon) {
        OGRMultiPolygon *multi = geom->toMultiPolygon();
        if (multi->getNumGeometries() > 1) {
            skip_reason = "MultiPolygon with " + std::to_string(multi->getNumGeometries()) + " parts";
            return {};
        }
        std::vector<OGRPolygon*> parts;
        for (int i = 0; i < multi->getNumGeometries(); ++i) {
            parts.push_back(multi->getGeometryRef(i)->toPolygon());
        }
        return parts;
    }
    skip_reason = std::string("unsupported geometry type ") + geom->getGeometryName();
    return {};
}

} // end of anonymous namespace


size_t read_polygons_from_gpkg(
        const std::filesystem::path &gpkg_file,
        std::vector<Polygon_with_holes> &polygons,
        std::vector<int> &shape_ids,
        const std::string &id_field)
{
    GDALAllRegister();

    // Open dataset
    GDALDataset *dataset = static_cast<GDALDataset*>(
        GDALOpenEx(gpkg_file.string().c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr));
    if (!dataset) {
        throw std::runtime_error("Could not open GeoPackage: " + gpkg_file.string());
    }

    OGRLayer *layer = dataset->GetLayer(0);
    if (!layer) {
        GDALClose(dataset);
        throw std::runtime_error("Could not get layer from GeoPackage: " + gpkg_file.string());
    }

    // Index of the id attribute, or -1 if it does not exist
    int id_field_index = layer->GetLayerDefn()->GetFieldIndex(id_field.c_str());

    // -------------------------
    // 1. Compute global min_x / min_y over all polygons that will be read
    // -------------------------
    double min_x = std::numeric_limits<double>::max();
    double min_y = std::numeric_limits<double>::max();

    layer->ResetReading();
    OGRFeature *feature;
    std::string skip_reason;
    while ((feature = layer->GetNextFeature()) != nullptr) {
        for (OGRPolygon *ogr_poly : polygon_parts(feature->GetGeometryRef(), skip_reason)) {
            if (!ogr_poly || !ogr_poly->getExteriorRing()) {
                break;
            }

            // Exterior ring
            OGRLinearRing *exterior = ogr_poly->getExteriorRing();
            for (int i = 0; i < exterior->getNumPoints(); ++i) {
                min_x = std::min(min_x, exterior->getX(i));
                min_y = std::min(min_y, exterior->getY(i));
            }

            // Interior rings
            for (int h = 0; h < ogr_poly->getNumInteriorRings(); ++h) {
                OGRLinearRing *ring = ogr_poly->getInteriorRing(h);
                for (int i = 0; i < ring->getNumPoints(); ++i) {
                    min_x = std::min(min_x, ring->getX(i));
                    min_y = std::min(min_y, ring->getY(i));
                }
            }
        }

        OGRFeature::DestroyFeature(feature);
    }

    // -------------------------
    // 2. Read polygons with global shift and millimeter rounding
    // -------------------------
    layer->ResetReading();
    size_t num_skipped = 0;
    auto round_mm = [](double v) {
        return std::round(v * 1000.0) / 1000.0; // round to millimeters
    };

    // Helper that converts a ring to a polygon (shifted and rounded) without the duplicate
    // closing vertex. Returns false if the ring has fewer than 3 distinct vertices.
    auto get_nondegenerate_polygon = [&](OGRLinearRing* ring, Polygon &polygon){

        int n = ring->getNumPoints();
        if (n < 3) {
            // not enough points to form a polygon
            return false;
        }

        // Check for duplicate closing vertex (after shift + rounding!)
        int effective_n = n;
        double x0 = round_mm(ring->getX(0) - min_x);
        double y0 = round_mm(ring->getY(0) - min_y);
        double xN = round_mm(ring->getX(n - 1) - min_x);
        double yN = round_mm(ring->getY(n - 1) - min_y);

        if (x0 == xN && y0 == yN) {
            effective_n = n - 1;
        }
        if (effective_n < 3) {
            return false;
        }
        // Build polygon without duplicate last point
        for (int i = 0; i < effective_n; ++i) {
            double x = round_mm(ring->getX(i) - min_x);
            double y = round_mm(ring->getY(i) - min_y);
            polygon.push_back(Point_2(x, y));
        }
        return true;
    };

    while ((feature = layer->GetNextFeature()) != nullptr) {

        // Read ID if exists, else fallback to FID
        int shape_id = static_cast<int>(feature->GetFID());
        if (id_field_index >= 0) {
            shape_id = feature->GetFieldAsInteger(id_field_index);
        }

        // Helper to report polygons that are skipped
        auto warn_skip = [&](const std::string &reason){
            log_warning("skipping polygon with id " + std::to_string(shape_id) + " (" + reason + ")");
            ++num_skipped;
        };

        skip_reason.clear();
        std::vector<OGRPolygon*> parts = polygon_parts(feature->GetGeometryRef(), skip_reason);
        if (not skip_reason.empty()) {
            warn_skip(skip_reason);
        }

        for (OGRPolygon *ogr_poly : parts) {
            if (!ogr_poly) {
                continue;
            }

            // Exterior ring
            OGRLinearRing *exterior = ogr_poly->getExteriorRing();
            if (!exterior) {
                warn_skip("no exterior ring");
                continue;
            }
            Polygon outer;
            if (not get_nondegenerate_polygon(exterior, outer)){
                warn_skip("degenerate exterior ring");
                continue;
            }
            if (!outer.is_counterclockwise_oriented()) {
                outer.reverse_orientation();
            }

            // Interior rings
            bool all_holes_are_valid = true;
            std::vector<Polygon> holes;
            for (int h = 0; h < ogr_poly->getNumInteriorRings(); ++h) {
                OGRLinearRing *ring = ogr_poly->getInteriorRing(h);
                Polygon hole;
                if (not get_nondegenerate_polygon(ring, hole)){
                    all_holes_are_valid = false;
                    break;
                }
                if (hole.is_counterclockwise_oriented()){
                    hole.reverse_orientation();
                }
                holes.push_back(hole);
            }
            if (not all_holes_are_valid){
                warn_skip("degenerate hole");
                continue;
            }

            // Store the id only together with its polygon, so that ids and polygons stay aligned
            shape_ids.push_back(shape_id);
            polygons.emplace_back(outer, holes.begin(), holes.end());
        }
        // The polygon parts point into the feature's geometry, so destroy it only after the loop
        OGRFeature::DestroyFeature(feature);
    }

    GDALClose(dataset);
    return num_skipped;
}
