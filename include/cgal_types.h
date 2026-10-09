// Created by Laura Bülte on 2026-03-20.
//
// CGAL types used throughout the project: the kernel, the segment Voronoi diagram
// (built from a segment Delaunay graph) and the polygon types.

#ifndef MEDIAL_AXIS_GRAPH_CGAL_TYPES_H
#define MEDIAL_AXIS_GRAPH_CGAL_TYPES_H

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Voronoi_diagram_2.h>
#include <CGAL/Segment_Delaunay_graph_2.h>
#include <CGAL/Segment_Delaunay_graph_traits_2.h>
#include <CGAL/Segment_Delaunay_graph_adaptation_traits_2.h>
#include <CGAL/Segment_Delaunay_graph_adaptation_policies_2.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_with_holes_2.h>
#include <CGAL/Parabola_segment_2.h>

// Kernel: exact predicates, inexact (double) constructions
using K  = CGAL::Exact_predicates_inexact_constructions_kernel;

// Segment Delaunay graph of points and segments; its dual is the segment Voronoi diagram.
// Note: with K = Epick these traits evaluate their predicates in floating point (not exactly),
// which makes a few results vary between runs (see "Known limitations" in the README).
// CGAL::Segment_Delaunay_graph_filtered_traits_2<K> would be exact and deterministic,
// but changes about 1.4 % of the graphs, so the traits used for the paper are kept.
using Gt  = CGAL::Segment_Delaunay_graph_traits_2<K>;                            // geometric traits
using SDG = CGAL::Segment_Delaunay_graph_2<Gt>;
// Adaptor that turns the segment Delaunay graph into a Voronoi diagram:
// AT provides the geometric operations, AP removes degenerate (zero-length) Voronoi edges
using AT  = CGAL::Segment_Delaunay_graph_adaptation_traits_2<SDG>;
using AP  = CGAL::Segment_Delaunay_graph_caching_degeneracy_removal_policy_2<SDG>;
using VD  = CGAL::Voronoi_diagram_2<SDG, AT, AP>;

// A Voronoi edge between a point site and a segment site is a parabolic arc
using Parabola_segment_2 = CGAL::Parabola_segment_2<Gt>;

using Point_2   = AT::Point_2;
using Segment_2 = K::Segment_2;
using Vector_2  = K::Vector_2;
using Vertex_handle   = VD::Vertex_handle;
using Halfedge_handle = VD::Halfedge_handle;

using Polygon            = CGAL::Polygon_2<K>;
using Polygon_with_holes = CGAL::Polygon_with_holes_2<K>;

#endif // MEDIAL_AXIS_GRAPH_CGAL_TYPES_H
