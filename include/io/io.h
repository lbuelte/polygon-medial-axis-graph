#ifndef MEDIAL_AXIS_GRAPH_IO_H
#define MEDIAL_AXIS_GRAPH_IO_H

#include "cgal_types.h"

#include <filesystem>
#include <string>
#include <vector>

// ----------------------------------------------------------------------
// Reads polygons with holes from the first layer of a GeoPackage (.gpkg) file
// and stores them as CGAL Polygon_with_holes objects. polygons[i] has the id shape_ids[i].
//
// To improve numeric stability, all coordinates are shifted so that
// the dataset's minimum x and y become (0,0), reducing round-off
// errors when polygon vertices are very close together compared to
// their absolute coordinates. Coordinates are also rounded to
// millimeter precision.
//
// Each polygon's integer ID is read from the attribute id_field (default "id").
// If the attribute does not exist, the feature's FID is used as a fallback.
//
// Skipped with a warning: features that are not (Multi)Polygons, MultiPolygons with
// more than one part (all parts would share one id), and polygons with degenerate rings.
// Returns the number of skipped features.
//
// Orientation of outer rings and holes is corrected: outer rings CCW, holes CW.
//
// This function produces a deterministic, reproducible dataset if the same
// GeoPackage is used and no additional randomness is applied.
size_t read_polygons_from_gpkg(
        const std::filesystem::path &gpkg_file,
        std::vector<Polygon_with_holes> &polygons,
        std::vector<int> &shape_ids,
        const std::string &id_field = "id");


#endif // MEDIAL_AXIS_GRAPH_IO_H
