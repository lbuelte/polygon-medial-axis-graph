// Small geometric helpers shared by the medial axis computation and the pruning code.

#ifndef MEDIAL_AXIS_GRAPH_GEOMETRY_UTILS_H
#define MEDIAL_AXIS_GRAPH_GEOMETRY_UTILS_H

#include "cgal_types.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>


// Squared Euclidean distance between a and b
inline double squared_dist(Point_2 const &a, Point_2 const &b)
{
    return CGAL::squared_distance(a, b);
}

// epsilon times the length of the bounding box diagonal of the polygon,
// used to turn relative thresholds into absolute distances
inline double epsilon_fraction_of_bbox_diag(Polygon_with_holes const &polygon, double epsilon){
    auto bbox = polygon.bbox();
    double dx = bbox.xmax() - bbox.xmin();
    double dy = bbox.ymax() - bbox.ymin();
    double diag = std::sqrt(dx*dx + dy*dy);
    return epsilon * diag;
}

// Mean (centroid) of a non-empty set of points
inline Point_2 mean_point(std::vector<Point_2> const &points)
{
    Point_2 sum = std::accumulate(points.begin(), points.end(), Point_2{0.0, 0.0},
        [](const Point_2& a, const Point_2& b) {
            return Point_2{a.x() + b.x(), a.y() + b.y()};
        });
    double count = static_cast<double>(points.size());
    return Point_2{ sum.x() / count, sum.y() / count };
}

// True if the angle prev - curr - next differs from 180 degrees by at most degree_offset,
// i.e. curr lies almost on a straight line between prev and next
inline bool is_nearly_collinear_by_angle(
    const Point_2& prev,
    const Point_2& curr,
    const Point_2& next,
    double degree_offset)
{
    Vector_2 v1 = prev - curr;
    Vector_2 v2 = next - curr;

    double len1 = std::sqrt(CGAL::to_double(v1.squared_length()));
    double len2 = std::sqrt(CGAL::to_double(v2.squared_length()));

    if (len1 == 0.0 || len2 == 0.0)
        return false;

    double dot = CGAL::to_double(v1 * v2);
    double cos_angle = dot / (len1 * len2);

    cos_angle = std::max(-1.0, std::min(1.0, cos_angle));

    double angle_deg = std::acos(cos_angle) * 180.0 / M_PI;

    return std::abs(angle_deg - 180.0) <= degree_offset;
}

// True if curr is a reflex vertex (right turn prev - curr - next, i.e. an interior angle
// above 180 degrees for a counterclockwise ring) or a nearly straight vertex (angle within
// tolerance_deg of 180 degrees).
// Medial axis edges ending at such vertices are not part of the medial axis graph.
inline bool is_reflex_or_nearly_straight(
    const Point_2 &prev,
    const Point_2 &curr,
    const Point_2 &next,
    double tolerance_deg)
{
    if (is_nearly_collinear_by_angle(prev, curr, next, tolerance_deg))
        return true;

    CGAL::Orientation o = CGAL::orientation(prev, curr, next);

    return o == CGAL::RIGHT_TURN;
}


#endif // MEDIAL_AXIS_GRAPH_GEOMETRY_UTILS_H
