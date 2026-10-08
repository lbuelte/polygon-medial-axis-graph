#include "io/io.h"

#include <ogrsf_frmts.h>
#include <cmath>
#include <limits>
#include <stdexcept>

void read_gpkg_to_polygons_with_holes(
        const std::filesystem::path &filepath,
        std::vector<Polygon_with_holes> &polygons,
        std::vector<int> &shape_ids,
        const std::string &idFieldName) 
{
    GDALAllRegister();

    // Open dataset
    std::string uri = filepath.string() + ".gpkg";
    GDALDataset *poDS = static_cast<GDALDataset*>(
        GDALOpenEx(uri.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr));
    if (!poDS) {
        throw std::runtime_error("Could not open GeoPackage: " + uri);
    }

    OGRLayer *layer = poDS->GetLayer(0);
    if (!layer) {
        GDALClose(poDS);
        throw std::runtime_error("Could not get layer from GeoPackage.");
    }

    // Check if the ID field exists
    OGRFeatureDefn *layerDef = layer->GetLayerDefn();
    int idFieldIndex = layerDef->GetFieldIndex(idFieldName.c_str());

    // -------------------------
    // 1. Compute global minX / minY
    // -------------------------
    double min_x = std::numeric_limits<double>::max();
    double min_y = std::numeric_limits<double>::max();

    layer->ResetReading();
    OGRFeature *feature;
    while ((feature = layer->GetNextFeature()) != nullptr) {
        OGRGeometry *geom = feature->GetGeometryRef();
        if (!geom) { 
            OGRFeature::DestroyFeature(feature); 
            continue; 
        }
        OGRwkbGeometryType geomType = wkbFlatten(geom->getGeometryType());
        std::vector<OGRPolygon*> ogrPolygons;
        if (geomType == wkbPolygon) {
            ogrPolygons.push_back(geom->toPolygon());
        } else if (geomType == wkbMultiPolygon) {
            OGRMultiPolygon *multi = geom->toMultiPolygon();
            for (int i = 0; i < multi->getNumGeometries(); ++i) {
                ogrPolygons.push_back(multi->getGeometryRef(i)->toPolygon());
            }
        } else {
            OGRFeature::DestroyFeature(feature); 
            continue;
        }       

        bool valid_feature = true;
        for (OGRPolygon *ogrPoly : ogrPolygons) {
            if (!ogrPoly) { 
                valid_feature = false;
                break; 
            }

            // Exterior ring
            OGRLinearRing *exterior = ogrPoly->getExteriorRing();
            if (!exterior){
                valid_feature = false;
                break;
            }
            for (int i = 0; i < exterior->getNumPoints(); ++i) {
                min_x = std::min(min_x, exterior->getX(i));
                min_y = std::min(min_y, exterior->getY(i));
            }

            // Interior rings
            for (int h = 0; h < ogrPoly->getNumInteriorRings(); ++h) {
                OGRLinearRing *ring = ogrPoly->getInteriorRing(h);
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
    auto round_mm = [](double v) {
        return std::round(v * 1000.0) / 1000.0; // round to millimeters
    };

    int min_shape_id = std::numeric_limits<int>::infinity();

    while ((feature = layer->GetNextFeature()) != nullptr) {
        OGRGeometry *geom = feature->GetGeometryRef();
        if (!geom) { 
            OGRFeature::DestroyFeature(feature); 
            continue; 
        }
        OGRwkbGeometryType geomType = wkbFlatten(geom->getGeometryType());
        std::vector<OGRPolygon*> ogrPolygons;
        if (geomType == wkbPolygon) {
            ogrPolygons.push_back(geom->toPolygon());
        } else if (geomType == wkbMultiPolygon) {
            OGRMultiPolygon *multi = geom->toMultiPolygon();
            for (int i = 0; i < multi->getNumGeometries(); ++i) {
                ogrPolygons.push_back(multi->getGeometryRef(i)->toPolygon());
            }
        } else {
            OGRFeature::DestroyFeature(feature); 
            continue;
        }       

        // Helper to make polygons non-degenerate
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

        bool valid_polyogn = true;
        for (OGRPolygon *ogrPoly : ogrPolygons) {
            if (!ogrPoly) { 
                continue; 
            }

            // Read ID if exists, else fallback to FID
            int shape_id = static_cast<int>(feature->GetFID());
            if (idFieldIndex >= 0) {
                shape_id = feature->GetFieldAsInteger(idFieldIndex);
            }
            shape_ids.push_back(shape_id);

            // Exterior ring
            OGRLinearRing *exterior = ogrPoly->getExteriorRing();
            if (!exterior) {
                continue;
            }
            Polygon outer;
            if (not get_nondegenerate_polygon(exterior, outer)){
                continue;
            }
            if (!outer.is_counterclockwise_oriented()) {
                outer.reverse_orientation();
            }

            // Interior rings
            bool all_holes_are_valid = true;
            std::vector<Polygon> holes;
            for (int h = 0; h < ogrPoly->getNumInteriorRings(); ++h) {
                OGRLinearRing *ring = ogrPoly->getInteriorRing(h);
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
                continue;
            }

            polygons.emplace_back(outer, holes.begin(), holes.end());
            OGRFeature::DestroyFeature(feature);
        }
    }

    GDALClose(poDS);
}
