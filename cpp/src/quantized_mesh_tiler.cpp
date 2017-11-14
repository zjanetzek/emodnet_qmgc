//
// Created by Ricard Campos (rcampos@eia.udg.edu)
//

#include "quantized_mesh_tiler.h"
#include <GeographicLib/Geocentric.hpp>
#include <algorithm>
#include "cgal_defines.h"
#include "cgal_western_and_southern_border_edges_are_constrained_edge_map.h"
#include "cgal_corner_vertices_are_constrained_vertex_map.h"
#include "cgal_further_constrained_placement.h"
#include <CGAL/Surface_mesh_simplification/Policies/Edge_collapse/Constrained_placement.h>
#include <CGAL/centroid.h>
#include <cmath>
#include "forsyth-too/forsythtriangleorderoptimizer.h"
#include "meshoptimizer/meshoptimizer.h"



QuantizedMeshTile* QuantizedMeshTiler::createTile( const ctb::TileCoordinate &coord,
                                                   std::vector<Point_3> &tileWestVertices,
                                                   std::vector<Point_3> &tileSouthVertices ) const
{
    // Get a terrain tile represented by the tile coordinate
    QuantizedMeshTile *qmTile = new QuantizedMeshTile(coord);
    ctb::GDALTile *rasterTile = createRasterTile(coord); // the raster associated with this tile coordinate
    GDALRasterBand *heightsBand = rasterTile->dataset->GetRasterBand(1);
    double resolution;
    ctb::CRSBounds tileBounds = terrainTileBounds(coord, resolution);

    // Copy the raster data into an array
    float rasterHeights[heightsBand->GetXSize()*heightsBand->GetYSize()];
    if (heightsBand->RasterIO(GF_Read, 0, 0, heightsBand->GetXSize(), heightsBand->GetYSize(),
                              (void *) rasterHeights,
                              heightsBand->GetXSize(), heightsBand->GetYSize(),
                              GDT_Float32, 0, 0) != CE_None) {
        throw ctb::CTBException("Could not read heights from raster");
    }

    // Create a base triangulation (using Delaunay) with all the raster info available
    std::vector< Point_3 > heightMapPoints ;
    std::vector< Point_3 > latLonPoints ;
    float minHeight =  std::numeric_limits<float>::infinity() ;
    float maxHeight = -std::numeric_limits<float>::infinity() ;

    // Check the start of the rasters: if there are constrained vertices from neighboring tiles to maintain,
    // the western and/or the southern vertices are not touched, and thus we should parse the raster starting from index 1
    bool constrainWestVertices = tileWestVertices.size() > 0 ;
    bool constrainSouthVertices = tileSouthVertices.size() > 0 ;

    int startX = constrainWestVertices? 1: 0 ;
    int endY = constrainSouthVertices? heightsBand->GetYSize()-1: heightsBand->GetYSize() ;

    for ( int i = startX; i < heightsBand->GetXSize(); i++ ) {
        for (int j = 0; j < endY; j++) {
            int y = heightsBand->GetYSize() - 1 - j; // y coordinate within the tile.
            // Note that the heights in RasterIO have the origin in the upper-left corner,
            // while the tile has it in the lower-left. Obviously, x = i

            // Height value
            float height = rasterHeights[j * heightsBand->GetXSize() + i];

            // **** Temp workaround for emodnet data (start) ****
            if ( height < 0 )
                height = 0 ;
            height = -height ;
            // **** Temp workaround for emodnet data (end) ****

            // Update max/min values
            if (height < minHeight)
                minHeight = height;
            if (height > maxHeight)
                maxHeight = height;

            // In heightmap format
            heightMapPoints.push_back(Point_3(i, y, height));

//            if (constrainSouthVertices)
//                std::cout << "p = " << i << ", " << y << " added from raster" << std::endl ;

            // In Latitude, Longitude, Height format
            float lat = tileBounds.getMinY() + ((tileBounds.getMaxY() - tileBounds.getMinY()) * ((float)y/((float)heightsBand->GetYSize()-1)));
            float lon = tileBounds.getMinX() + ((tileBounds.getMaxX() - tileBounds.getMinX()) * ((float)i/((float)heightsBand->GetXSize()-1)));
            latLonPoints.push_back(Point_3(lat, lon, height));
        }
    }

    // Also, add the vertices to preserve from neighboring tiles
    if ( constrainWestVertices ) {
//        std::cout << "Inserting west vertices" << std::endl ;
        for ( std::vector<Point_3>::iterator it = tileWestVertices.begin(); it != tileWestVertices.end(); ++it ) {
            // In heightmap format
            heightMapPoints.push_back(*it);

            // Update max/min height
            if (it->z() < minHeight)
                minHeight = it->z();
            if (it->z() > maxHeight)
                maxHeight = it->z();

//            std::cout << it->x() << ", " << it->y() << ", " << it->z() << std::endl ;

            // In Latitude, Longitude, Height format
            float lat = tileBounds.getMinY() + ((tileBounds.getMaxY() - tileBounds.getMinY()) * ((float)it->y()/((float)heightsBand->GetYSize()-1)));
            float lon = tileBounds.getMinX() + ((tileBounds.getMaxX() - tileBounds.getMinX()) * ((float)it->x()/((float)heightsBand->GetXSize()-1)));
            latLonPoints.push_back(Point_3(lat, lon, it->z()));
        }
    }
    if ( constrainSouthVertices ) {
//        std::cout << "Inserting south vertices" << std::endl ;
        for ( std::vector<Point_3>::iterator it = tileSouthVertices.begin(); it != tileSouthVertices.end(); ++it ) {

            // Skip (0,0) corner if it was already included
//            if ( constrainWestVertices &&
//                 it->x() <= std::numeric_limits<double>::epsilon() &&
//                 it->y() <= std::numeric_limits<double>::epsilon() ) {
//                //std::cout << "(0, 0) southern corner not included" << std::endl ;
//            }
//            else {
                // In heightmap format
                heightMapPoints.push_back(*it);

                // Update max/min height
                if (it->z() < minHeight)
                    minHeight = it->z();
                if (it->z() > maxHeight)
                    maxHeight = it->z();

//                std::cout << it->x() << ", " << it->y() << ", " << it->z() << std::endl ;
                // In Latitude, Longitude, Height format
                float lat = tileBounds.getMinY() + ((tileBounds.getMaxY() - tileBounds.getMinY()) *
                                                    ((float) it->y() / ((float) heightsBand->GetYSize() - 1)));
                float lon = tileBounds.getMinX() + ((tileBounds.getMaxX() - tileBounds.getMinX()) *
                                                    ((float) it->x() / ((float) heightsBand->GetXSize() - 1)));
                latLonPoints.push_back(Point_3(lat, lon, it->z()));
//            }
        }
    }

    // --- Set all the info in the quantized mesh format ---

    // --> Header part

    QuantizedMesh::Header header ;
    header.MinimumHeight = minHeight ;
    header.MaximumHeight = maxHeight ;

    // Points in ECEF coordinates
    std::vector< Point_3 > ecefPoints;
    ecefPoints.reserve(latLonPoints.size());
    double minEcefX = std::numeric_limits<double>::infinity();
    double maxEcefX = -std::numeric_limits<double>::infinity();
    double minEcefY = std::numeric_limits<double>::infinity();
    double maxEcefY = -std::numeric_limits<double>::infinity();
    double minEcefZ = std::numeric_limits<double>::infinity();
    double maxEcefZ = -std::numeric_limits<double>::infinity();
    for ( int i = 0; i < latLonPoints.size(); i++ ) {
        GeographicLib::Geocentric earth(GeographicLib::Constants::WGS84_a(), GeographicLib::Constants::WGS84_f()) ;
        double tmpx, tmpy, tmpz ;
        earth.Forward( (float)latLonPoints[i].x(), (float)latLonPoints[i].y(), (float)latLonPoints[i].z(), tmpx, tmpy, tmpz ) ;
        ecefPoints.push_back( Point_3( tmpx, tmpy, tmpz )) ;

        if( tmpx < minEcefX )
            minEcefX = tmpx ;
        if (tmpx > maxEcefX )
            maxEcefX = tmpx ;
        if( tmpy < minEcefY )
            minEcefY = tmpy ;
        if (tmpy > maxEcefY )
            maxEcefY = tmpy ;
        if( tmpz < minEcefZ )
            minEcefZ = tmpz ;
        if (tmpz > maxEcefZ )
            maxEcefZ = tmpz ;
    }

////     Get the middle point as the center of the bounding box (lat/lon -> ecef)
//    double midLon = tileBounds.getMinX() + ( ( tileBounds.getMaxX() - tileBounds.getMinX() ) / 2 ) ;
//    double midLat = tileBounds.getMinY() + ( ( tileBounds.getMaxY() - tileBounds.getMinY() ) / 2 ) ;
//    double midH = minHeight + ( ( maxHeight - minHeight ) / 2 ) ;
//// Convert to ECEF and store on the structure
//    GeographicLib::Geocentric earth(GeographicLib::Constants::WGS84_a(), GeographicLib::Constants::WGS84_f()) ;
//    earth.Forward( midLat, midLon, midH, header.CenterX, header.CenterY, header.CenterZ ) ;

    // Get the middle point as the the center of the bounding box (from ecef directly)
    header.CenterX = minEcefX + ( ( maxEcefX - minEcefX ) / 2 ) ;
    header.CenterY = minEcefY + ( ( maxEcefY - minEcefY ) / 2 ) ;
    header.CenterZ = minEcefZ + ( ( maxEcefZ - minEcefZ ) / 2 ) ;

    // Compute the minimum bounding sphere given the points
    MinSphere ms( ecefPoints.begin(), ecefPoints.end() ) ;
    header.BoundingSphereRadius = CGAL::to_double( ms.radius() ) ;
    header.BoundingSphereCenterX = CGAL::to_double( *ms.center_cartesian_begin() ) ;
    header.BoundingSphereCenterY = CGAL::to_double( *(ms.center_cartesian_begin()+1) ) ;
    header.BoundingSphereCenterZ = CGAL::to_double( *(ms.center_cartesian_begin()+2) ) ;

    // Compute the horizon occlusion point
    // Explanation of horizonOcclusion in: https://cesium.com/blog/2013/04/25/horizon-culling/
    // and: https://groups.google.com/forum/#!topic/cesium-dev/8NTW1Wl0d8s
    // Note: The test point for occlusion is scaled within the WGS84 ellipsoid
    Point_3 hop = QuantizedMesh::horizonOcclusionPoint( ecefPoints, Point_3(header.CenterX, header.CenterY, header.CenterZ) ) ;
    header.HorizonOcclusionPointX = hop.x() ;
    header.HorizonOcclusionPointY = hop.y() ;
    header.HorizonOcclusionPointZ = hop.z() ;

    qmTile->setHeader(header) ;

    // --> Create connectivity

    // Encode the points extracted from the raster in u/v/height values in the range [0..1]
    // We simplify the mesh in u/v/h format because they are normalized values and the surface will be better
    // conditioned for simplification
    std::vector< Point_3 > uvhPts ;
    for ( std::vector<Point_3>::iterator it = heightMapPoints.begin(); it != heightMapPoints.end(); ++it ) {
//        unsigned short u = QuantizedMesh::remapToVertexDataValue( CGAL::to_double(it->x()), 0, heightsBand->GetXSize()-1) ;
//        unsigned short v = QuantizedMesh::remapToVertexDataValue( CGAL::to_double(it->y()), 0, heightsBand->GetYSize()-1) ;
//        unsigned short h = QuantizedMesh::remapToVertexDataValue( CGAL::to_double(it->z()), minHeight, maxHeight ) ;
//
//        uvhPts.push_back( Point_3( static_cast<double>(u),
//                                   static_cast<double>(v),
//                                   static_cast<double>(h) ) ) ;
        double u = QuantizedMesh::remap( CGAL::to_double(it->x()), 0.0, heightsBand->GetXSize()-1, 0.0, 1.0 ) ;
        double v = QuantizedMesh::remap( CGAL::to_double(it->y()), 0.0, heightsBand->GetYSize()-1, 0.0, 1.0 ) ;
        double h = QuantizedMesh::remap( CGAL::to_double(it->z()), minHeight, maxHeight, 0.0, 1.0 ) ;

        if (h > 1.0)
            std::cout << "h0 = " << h << std::endl ;

        uvhPts.push_back( Point_3( u, v, h ) ) ;
    }

    // Delaunay triangulation
    Delaunay dt( uvhPts.begin(), uvhPts.end() );

    // --- Debug ---
//    delaunayToOFF("./" + std::to_string(coord.zoom) + "_" + std::to_string(coord.x) + "_" + std::to_string(coord.y) + "_dt.off", dt) ;

    // Translate to Polyhedron
    Polyhedron surface ;
    PolyhedronBuilder<Gt, HalfedgeDS> builder(dt);
    surface.delegate(builder);

    // Set up the edge constrainer
    typedef SMS::FurtherConstrainedPlacement<SimplificationPlacement,
                                        WesternAndSouthernBorderEdgesAreConstrainedEdgeMap,
                                        CornerVerticesAreConstrainedVertexMap > SimplificationConstrainedPlacement ;
    WesternAndSouthernBorderEdgesAreConstrainedEdgeMap wsbeac(surface, constrainWestVertices, constrainSouthVertices);
    CornerVerticesAreConstrainedVertexMap cvacvm(surface) ;
    SimplificationConstrainedPlacement scp( wsbeac, cvacvm ) ;

//    typedef SMS::Constrained_placement<SimplificationPlacement, WesternAndSouthernBorderEdgesAreConstrainedEdgeMap > Placement;
//    Placement pl(wsbeac) ;

//    std::cout << "Simplifying" << std::endl ;
//    if (constrainWestVertices)
//        std::cout << "with west constraints" << std::endl ;
//    if (constrainSouthVertices)
//        std::cout << "with south constraints" << std::endl ;


    int r = SMS::edge_collapse
            ( surface, SimplificationStopPredicate(0.05),
              CGAL::parameters::vertex_index_map( get( CGAL::vertex_external_index,surface ) )
                      .halfedge_index_map(get(CGAL::halfedge_external_index, surface))
                      .get_cost(SimplificationCost())
//                      .get_placement(pl)
//                      .get_placement(SimplificationPlacement())
                      .get_placement(scp)
                      .edge_is_constrained_map(wsbeac)
            ) ;

//    std::cout << "done" << std::endl ;

    // [DEBUG] Write the simplified polyhedron to file
//    std::ofstream os("./" + std::to_string(coord.zoom) + "_" + std::to_string(coord.x) + "_" + std::to_string(coord.y) + "_simp.off") ;
//    os << surface;
//    os.close();

    // --> VertexData part

    std::vector<unsigned short> vertices ;
    int numVertices = surface.size_of_vertices() ;
    vertices.reserve(numVertices*3) ;
    for ( Polyhedron::Point_iterator it = surface.points_begin(); it != surface.points_end(); ++it ) {
//        unsigned short u = static_cast<unsigned short>( it->x() ) ;
//        unsigned short v = static_cast<unsigned short>( it->y() ) ;
//        unsigned short h = static_cast<unsigned short>( it->z() ) ;

        double x = CGAL::to_double(it->x()) ;
        double y = CGAL::to_double(it->y()) ;
        double z = CGAL::to_double(it->z()) ;

        // Truncate values (after simplification, values might be smaller than 0.0 or larger than 1.0
        x = x < 0.0? 0.0 : x ;
        x = x > 1.0? 1.0 : x ;
        y = y < 0.0? 0.0 : y ;
        y = y > 1.0? 1.0 : y ;
        z = z < 0.0? 0.0 : z ;
        z = z > 1.0? 1.0 : z ;

        unsigned short u = QuantizedMesh::remapToVertexDataValue( x, 0.0, 1.0 ) ;
        unsigned short v = QuantizedMesh::remapToVertexDataValue( y, 0.0, 1.0 ) ;
        unsigned short h = QuantizedMesh::remapToVertexDataValue( z, 0.0, 1.0 ) ;

        if (u > QuantizedMesh::MAX_VERTEX_DATA )
            std::cout << "u = " << u << "; x = " << CGAL::to_double(it->x()) << std::endl ;
        if (v > QuantizedMesh::MAX_VERTEX_DATA )
            std::cout << "v = " << v << "; y = " << CGAL::to_double(it->y()) << std::endl ;
        if (h > QuantizedMesh::MAX_VERTEX_DATA )
            std::cout << "h = " << h << "; z = " << CGAL::to_double(it->z()) << std::endl ;

        vertices.push_back(u) ;
        vertices.push_back(v) ;
        vertices.push_back(h) ;
    }

    // --> IndexData part
    QuantizedMesh::IndexData indexData ;

    indexData.triangleCount = surface.size_of_facets() ;
    indexData.indices.reserve(indexData.triangleCount*3) ;
    for ( Polyhedron::Facet_iterator it = surface.facets_begin(); it != surface.facets_end(); ++it) {
        Polyhedron::Halfedge_around_facet_circulator j = it->facet_begin();
        // Facets in our polyhedral surface should be triangles
        CGAL_assertion( CGAL::circulator_size(j) == 3);
        // Extract integer indices
        do {
            indexData.indices.push_back( static_cast<unsigned int>( std::distance(surface.vertices_begin(), j->vertex()) ) );
        } while ( ++j != it->facet_begin() );
    }

    // Optimize the resulting mesh (in order to be able to codify the indices using the "high watermark" method required
    // by the quantized-mesh format, we need to optimize the vertex indices for the cache and fetch
    meshopt_optimizeVertexCache(&indexData.indices[0], &indexData.indices[0], indexData.indices.size(), numVertices, 32 ) ; // Last number is the virtual cache size
    std::vector<unsigned int> vertexRemap(numVertices, ~0u);
    meshopt_optimizeVertexFetch(&vertices[0], &indexData.indices[0], indexData.indices.size(), &vertices[0], numVertices, sizeof(unsigned short)*3, &vertexRemap[0] );

    // Store optimized vertices and indices
    QuantizedMesh::VertexData vertexData ;
    vertexData.vertexCount = surface.size_of_vertices() ;
    vertexData.u.reserve(vertexData.vertexCount) ;
    vertexData.v.reserve(vertexData.vertexCount) ;
    vertexData.height.reserve(vertexData.vertexCount) ;
    for ( int i = 0; i < vertices.size(); i=i+3 ) {
        vertexData.u.push_back(vertices[i]) ;
        vertexData.v.push_back(vertices[i+1]) ;
        vertexData.height.push_back(vertices[i+2]) ;
    }
    qmTile->setVertexData(vertexData) ;
    qmTile->setIndexData(indexData) ;

    // --> EdgeIndices part (also collect the vertices to maintain for this tile)
    QuantizedMesh::EdgeIndices edgeIndices ;
    edgeIndices.westIndices = std::vector<unsigned int>() ;
    edgeIndices.southIndices = std::vector<unsigned int>() ;
    edgeIndices.eastIndices = std::vector<unsigned int>() ;
    edgeIndices.northIndices = std::vector<unsigned int>() ;
    tileWestVertices.clear() ;
    tileSouthVertices.clear() ;
    Point_2 tileCenter( 0.5, 0.5 ) ;
    Point_2 tileLowerLeftCorner( 0.0, 0.0 ) ;
    Point_2 tileUpperLeftCorner( 0.0, 1.0 ) ;
    Point_2 tileLowerRightCorner( 1.0, 0.0 ) ;
    Point_2 tileUpperRightCorner( 1.0, 1.0 ) ;

    surface.normalize_border() ; // Important call! Sorts halfedges such that the non-border edges precede the border edges

    // According to the CGAL documentation, normalization: "reorganizes the sequential storage of the edges such that the
    // non-border edges precede the border edges, and that for each border edge the latter one of the two halfedges is a
    // border halfedge (the first one is a non-border halfedge in conformance with the polyhedral surface definition)"
    // Thus, we move along halfedges with an increment of 2
    int numCorners = 0 ;

    Polyhedron::Halfedge_iterator e = surface.border_halfedges_begin() ;
    ++e ; // We start at the second halfedge!
//    for ( ; e != surface.halfedges_end(); std::advance(e,2) )
    while( e->is_border() )
    {
        // Get the vertex index in the surface structure
        unsigned int vertInd = static_cast<unsigned int>( std::distance(surface.vertices_begin(), e->vertex() ) ) ;
        // Re-map the vertex index to the new index after optimizing for vertex fetching
        vertInd = vertexRemap[vertInd] ;

        // Relevant geometric info of the current edge
        Point_3 p0 = e->vertex()->point() ; // This is the point we will take care of now
        Point_3 p1 = e->prev()->vertex()->point() ; // This is the previous vertex, with which p0 forms an edge

        // Differences between the points in the edge
        double diffX = fabs( p1.x() - p0.x() ) ;
        double diffY = fabs( p1.y() - p0.y() ) ;

        // Check if it is a corner point: the next vertex changes from vertical to horizontal or viceversa
        // If it is a corner point, we should add it twice to the corresponding border

        // Next edge on the border (since we are in a border halfedge, the next operator points to the next halfedge around the "hole"
        Point_3 p2 = e->next()->vertex()->point() ;

        double diffXNext = fabs( p2.x() - p0.x() ) ;
        double diffYNext = fabs( p2.y() - p0.y() ) ;
        bool isCorner = ( ( diffX < diffY ) && ( diffXNext > diffYNext ) ) ||
                        ( ( diffX > diffY ) && ( diffXNext < diffYNext ) ) ;

        int timesInserted = 0;
        if ( isCorner ) {
            std::cout << "Corner point = (" << p0.x() << ", " << p0.y() << ")" << std::endl ;
            numCorners++ ;
            if ( p0.x() < 0.5 && p0.y() < 0.5 ) { // Corner (0, 0)
                edgeIndices.westIndices.push_back(vertInd);
                edgeIndices.southIndices.push_back(vertInd);

                timesInserted = 2 ;
            }
            else if ( p0.x() < 0.5 && p0.y() > 0.5 ) { // Corner (0, 1)
                edgeIndices.westIndices.push_back(vertInd);
                edgeIndices.northIndices.push_back(vertInd);

                // Northern vertex turns into a southern vertex to preserve for the next tile
                // double x = QuantizedMesh::remap(p0.x(), 0.0, 1.0, 0.0, heightsBand->GetXSize() - 1); // The data must be converted back to double... because the ranges for height depend on min/max height for each tile and we need to add this vertices as part of the vertices of the new tile, in other bounds
                double x = 0.0; // The data must be converted back to double... because the ranges for height depend on min/max height for each tile and we need to add this vertices as part of the vertices of the new tile, in other bounds
                double y = 0.0; // North to south conversion (y=0)
                double h = QuantizedMesh::remap(p0.z(), 0.0, 1.0, minHeight, maxHeight); // The data must be converted back to double... because the ranges for height depend on min/max height for each tile and we need to add this vertices as part of the vertices of the new tile, in other bounds
                tileSouthVertices.push_back(Point_3(x, y, h));

                timesInserted = 2 ;
            }
            else if ( p0.x() > 0.5 && p0.y() > 0.5 ) { // Corner (1, 1)
                edgeIndices.northIndices.push_back(vertInd);
                edgeIndices.eastIndices.push_back(vertInd);

                // Eastern vertex turns into a western vertex to preserve for the next tile
                double x = 0.0; // East to west conversion (x=0)
                //double y = QuantizedMesh::remap(p0.y(), 0.0, 1.0, 0.0, heightsBand->GetYSize() - 1);
                double y = heightsBand->GetYSize() - 1;
                double h = QuantizedMesh::remap(p0.z(), 0.0, 1.0, minHeight, maxHeight);
                tileWestVertices.push_back(Point_3(x, y, h));

                // Northern vertex turns into a southern vertex to preserve for the next tile
                //x = QuantizedMesh::remap(p0.x(), 0.0, 1.0, 0.0, heightsBand->GetXSize() - 1); // The data must be converted back to double... because the ranges for height depend on min/max height for each tile and we need to add this vertices as part of the vertices of the new tile, in other bounds
                x = heightsBand->GetXSize() - 1; // The data must be converted back to double... because the ranges for height depend on min/max height for each tile and we need to add this vertices as part of the vertices of the new tile, in other bounds
                y = 0.0;
                h = QuantizedMesh::remap(p0.z(), 0.0, 1.0, minHeight, maxHeight); // The data must be converted back to double... because the ranges for height depend on min/max height for each tile and we need to add this vertices as part of the vertices of the new tile, in other bounds
                tileSouthVertices.push_back(Point_3(x, y, h));

                timesInserted = 2 ;
            }
            else { // p0.x() > 0.5 && p0.y() < 0.5 ) // Corner (1, 0)
                edgeIndices.eastIndices.push_back(vertInd);
                edgeIndices.southIndices.push_back(vertInd);

                // Eastern vertex turns into a western vertex to preserve for the next tile
                double x = 0.0; // East to west conversion (x=0)
                // double y = QuantizedMesh::remap(p0.y(), 0.0, 1.0, 0.0, heightsBand->GetYSize() - 1);
                double y = 0.0;
                double h = QuantizedMesh::remap(p0.z(), 0.0, 1.0, minHeight, maxHeight);
                tileWestVertices.push_back(Point_3(x, y, h));

                timesInserted = 2 ;
            }
        }
        else {
            if (diffX < diffY) {
                // Vertical edge, can be a western or eastern edge
                if (p0.x() < 0.5) {
                    // Western border edge/vertex
                    edgeIndices.westIndices.push_back(vertInd);
                    timesInserted++;
                } else { // p0.x() >= 0.5
                    // Eastern border vertex
                    edgeIndices.eastIndices.push_back(vertInd);

                    // The data must be converted back to double... because the ranges for height depend on min/max height for each tile and we need to add this vertices as part of the vertices of the new tile, in other bounds
                    double x = 0.0; // West to east conversion (x=0)
                    double y = QuantizedMesh::remap(p0.y(), 0.0, 1.0, 0.0, heightsBand->GetYSize() - 1);
                    double h = QuantizedMesh::remap(p0.z(), 0.0, 1.0, minHeight, maxHeight);
                    tileWestVertices.push_back(Point_3(x, y, h));

                    timesInserted++;
                }
            } else { // diffX >= diffY
                // Horizontal edge, can be a northern or southern edge
                if (p0.y() < 0.5) {
                    // Southern border edge/vertex
                    edgeIndices.southIndices.push_back(vertInd);
                    timesInserted++;
                } else { // p0.y() >= 0.5
                    // Northern border edge/vertex
                    edgeIndices.northIndices.push_back(vertInd);

                    //            // The data must be converted back to double... because the ranges for height depend on min/max height for each tile and we need to add this vertices as part of the vertices of the new tile, in other bounds
                    //            double h = QuantizedMesh::remapFromVertexDataValue(p.z(), minHeight, maxHeight);
                    //            double x = QuantizedMesh::remapFromVertexDataValue(p.x(), 0, heightsBand->GetXSize()-1);
                    ////                double y = QuantizedMesh::remapFromVertexDataValue(0.0, 0, heightsBand->GetYSize()-1); // North to south conversion (y=0)
                    //            double y = 0.0 ;
                    //            tileSouthVertices.push_back(Point_3(x, y, h));

                    double x = QuantizedMesh::remap(p0.x(), 0.0, 1.0, 0.0, heightsBand->GetXSize() - 1);
                    double y = 0.0; // North to south conversion
                    double h = QuantizedMesh::remap(p0.z(), 0.0, 1.0, minHeight, maxHeight);
                    tileSouthVertices.push_back(Point_3(x, y, h));

                    timesInserted++;
                }
            }
        }

//        if ( timesInserted == 0 ) {
//            std::cout << "point p = (" << p0.x() << ", " << p0.y() << ") not included in any case!" << std::endl ;
//        }
//
//        if ( timesInserted > 1 ) {
//            std::cout << "point p = (" << p0.x() << ", " << p0.y() << ") included " << timesInserted << " times" << std::endl ;
//            std::cout << "point p prev = (" << p1.x() << ", " << p1.y() << ")" << std::endl ;
//            std::cout << "point p next = (" << p2.x() << ", " << p2.y() << ")" << std::endl ;
//            std::cout << "diffX = " << diffX << std::endl ;
//            std::cout << "diffY = " << diffY << std::endl ;
//            std::cout << "diffXNext = " << diffXNext << std::endl ;
//            std::cout << "diffYNext = " << diffYNext << std::endl ;
//        }

        // Advance 2 positions (i.e., skip non-border halfedges)
        std::advance(e,2) ;
    }

    if ( numCorners != 4 )
        std::cout << "[ERROR] Not all 4 corners of the tile were detected!" << std::endl ;

    edgeIndices.westVertexCount = edgeIndices.westIndices.size() ;
    edgeIndices.southVertexCount = edgeIndices.southIndices.size() ;
    edgeIndices.eastVertexCount = edgeIndices.eastIndices.size() ;
    edgeIndices.northVertexCount = edgeIndices.northIndices.size() ;

    qmTile->setEdgeIndices(edgeIndices) ;

//    qmTile->printHeader() ;
//    qmTile->print() ;

    // [DEBUG] Export the final tile in OFF format
    qmTile->exportToOFF("./" + std::to_string(coord.zoom) + "_" + std::to_string(coord.x) + "_" + std::to_string(coord.y) + "_simp_qm.off") ;

    delete rasterTile;

    return qmTile;
}


