add_executable(qm_parser qm_parser.cpp
               ../base/quantized_mesh_tile.cpp
               ../base/gzip_file_reader.cpp
               ../base/gzip_file_writer.cpp
               ../base/quantized_mesh.cpp)
target_link_libraries(qm_parser ${Boost_LIBRARIES} ${ZLIB_LIBRARIES} ${CTB_LIBRARY} ${CGAL_LIBRARIES} ${GDAL_LIBRARY})

add_executable(test_read_write test_read_write.cpp
                               ../base/quantized_mesh_tile.cpp
                               ../base/gzip_file_reader.cpp
                               ../base/gzip_file_writer.cpp
                               ../base/quantized_mesh.cpp)
target_link_libraries(test_read_write ${Boost_LIBRARIES}
                                      ${ZLIB_LIBRARIES}
                                      ${CTB_LIBRARY}
                                      ${CGAL_LIBRARIES}
                                      ${GDAL_LIBRARY})

add_executable(simplify_maintaining_border_edges simplify_preserving_border_edges.cpp)
target_link_libraries(simplify_maintaining_border_edges ${Boost_LIBRARIES} ${CGAL_LIBRARIES})

add_executable(remesh_preserving_borders remesh_preserving_borders.cpp)
target_link_libraries(remesh_preserving_borders ${Boost_LIBRARIES} ${CGAL_LIBRARIES})

add_executable(hierarchy_simplification hierarchy_simplification.cpp)
target_link_libraries(hierarchy_simplification ${Boost_LIBRARIES} ${CGAL_LIBRARIES})

add_executable(wlop_simplification wlop_simplification.cpp)
target_link_libraries(wlop_simplification ${Boost_LIBRARIES} ${CGAL_LIBRARIES})

add_executable(test_check_borders test_check_borders.cpp
                                  ../base/quantized_mesh_tile.cpp
                                  ../base/gzip_file_reader.cpp
                                  ../base/gzip_file_writer.cpp
                                  ../base/quantized_mesh.cpp)
target_link_libraries(test_check_borders ${Boost_LIBRARIES} ${ZLIB_LIBRARIES} ${CTB_LIBRARY} ${GDAL_LIBRARY})

add_executable(detect_features_without_borders detect_features_without_borders.cpp)
target_link_libraries(detect_features_without_borders ${Boost_LIBRARIES} ${CGAL_LIBRARIES})

add_executable(get_tile_bounds get_tile_bounds.cpp)
target_link_libraries(get_tile_bounds ${Boost_LIBRARIES} ${CTB_LIBRARY} ${GDAL_LIBRARY})

# OpenMP
find_package(OpenMP)
if(OPENMP_FOUND)
    message(STATUS "OpenMP found in the system, enabling OpenMP support for some dev-tests")
    set(USE_OPENMP TRUE)
    add_definitions("-DUSE_OPENMP")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
else()
    message(STATUS "Disabling OpenMP support")
endif()

add_executable(compute_statistics compute_statistics.cpp
                                  ../base/quantized_mesh_tile.cpp
                                  ../base/quantized_mesh_tiler.cpp
                                  ../base/gzip_file_reader.cpp
                                  ../base/gzip_file_writer.cpp
                                  ../base/quantized_mesh.cpp
                                  ../../3rdParty/meshoptimizer/vcacheoptimizer.cpp
                                  ../../3rdParty/meshoptimizer/vfetchoptimizer.cpp
                                  ../base/crs_conversions.cpp ../cgal/avoid_vertical_walls_placement.h)
target_link_libraries(compute_statistics ${Boost_LIBRARIES} ${ZLIB_LIBRARIES} ${CTB_LIBRARY} ${GDAL_LIBRARY} ${GeographicLib_LIBRARIES})