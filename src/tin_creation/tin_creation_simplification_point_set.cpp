// Copyright (c) 2018 Coronis Computing S.L. (Spain)
// All rights reserved.
//
// This file is part of EMODnet Quantized Mesh Generator for Cesium.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//
// Author: Ricard Campos (ricardcd@gmail.com)
#include "tin_creation_simplification_point_set.h"
#include <CGAL/convex_hull_2.h>
#include <CGAL/Triangulation_conformer_2.h>
#include "cgal/polyhedron_builder_from_projected_triangulation.h"
//#include "cgal/Polyhedral_mesh_domain_with_features_3_extended.h"
// Project-related
#include "cgal/detect_sharp_edges_without_borders.h"
#include "cgal/extract_polylines_from_sharp_edges.h"
#include "cgal/extract_tile_borders_from_polyhedron.h"
#include "cgal/cgal_utils.h"

namespace TinCreation {

Polyhedron TinCreationSimplificationPointSet::create( const std::vector<Point_3>& dataPts,
                                                      const bool &constrainEasternVertices,
                                                      const bool &constrainWesternVertices,
                                                      const bool &constrainNorthernVertices,
                                                      const bool &constrainSouthernVertices)
{
    // Scale the parameters according to the tile
    m_borderSimpMaxScaledSqDist = m_borderSimpMaxDist*this->getScaleZ();
    m_borderSimpMaxScaledSqDist *= m_borderSimpMaxScaledSqDist; // Squared value to ease distance computations

    PointCloud ptsToSimplify;

    // Delaunay triangulation
    Delaunay dt( dataPts.begin(), dataPts.end() );

    // Translate to Polyhedron
    Polyhedron surface;
    PolyhedronBuilderFromProjectedTriangulation<Delaunay, HalfedgeDS> builderDT(dt);
    surface.delegate(builderDT);
    surface.normalize_border();

    // Simplification
    getAllNonBorderVertices(surface, ptsToSimplify); // Note that, because of the required pixel overlap for terrain tiles, this additional line of pixels go over the poles in extreme tiles when in ECEF and when converted back they get lat/lon on the other half of the globe... Since we treat border points differently, we don't have any problem. If you try to simplify ALL the points in the tile, the method will fail because of that reason!
    ptsToSimplify = simplify(ptsToSimplify);

    // Impose the constraints based on borders and features in the original mesh
    imposeConstraintsAndSimplifyPolylines(surface,
                                          constrainEasternVertices,
                                          constrainWesternVertices,
                                          constrainNorthernVertices,
                                          constrainSouthernVertices);

    // Insert the simplified points in the constrained triangulation
    for( PointCloud::iterator it = ptsToSimplify.begin(); it != ptsToSimplify.end(); ++it ) {
        // Error check
        if (it->x() < 0.0 || it->y() < 0.0 || it->x() > 1.0 || it->y() > 1.0) {
            std::cout << "[ERROR] point to insert not on the 0..1 range: " << *it << std::endl;
            continue;
        }

        // Insert the point
        m_cdt.insert(*it);
    }

    // Test: Make it conforming Delaunay before storing (don't know why but this results in an infinite loop...)
//    std::cout << "Making conforming delaunay" << std::endl;
//    CGAL::make_conforming_Delaunay_2(m_cdt);

    // Translate to Polyhedron
    Polyhedron surface2;
    PolyhedronBuilderFromProjectedTriangulation<CTXY, HalfedgeDS> builderCDT(m_cdt);
    surface2.delegate(builderCDT);

    m_cdt.clear() ; // Clear internal variables for reusing the object
    return surface2 ;
}



void
TinCreationSimplificationPointSet::
imposeConstraintsAndSimplifyPolylines(Polyhedron& surface, // Note: points in the borders to maintain will be removed from this list!
                  const bool &constrainEasternVertices,
                  const bool &constrainWesternVertices,
                  const bool &constrainNorthernVertices,
                  const bool &constrainSouthernVertices)
{
    // NOTE: we do not use the convex hull to get the borders, basically because the implementation of CGAL only
    // considers the corner vertices of the tiles as points on the convex hull, and not the ones ON the edges joining them.
    // We leave the code comented for future reference:
    // Compute the convex hull in the base 2D plane
//    PointCloud chPts;
//    CGAL::convex_hull_2( pts.begin(), pts.end(), std::back_inserter(chPts), CGAL::Projection_traits_xy_3<K>() );

    // Extract the points at the borders
    PointCloud northernBorderVertices, southernBorderVertices, easternBorderVertices, westernBorderVertices ;
    Point_3 cornerPoint00, cornerPoint01, cornerPoint10, cornerPoint11;
    extractTileBordersFromPolyhedron<Polyhedron>(surface, easternBorderVertices, westernBorderVertices, northernBorderVertices, southernBorderVertices, cornerPoint00, cornerPoint01, cornerPoint10, cornerPoint11);

    // Sort the points in the borders
    auto smallerThanInX = [](const Point_3& a, const Point_3& b) -> bool {return a.x() < b.x();};
    auto smallerThanInY = [](const Point_3& a, const Point_3& b) -> bool {return a.y() < b.y();};
    std::sort(easternBorderVertices.begin(), easternBorderVertices.end(), smallerThanInY) ;
    std::sort(westernBorderVertices.begin(), westernBorderVertices.end(), smallerThanInY) ;
    std::sort(northernBorderVertices.begin(), northernBorderVertices.end(), smallerThanInX) ;
    std::sort(southernBorderVertices.begin(), southernBorderVertices.end(), smallerThanInX) ;

    // Collect the polylines to simplify, or leave them as they are if required to
/*
std::cout << "icsp: Collect the polylines to simplify" << std::endl;
    Polylines polylinesToSimplify, polylinesToMaintain ;
    if (!constrainEasternVertices) {
	std::cout << "Eastern polyline, set to simplify" << std::endl;
	for (Polyline::iterator itp = easternBorderVertices.begin(); itp != easternBorderVertices.end(); ++itp) 
		std::cout << *itp << std::endl;
        m_cdt.insert_constraint(easternBorderVertices.begin(), easternBorderVertices.end(), false);
    }
    else
        polylinesToMaintain.push_back(easternBorderVertices);
    if (!constrainWesternVertices) {
	std::cout << "Western polyline, set to simplify" << std::endl;
	for (Polyline::iterator itp = westernBorderVertices.begin(); itp != westernBorderVertices.end(); ++itp) 
		std::cout << *itp << std::endl;
        m_cdt.insert_constraint(westernBorderVertices.begin(), westernBorderVertices.end(), false);
    }
    else
        polylinesToMaintain.push_back(westernBorderVertices);
    if (!constrainNorthernVertices) {
	std::cout << "Northern polyline, set to simplify" << std::endl;
	for (Polyline::iterator itp = northernBorderVertices.begin(); itp != northernBorderVertices.end(); ++itp) 
		std::cout << *itp << std::endl;
        m_cdt.insert_constraint(northernBorderVertices.begin(), northernBorderVertices.end(), false);
    }
    else
        polylinesToMaintain.push_back(northernBorderVertices);
    if (!constrainSouthernVertices) {
        std::cout << "Southern polyline, set to simplify" << std::endl;
	for (Polyline::iterator itp = southernBorderVertices.begin(); itp != southernBorderVertices.end(); ++itp) 
		std::cout << *itp << std::endl;
        m_cdt.insert_constraint(southernBorderVertices.begin(), southernBorderVertices.end(), false);
    }
    else
        polylinesToMaintain.push_back(southernBorderVertices);
std::cout << "icsp: Collect the polylines to simplify DONE" << std::endl;

    // Create a property map storing if an edge is sharp or not (since the Polyhedron_3 does not have internal property_maps creation, we use a map container within a boost::associative_property_map)
    typedef typename boost::graph_traits<Polyhedron>::edge_descriptor edge_descriptor;
    typedef typename std::map<edge_descriptor, bool> EdgeIsSharpMap;
    typedef typename boost::associative_property_map<EdgeIsSharpMap> EdgeIsSharpPropertyMap;
    EdgeIsSharpMap map;
    EdgeIsSharpPropertyMap eisMap(map);

    // Detect sharp edges
std::cout << "icsp: Detect sharp edges" << std::endl;
    detect_sharp_edges_without_borders<Polyhedron, double, EdgeIsSharpPropertyMap, K>(surface, FT(60.0), eisMap);
std::cout << "icsp: Detect sharp edges DONE" << std::endl;

    // Trace the polylines from the detected edges

std::cout << "icsp: Trace polylines from detected edges" << std::endl;
    Polylines featurePolylines;
    extract_polylines_from_sharp_edges(surface, eisMap, featurePolylines);
std::cout << "icsp: Trace polylines from detected edges DONE" << std::endl;

std::cout << "icsp: Insert constraints" << std::endl;
    for ( Polylines::const_iterator it = featurePolylines.begin(); it != featurePolylines.end(); ++it ) {
	if ((*it).size() > m_minFeaturePolylineSize) {
	    std::cout << "A polyline to simplify:" << std::endl;
	    for (Polyline::const_iterator itp = (*it).begin(); itp != (*it).end(); ++itp) 
		std::cout << *itp << std::endl;
	    std::cout << "Insert the polyline to simplify" << std::endl;
            m_cdt.insert_constraint((*it).begin(), (*it).end(), false);
	    std::cout << "Insert the polyline to simplify DONE" << std::endl;
	}
    }
std::cout << "icsp: Insert constraints DONE" << std::endl;

    // Simplify the polylines
std::cout << "icsp: Simplify the polylines" << std::endl;
    std::size_t numRemoved = PS::simplify(m_cdt, PSSqDist3Cost(m_borderSimpMaxLengthPercent), PSStopCost(m_borderSimpMaxScaledSqDist), true);
std::cout << "icsp: Simplify the polylines DONE" << std::endl;
    
    // Debug!!!!
    Polyhedron surface2;
    PolyhedronBuilderFromProjectedTriangulation<CTXY, HalfedgeDS> builderCDT(m_cdt);
    surface2.delegate(builderCDT);
    std::ofstream of("current_after_simplify.off");
    of << surface2;

    // Finally, insert the border polylines that need to be maintained as they are
std::cout << "icsp: insert the border polylines that need to be maintained as they are" << std::endl;
    for ( Polylines::iterator it = polylinesToMaintain.begin(); it != polylinesToMaintain.end(); ++it ) {
	std::cout << "A polyline to maintain:" << std::endl;
	for (Polyline::iterator itp = (*it).begin(); itp != (*it).end(); ++itp) {
		std::cout << *itp << std::endl;
	}

	for (int i = 0; i < (*it).size()-1; i++) {
		std::cout << "Adding edge " << (*it)[i] << ", " << (*it)[i+1] << std::endl;
		CDTXY::Vertex_handle va = m_cdt.insert((*it)[i]);
		CDTXY::Vertex_handle vb = m_cdt.insert((*it)[i+1]);
		m_cdt.insert_constraint((*it)[i], (*it)[i+1], false);
	}


//	std::cout << "Insert the polyline to maintain" << std::endl;
//        m_cdt.insert_constraint((*it).begin(), (*it).end(), false);
//	std::cout << "Insert the polyline to maintain DONE" << std::endl;
    }
std::cout << "icsp: insert the border polylines that need to be maintained as they are DONE" << std::endl;
*/

    if (!constrainEasternVertices) {
        // Add as a constraint (will be simplified by PS::simplify())
        m_cdt.insert_constraint(easternBorderVertices.begin(), easternBorderVertices.end(), false);
    }
    else {
        // Add as regular vertices in the triangulation: since they are in the borders, the edges will be maintained!
		for (Polyline::iterator it = easternBorderVertices.begin(); it != easternBorderVertices.end(); ++it)
			m_cdt.insert(*it);
    }
    if (!constrainWesternVertices) {
        // Add as a constraint (will be simplified by PS::simplify())
        m_cdt.insert_constraint(westernBorderVertices.begin(), westernBorderVertices.end(), false);
    }
    else {
        // Add as regular vertices in the triangulation: since they are in the borders, the edges will be maintained!
		for (Polyline::iterator it = westernBorderVertices.begin(); it != westernBorderVertices.end(); ++it)
			m_cdt.insert(*it);
    }
    if (!constrainSouthernVertices) {
        // Add as a constraint (will be simplified by PS::simplify())
        m_cdt.insert_constraint(southernBorderVertices.begin(), southernBorderVertices.end(), false);
    }
    else {
        // Add as regular vertices in the triangulation: since they are in the borders, the edges will be maintained!
		for (Polyline::iterator it = southernBorderVertices.begin(); it != southernBorderVertices.end(); ++it)
			m_cdt.insert(*it);
    }
    if (!constrainNorthernVertices) {
        // Add as a constraint (will be simplified by PS::simplify())
        m_cdt.insert_constraint(northernBorderVertices.begin(), northernBorderVertices.end(), false);
    }
    else {
        // Add as regular vertices in the triangulation: since they are in the borders, the edges will be maintained!
		for (Polyline::iterator it = northernBorderVertices.begin(); it != northernBorderVertices.end(); ++it)
			m_cdt.insert(*it);
    }

    if (m_preserveSharpEdges) {
        // Create a property map storing if an edge is sharp or not (since the Polyhedron_3 does not have internal property_maps creation, we use a map container within a boost::associative_property_map)
        typedef typename boost::graph_traits<Polyhedron>::edge_descriptor edge_descriptor;
        typedef typename std::map<edge_descriptor, bool> EdgeIsSharpMap;
        typedef typename boost::associative_property_map<EdgeIsSharpMap> EdgeIsSharpPropertyMap;
        EdgeIsSharpMap map;
        EdgeIsSharpPropertyMap eisMap(map);

        // Detect sharp edges
        detect_sharp_edges_without_borders<Polyhedron, double, EdgeIsSharpPropertyMap, K>(surface, FT(60.0), eisMap);

        // Trace the polylines from the detected edges
        Polylines featurePolylines;
        extract_polylines_from_sharp_edges(surface, eisMap, featurePolylines);

        for (Polylines::const_iterator it = featurePolylines.begin(); it != featurePolylines.end(); ++it) {
            if ((*it).size() > m_minFeaturePolylineSize) {
                //	    std::cout << "A polyline to simplify:" << std::endl;
                //	    for (Polyline::const_iterator itp = (*it).begin(); itp != (*it).end(); ++itp)
                //		std::cout << *itp << std::endl;
                m_cdt.insert_constraint((*it).begin(), (*it).end(), false);
            }
        }
    }

    // Simplify the polylines
    std::size_t numRemoved = PS::simplify(m_cdt, PSSqDist3Cost(m_borderSimpMaxLengthPercent), PSStopCost(m_borderSimpMaxScaledSqDist), true);
}



void
TinCreationSimplificationPointSet::
getAllNonBorderVertices(const Polyhedron& poly, PointCloud& nonBorderPts) const
{
    Polyhedron::Vertex_const_iterator vi = poly.vertices_begin() ;
    for (; vi != poly.vertices_end(); ++vi ) {
        if (!isBorder<Polyhedron>(vi))
            nonBorderPts.push_back(vi->point());
    }
}

} // End namespace TinCreation
