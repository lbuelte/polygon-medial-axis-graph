// Created by Laura Bülte on 2026-03-20.

#ifndef MEDIALAXISWITHHOLES_CGAL_INCLUDES_H
#define MEDIALAXISWITHHOLES_CGAL_INCLUDES_H

/// includes for defining the Voronoi diagram adaptor
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Voronoi_diagram_2.h>
#include <CGAL/Segment_Delaunay_graph_2.h>
#include <CGAL/Segment_Delaunay_graph_traits_2.h>
#include <CGAL/Segment_Delaunay_graph_adaptation_traits_2.h>
#include <CGAL/Segment_Delaunay_graph_adaptation_policies_2.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_with_holes_2.h>
#include <CGAL/Parabola_segment_2.h>

using K  = CGAL::Exact_predicates_inexact_constructions_kernel;
// using K  = CGAL::Exact_predicates_exact_constructions_kernel;

using Gt = CGAL::Segment_Delaunay_graph_traits_2<K>;
using SDG = CGAL::Segment_Delaunay_graph_2<Gt>;
using AT = CGAL::Segment_Delaunay_graph_adaptation_traits_2<SDG>;
using AP = CGAL::Segment_Delaunay_graph_caching_degeneracy_removal_policy_2<SDG>;
using VD = CGAL::Voronoi_diagram_2<SDG, AT, AP>;
using Parabola_segment_2 = CGAL::Parabola_segment_2<Gt>;

using Site_2 = AT::Site_2;
using Point_2 = AT::Point_2;
using Segment_2 = K::Segment_2;
using Locate_result = VD::Locate_result;
using Vertex_handle = VD::Vertex_handle;
using Face_handle = VD::Face_handle;
using Halfedge_handle = VD::Halfedge_handle;
using Ccb_halfedge_circulator = VD::Ccb_halfedge_circulator;

using Polygon = CGAL::Polygon_2<K>;
using Vector_2 = K::Vector_2;
using Polygon_with_holes = CGAL::Polygon_with_holes_2<K>;

#endif // MEDIALAXISWITHHOLES_CGAL_INCLUDES_H