QuantizedMeshTile* QuantizedMeshTiler::createTileNoSimp(const ctb::TileCoordinate &coord ) const
{
    const int tileSize = 65;

    // Get a terrain tile represented by the tile coordinate
    QuantizedMeshTile *qmTile = new QuantizedMeshTile(coord);
    ctb::GDALTile *rasterTile = createRasterTile(coord); // the raster associated with this tile coordinate
    GDALRasterBand *heightsBand = rasterTile->dataset->GetRasterBand(1);
    double resolution;
    ctb::CRSBounds tileBounds = terrainTileBounds(coord, resolution);

    // Copy the raster data into an array
    float rasterHeights[tileSize*tileSize];
    if (heightsBand->RasterIO(GF_Read, 0, 0, 256, 256,
                              (void *) rasterHeights,
                              tileSize, tileSize,
                              GDT_Float32, 0, 0) != CE_None) {
        throw ctb::CTBException("Could not read heights from raster");
    }

    // Create a base triangulation (using Delaunay) with all the raster info available
    std::vector< Point_3 > heightMapPoints ;
    std::vector< Point_3 > latLonPoints ;
    float minHeight =  std::numeric_limits<float>::infinity() ;
    float maxHeight = -std::numeric_limits<float>::infinity() ;

    for ( int i = 0; i < tileSize; i++ ) {
        for (int j = 0; j < tileSize; j++) {
            int y = tileSize - 1 - j; // y coordinate within the tile.
                                      // Note that the heights in RasterIO have the origin in the upper-left corner,
                                      // while the tile has it in the lower-left. Obviously, x = i

            // Height value
            float height = rasterHeights[j * tileSize + i];

            // Update max/min values
            if (height < minHeight)
                minHeight = height;
            if (height > maxHeight)
                maxHeight = height;

            // In heightmap format
            heightMapPoints.push_back(Point_3(i, y, height));

            // In Latitude, Longitude, Height format
            float lat = tileBounds.getMinY() + ((tileBounds.getMaxY() - tileBounds.getMinY()) * ((float)y/((float)tileSize-1)));
            float lon = tileBounds.getMinX() + ((tileBounds.getMaxX() - tileBounds.getMinX()) * ((float)i/((float)tileSize-1)));
            latLonPoints.push_back(Point_3(lat, lon, height));
        }
    }

    // --- Set all the info in the quantized mesh format ---

    // --> Header part

    QuantizedMesh::Header header ;
    header.MinimumHeight = minHeight ;
    header.MaximumHeight = maxHeight ;

    // Points in ECEF coordinates
    std::vector< Point_3 > ecefPoints;
    ecefPoints.reserve(latLonPoints.size());
    double minEcefX = std::numeric_limits<double>::infinity();
    double maxEcefX = -std::numeric_limits<double>::infinity();
    double minEcefY = std::numeric_limits<double>::infinity();
    double maxEcefY = -std::numeric_limits<double>::infinity();
    double minEcefZ = std::numeric_limits<double>::infinity();
    double maxEcefZ = -std::numeric_limits<double>::infinity();
    for ( int i = 0; i < latLonPoints.size(); i++ ) {
        GeographicLib::Geocentric earth(GeographicLib::Constants::WGS84_a(), GeographicLib::Constants::WGS84_f()) ;
        double tmpx, tmpy, tmpz ;
        earth.Forward( (float)latLonPoints[i].x(), (float)latLonPoints[i].y(), (float)latLonPoints[i].z(), tmpx, tmpy, tmpz ) ;
        ecefPoints.push_back( Point_3( tmpx, tmpy, tmpz )) ;

        if( tmpx < minEcefX )
            minEcefX = tmpx ;
        if (tmpx > maxEcefX )
            maxEcefX = tmpx ;
        if( tmpy < minEcefY )
            minEcefY = tmpy ;
        if (tmpy > maxEcefY )
            maxEcefY = tmpy ;
        if( tmpz < minEcefZ )
            minEcefZ = tmpz ;
        if (tmpz > maxEcefZ )
            maxEcefZ = tmpz ;
    }

////     Get the middle point as the center of the bounding box (lat/lon -> ecef)
//    double midLon = tileBounds.getMinX() + ( ( tileBounds.getMaxX() - tileBounds.getMinX() ) / 2 ) ;
//    double midLat = tileBounds.getMinY() + ( ( tileBounds.getMaxY() - tileBounds.getMinY() ) / 2 ) ;
//    double midH = minHeight + ( ( maxHeight - minHeight ) / 2 ) ;
//// Convert to ECEF and store on the structure
//    GeographicLib::Geocentric earth(GeographicLib::Constants::WGS84_a(), GeographicLib::Constants::WGS84_f()) ;
//    earth.Forward( midLat, midLon, midH, header.CenterX, header.CenterY, header.CenterZ ) ;

    // Get the middle point as the the center of the bounding box (from ecef directly)
    header.CenterX = minEcefX + ( ( maxEcefX - minEcefX ) / 2 ) ;
    header.CenterY = minEcefY + ( ( maxEcefY - minEcefY ) / 2 ) ;
    header.CenterZ = minEcefZ + ( ( maxEcefZ - minEcefZ ) / 2 ) ;

    // Compute the minimum bounding sphere given the points
    MinSphere ms( ecefPoints.begin(), ecefPoints.end() ) ;
    header.BoundingSphereRadius = CGAL::to_double( ms.radius() ) ;
    header.BoundingSphereCenterX = CGAL::to_double( *ms.center_cartesian_begin() ) ;
    header.BoundingSphereCenterY = CGAL::to_double( *(ms.center_cartesian_begin()+1) ) ;
    header.BoundingSphereCenterZ = CGAL::to_double( *(ms.center_cartesian_begin()+2) ) ;

    // Compute the horizon occlusion point
    // Explanation of horizonOcclusion in: https://cesium.com/blog/2013/04/25/horizon-culling/
    // and: https://groups.google.com/forum/#!topic/cesium-dev/8NTW1Wl0d8s
    // Note: The test point for occlusion is scaled within the WGS84 ellipsoid
    Point_3 hop = QuantizedMesh::horizonOcclusionPoint( ecefPoints, Point_3(header.CenterX, header.CenterY, header.CenterZ) ) ;
    header.HorizonOcclusionPointX = hop.x() ;
    header.HorizonOcclusionPointY = hop.y() ;
    header.HorizonOcclusionPointZ = hop.z() ;

    qmTile->setHeader(header) ;

    // --> Create connectivity

    // Encode the points in u/v/height format
    // We simplify the mesh in u/v/h format because they are normalized values and the surface will be better
    // conditioned for simplification
    std::vector< Point_3 > uvhPts ;
    for ( std::vector<Point_3>::iterator it = heightMapPoints.begin(); it != heightMapPoints.end(); ++it ) {
        unsigned short u = QuantizedMesh::remapToVertexDataValue( CGAL::to_double(it->x()), 0, tileSize-1) ;
        unsigned short v = QuantizedMesh::remapToVertexDataValue( CGAL::to_double(it->y()), 0, tileSize-1) ;
        unsigned short h = QuantizedMesh::remapToVertexDataValue( CGAL::to_double(it->z()), minHeight, maxHeight ) ;

        uvhPts.push_back( Point_3( static_cast<double>(u),
                                   static_cast<double>(v),
                                   static_cast<double>(h) ) ) ;
    }

    // Delaunay triangulation
    Delaunay dt( uvhPts.begin(), uvhPts.end() );

    // --- Debug ---
//    delaunayToOFF("./" + std::to_string(coord.zoom) + "_" + std::to_string(coord.x) + "_" + std::to_string(coord.y) + "_dt.off", dt) ;

    // Translate to Polyhedron
    Polyhedron surface ;
    PolyhedronBuilder<Gt, HalfedgeDS> builder(dt);
    surface.delegate(builder);

    // [DEBUG] Write the simplified polyhedron to file
//    std::ofstream os("./" + std::to_string(coord.zoom) + "_" + std::to_string(coord.x) + "_" + std::to_string(coord.y) + "_nosimp.off") ;
//    os << surface;
//    os.close();

    // --> VertexData part

    std::vector<unsigned short> vertices ;
    int numVertices = surface.size_of_vertices() ;
    vertices.reserve(numVertices*3) ;
    for ( Polyhedron::Point_iterator it = surface.points_begin(); it != surface.points_end(); ++it ) {
        unsigned short u = static_cast<unsigned short>( it->x() ) ;
        unsigned short v = static_cast<unsigned short>( it->y() ) ;
        unsigned short h = static_cast<unsigned short>( it->z() ) ;

        vertices.push_back(u) ;
        vertices.push_back(v) ;
        vertices.push_back(h) ;
    }

    // --> IndexData part
    QuantizedMesh::IndexData indexData ;

    indexData.triangleCount = surface.size_of_facets() ;
    indexData.indices.reserve(indexData.triangleCount*3) ;
    for ( Polyhedron::Facet_iterator it = surface.facets_begin(); it != surface.facets_end(); ++it) {
        Polyhedron::Halfedge_around_facet_circulator j = it->facet_begin();
        // Facets in our polyhedral surface should be triangles
        CGAL_assertion( CGAL::circulator_size(j) == 3);
        // Extract integer indices
        do {
            indexData.indices.push_back( static_cast<unsigned int>( std::distance(surface.vertices_begin(), j->vertex()) ) );
        } while ( ++j != it->facet_begin() );
    }

    // Optimize the resulting mesh (in order to be able to codify the indices using the "high watermark" method required
    // by the quantized-mesh format, we need to optimize the vertex indices for the cache and fetch
    meshopt_optimizeVertexCache(&indexData.indices[0], &indexData.indices[0], indexData.indices.size(), numVertices, 32 ) ; // Last number is the virtual cache size
    std::vector<unsigned int> vertexRemap(numVertices, ~0u);
    meshopt_optimizeVertexFetch(&vertices[0], &indexData.indices[0], indexData.indices.size(), &vertices[0], numVertices, sizeof(unsigned short)*3, &vertexRemap[0] );

    // Store optimized vertices and indices
    QuantizedMesh::VertexData vertexData ;
    vertexData.vertexCount = surface.size_of_vertices() ;
    vertexData.u.reserve(vertexData.vertexCount) ;
    vertexData.v.reserve(vertexData.vertexCount) ;
    vertexData.height.reserve(vertexData.vertexCount) ;
    for ( int i = 0; i < vertices.size(); i=i+3 ) {
        vertexData.u.push_back(vertices[i]) ;
        vertexData.v.push_back(vertices[i+1]) ;
        vertexData.height.push_back(vertices[i+2]) ;
    }
    qmTile->setVertexData(vertexData) ;
    qmTile->setIndexData(indexData) ;

    // --> EdgeIndices part (also collect the vertices to maintain for this tile)
    QuantizedMesh::EdgeIndices edgeIndices ;
    edgeIndices.westIndices = std::vector<unsigned int>() ;
    edgeIndices.southIndices = std::vector<unsigned int>() ;
    edgeIndices.eastIndices = std::vector<unsigned int>() ;
    edgeIndices.northIndices = std::vector<unsigned int>() ;
    surface.normalize_border() ; // Important call! Sorts halfedges such that the non-border edges precede the border edges
//    const double myEPS = DBL_EPSILON ; // Since the vertices are supposed to be integers, this value can be "large"
    const double pivot = QuantizedMesh::MAX_VERTEX_DATA / 2. ;
    for ( Polyhedron::Edge_iterator e = surface.border_edges_begin(); e != surface.edges_end(); ++e )
    {
        // Get the vertex index in the surface structure
        Point_3 p( e->vertex()->point() ) ;
        int vertInd = (int)std::distance(surface.vertices_begin(), e->vertex() ) ;
        // Re-map the vertex index to the new index after optimizing for vertex fetching
        vertInd = vertexRemap[vertInd] ;

        if( p.x() < pivot ) {
            // Western border vertex
            edgeIndices.westIndices.push_back( vertInd ) ;
        }
        if ( p.x() > pivot ) {
            // Eastern border vertex
            edgeIndices.eastIndices.push_back( vertInd ) ;
        }
        if ( p.y() < pivot ) {
            // Southern border vertex
            edgeIndices.southIndices.push_back( vertInd ) ;
        }
        if ( p.y() > pivot ) {
            // Northern border vertex
            edgeIndices.northIndices.push_back( vertInd ) ;
        }
    }
    edgeIndices.westVertexCount = edgeIndices.westIndices.size() ;
    edgeIndices.southVertexCount = edgeIndices.southIndices.size() ;
    edgeIndices.eastVertexCount = edgeIndices.eastIndices.size() ;
    edgeIndices.northVertexCount = edgeIndices.northIndices.size() ;

    qmTile->setEdgeIndices(edgeIndices) ;

//    qmTile->printHeader() ;
//    qmTile->print() ;

    // [DEBUG] Export the final tile in OFF format
//    qmTile->exportToOFF("./" + std::to_string(coord.zoom) + "_" + std::to_string(coord.x) + "_" + std::to_string(coord.y) + "_nosimp_qm.off") ;

    delete rasterTile;

    return qmTile;
}


