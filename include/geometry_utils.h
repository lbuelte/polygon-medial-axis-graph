// Small geometric helpers shared by the medial axis computation and the pruning code.

#ifndef MEDIALAXISWITHHOLES_GEOMETRY_UTILS_H
#define MEDIALAXISWITHHOLES_GEOMETRY_UTILS_H

#include "CGAL_includes.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>


// Helper to compute squared distance of x and y
inline double squared_dist(Point_2 const &a, Point_2 const &b)
{
    return CGAL::squared_distance(a, b);
}

// Helper to compute epsilon times the length of the bounding box diagonal of a polygon
inline double epsilon_fraction_of_bbox_diag(Polygon_with_holes const &polygon, double epsilon){
    auto bbox = polygon.bbox();
    double dx = bbox.xmax() - bbox.xmin();
    double dy = bbox.ymax() - bbox.ymin();
    double diag = std::sqrt(dx*dx + dy*dy);
    return epsilon * diag;
}

// Helper to compute mean of point vector
inline Point_2 meanPoint(std::vector<Point_2> points)
{
    Point_2 sum = std::accumulate(points.begin(), points.end(), Point_2{0.0, 0.0},
        [](const Point_2& a, const Point_2& b) {
            return Point_2{a.x() + b.x(), a.y() + b.y()};
        });
    double count = static_cast<double>(points.size());
    return Point_2{ sum.x() / count, sum.y() / count };
}

// Helper to check if prev - curr - next has a nearly collinear angle
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

// Helper to check if prev - curr - next is a reflex vertex or almost collinear
inline bool is_reflex_vertex(
    const Point_2 &prev,
    const Point_2 &curr,
    const Point_2 &next)
{
    const double degree_offset = 2.0;

    if (is_nearly_collinear_by_angle(prev, curr, next, degree_offset))
        return true;

    CGAL::Orientation o = CGAL::orientation(prev, curr, next);

    return o == CGAL::RIGHT_TURN;
}


#endif // MEDIALAXISWITHHOLES_GEOMETRY_UTILS_H
