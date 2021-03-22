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

/**
 * \brief Extension of CGAL's Constrained_placement class
 *
 * Extension of CGAL's Constrained_placement class in the CGAL/Surface_mesh_simplification/Policies/Edge_collapse/Constrained_placement.h file.
 * The placement of the vertex resulting from a contraction of an edge containing a constrained vertex is that constrained vertex.
 *
 */

#ifndef EMODNET_QMGC_CGAL_FURTHER_CONSTRAINED_PLACEMENT_H
#define EMODNET_QMGC_CGAL_FURTHER_CONSTRAINED_PLACEMENT_H

#include <CGAL/Surface_mesh_simplification/Detail/Common.h>
#include <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/Edge_profile.h>
#include <iostream>



namespace CGAL {

    namespace Surface_mesh_simplification {

        /**
         * @class FurtherConstrainedPlacement
         *
         * @brief Constrained placement class for CGAL::Surface_mesh_simplification, extending the normal constrained placement
         * to allow maintaining border vertices as well as corners in a tile
         */
        template<class BasePlacement, class SurfaceMesh, class EdgeIsConstrainedMap, class VertexIsConstrainedMap>
        class FurtherConstrainedPlacement : public BasePlacement {
        public:

            EdgeIsConstrainedMap Edge_is_constrained_map;
            VertexIsConstrainedMap Vertex_is_constrained_map;
            bool m_constrainEasternVertices;
            bool m_constrainWesternVertices;
            bool m_constrainNorthernVertices;
            bool m_constrainSouthernVertices;

        public:
            FurtherConstrainedPlacement(
                    EdgeIsConstrainedMap edgesMap = EdgeIsConstrainedMap(),
                    VertexIsConstrainedMap vertMap = VertexIsConstrainedMap(),
                    BasePlacement base = BasePlacement(),
                    const bool& constrainEasternVertices = false,
                    const bool& constrainWesternVertices = false,
                    const bool& constrainNorthernVertices = false,
                    const bool& constrainSouthernVertices = false)
                    : BasePlacement(base)
                    , Edge_is_constrained_map(edgesMap)
                    , Vertex_is_constrained_map(vertMap)
                    , m_constrainEasternVertices(constrainEasternVertices)
                    , m_constrainWesternVertices(constrainWesternVertices)
                    , m_constrainNorthernVertices(constrainNorthernVertices)
                    , m_constrainSouthernVertices(constrainSouthernVertices) {}

            template<typename Profile>
            optional<typename Profile::Point> operator()(Profile const &aProfile) const {
              //typedef typename Profile::ECM ECM;
                typedef typename CGAL::Halfedge_around_target_iterator<SurfaceMesh> in_edge_iterator;

                // Constrained vertices
                bool isConstrainedV0 = get(Vertex_is_constrained_map, aProfile.v0()) ;
                bool isConstrainedV1 = get(Vertex_is_constrained_map, aProfile.v1()) ;

                if ( isConstrainedV0 && isConstrainedV1 ) {
                    // Both vertices in the edge are corners (will this ever happen?): the edge must remain as is, no placement possible
                    return boost::optional<typename Profile::Point>();
                }

                // This is implicitly tested in the BasePlacement when the Edge_is_constrained_map is set, but we do it here because we need to check it before the test on the corners
                if (get(Edge_is_constrained_map, edge(aProfile.v0_v1(), aProfile.surface_mesh()))) {
                    // The edge is constrained: the edge must remain as is, no placement possible
                    return boost::optional<typename Profile::Point>();
                }

                if ( isConstrainedV0 ) {
                    // v0 is constrained, return it as the placement
                    typename Profile::Point p = get(aProfile.vertex_point_map(), aProfile.v0()) ;
                    return get(aProfile.vertex_point_map(), aProfile.v0()) ;
                }
                else if ( isConstrainedV1 ) {
                    // v1 is constrained, return it as the placement
                    typename Profile::Point p = get(aProfile.vertex_point_map(), aProfile.v1()) ;
                    return get(aProfile.vertex_point_map(), aProfile.v1()) ;
                }

                // Constrained edges
                in_edge_iterator eb, ee;
                for (boost::tie(eb, ee) = halfedges_around_target(aProfile.v0(), aProfile.surface_mesh());
                     eb != ee; ++eb) {
                    if (get(Edge_is_constrained_map, edge(*eb, aProfile.surface_mesh()))) {
                        return aProfile.p0();
                    }
                }
                for (boost::tie(eb, ee) = halfedges_around_target(aProfile.v1(), aProfile.surface_mesh());
                     eb != ee; ++eb) {
                    if (get(Edge_is_constrained_map, edge(*eb, aProfile.surface_mesh()))) {
                        return aProfile.p1();
                    }
                }

                // Get the result of the base placement
                boost::optional<typename Profile::Point> op = static_cast<const BasePlacement *>(this)->operator()(aProfile);

                // If some placement is computed, check its validity
                if(op) {
                    typedef typename Profile::Point Point;
                    Point q2 = *op; // <-- The candidate placement

                    // Check if the placement result is in bounds
                    // Quick test: is it within the limits?
                    if (q2.x() < 0 || q2.x() > 1 || q2.y() < 0 || q2.y() > 1 )
                        return boost::optional<typename Profile::Point>();

                    // Check if the placement is in a constrained border
                    if (q2.x() == 0 && m_constrainWesternVertices)
                        return boost::optional<typename Profile::Point>();
                    if (q2.x() == 1 && m_constrainEasternVertices)
                        return boost::optional<typename Profile::Point>();
                    if (q2.y() == 0 && m_constrainSouthernVertices)
                        return boost::optional<typename Profile::Point>();
                    if (q2.y() == 1 && m_constrainNorthernVertices)
                        return boost::optional<typename Profile::Point>();
                }
                return op;
//                return static_cast<const BasePlacement *>(this)->operator()(aProfile);
            }
        };

    }
}
#endif //EMODNET_QMGC_CGAL_FURTHER_CONSTRAINED_PLACEMENT_H