ctb::GDALTile *
QuantizedMeshTiler::createRasterTile(const ctb::TileCoordinate &coord) const {
    // Ensure we have some data from which to create a tile
    if (poDataset && poDataset->GetRasterCount() < 1) {
        throw ctb::CTBException("At least one band must be present in the GDAL dataset");
    }

    // Get the bounds and resolution for a tile coordinate which represents the
    // data overlap requested by the terrain specification.
    double resolution;
    ctb::CRSBounds tileBounds = terrainTileBounds(coord, resolution);

    // Convert the tile bounds into a geo transform
    double adfGeoTransform[6];
    adfGeoTransform[0] = tileBounds.getMinX(); // min longitude
    adfGeoTransform[1] = resolution;
    adfGeoTransform[2] = 0;
    adfGeoTransform[3] = tileBounds.getMaxY(); // max latitude
    adfGeoTransform[4] = 0;
    adfGeoTransform[5] = -resolution;

    ctb::GDALTile *tile = ctb::GDALTiler::createRasterTile(adfGeoTransform);

    // The previous geotransform represented the data with an overlap as required
    // by the terrain specification.  This now needs to be overwritten so that
    // the data is shifted to the bounds defined by tile itself.
    tileBounds = mGrid.tileBounds(coord);
    resolution = mGrid.resolution(coord.zoom);
    adfGeoTransform[0] = tileBounds.getMinX(); // min longitude
    adfGeoTransform[1] = resolution;
    adfGeoTransform[2] = 0;
    adfGeoTransform[3] = tileBounds.getMaxY(); // max latitude
    adfGeoTransform[4] = 0;
    adfGeoTransform[5] = -resolution;

    // Set the shifted geo transform to the VRT
    if (GDALSetGeoTransform(tile->dataset, adfGeoTransform) != CE_None) {
        throw ctb::CTBException("Could not set geo transform on VRT");
    }

    return tile;
}


