#ifndef MEDIALAXISWITHHOLES_IO_H
#define MEDIALAXISWITHHOLES_IO_H

#include "CGAL_includes.h"

#include <filesystem>
#include <string>
#include <vector>

// ----------------------------------------------------------------------
// Reads polygons with holes from a GeoPackage (.gpkg) file and stores
// them as CGAL Polygon_with_holes objects.
//
// The file read is filepath + ".gpkg"; only the first layer is used.
//
// To improve numeric stability, all coordinates are shifted so that
// the dataset's minimum x and y become (0,0), reducing round-off
// errors when polygon vertices are very close together compared to
// their absolute coordinates. Coordinates are also rounded to
// millimeter precision.
//
// Each polygon's integer ID is extracted from the specified field (default "id").
// If the field does not exist, the feature's FID is used as a fallback.
//
// Orientation of outer rings and holes is corrected: outer rings CCW, holes CW.
//
// This function produces a deterministic, reproducible dataset if the same
// GeoPackage is used and no additional randomness is applied.
void read_gpkg_to_polygons_with_holes(
        const std::filesystem::path &filepath,
        std::vector<Polygon_with_holes> &polygons,
        std::vector<int> &shape_ids,
        const std::string &idFieldName = "id");


#endif // MEDIALAXISWITHHOLES_IO_H