QuantizedMeshTiler &
QuantizedMeshTiler::operator=(const QuantizedMeshTiler &other) {
    ctb::GDALTiler::operator=(other);

    return *this;
}


/**
 * \brief Create the tile pyramid in quantized-mesh format
 *
 * Create the tile pyramid in quantized-mesh format. Ensures that the vertices between neighboring tiles in the same
 * zoom are the the same
 *
 */
void QuantizedMeshTiler::createTilePyramid(const int &startZoom, const int &endZoom, const std::string &outDir)
{
    for (ctb::i_zoom zoom = endZoom; zoom <= startZoom; ++zoom) {
        ctb::TileCoordinate ll = this->grid().crsToTile( this->bounds().getLowerLeft(), zoom ) ;
        ctb::TileCoordinate ur = this->grid().crsToTile( this->bounds().getUpperRight(), zoom ) ;

        ctb::TileBounds zoomBounds(ll, ur);

        int numStepsX = (int)zoomBounds.getWidth() + 1 ;

        std::vector<Point_3 > prevTileWesternVertices(0) ; // Stores the vertices to maintain from the previous tile's eastern border. Since we are creating the tiles from left-right, down-up, just the previous one is required
        std::vector< std::vector<Point_3 > > prevRowTilesSouthernVertices( numStepsX, std::vector<Point_3>(0) ) ; // Stores the vertices to maintain of the previous row of tiles' northern borders. Since we process a row each time, we need to store all the vertices of all the tiles from the previous row.
        int tileColumnInd = 0 ;



        // Processing the tiles row-wise, from
        for ( int ty = zoomBounds.getMinY(); ty <= zoomBounds.getMaxY(); ty++ ) {
            for ( int tx = zoomBounds.getMinX(); tx <= zoomBounds.getMaxX(); tx++, tileColumnInd++ ) {
                std::cout << "Processing tile: zoom = " << zoom << ", x = " << tx << ", y = " << ty << std::endl ;

                ctb::TileCoordinate coord( zoom, tx, ty ) ;

                QuantizedMeshTile *terrainTile = this->createTile(coord, prevTileWesternVertices, prevRowTilesSouthernVertices[tileColumnInd] ) ;
                //QuantizedMeshTile *terrainTile = this->createTileNoSimp(coord) ;
//                std::cout << "Num western vertices = " << prevTileWesternVertices.size() << std::endl ;
//                for (int i = 0; i < prevTileWesternVertices.size(); i++ )
//                    std::cout << prevTileWesternVertices[i].x() << ", " << prevTileWesternVertices[i].y() << ", " << prevTileWesternVertices[i].z() << std::endl ;
//                std::cout << "Num southern vertices = " << prevRowTilesSouthernVertices[tileColumnInd].size() << std::endl ;
//                for (int i = 0; i < prevRowTilesSouthernVertices[tileColumnInd].size(); i++ )
//                    std::cout << prevRowTilesSouthernVertices[tileColumnInd][i].x() << ", " << prevRowTilesSouthernVertices[tileColumnInd][i].y() << ", " << prevRowTilesSouthernVertices[tileColumnInd][i].z() << std::endl ;

                // write the file
                const std::string fileName = getTileFileAndCreateDirs(coord, outDir);
                terrainTile->writeFile(fileName) ;
            }
            tileColumnInd = 0 ;
            // The first tile on the row does not have to maintain the western edge, clear the list
            prevTileWesternVertices.clear() ;
        }
    }
}