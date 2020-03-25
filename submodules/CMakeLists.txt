cmake_minimum_required(VERSION 3.10)

find_package(Threads)
find_package(ZLIB)

add_library(routingkit
  RoutingKit/src/bit_select.cpp 
  RoutingKit/src/bit_vector.cpp 
  RoutingKit/src/buffered_asynchronous_reader.cpp 
  RoutingKit/src/contraction_hierarchy.cpp 
  RoutingKit/src/customizable_contraction_hierarchy.cpp 
  RoutingKit/src/expect.cpp
  RoutingKit/src/file_data_source.cpp 
  RoutingKit/src/geo_position_to_node.cpp 
  RoutingKit/src/graph_util.cpp 
  RoutingKit/src/id_mapper.cpp 
  RoutingKit/src/nested_dissection.cpp 
  RoutingKit/src/osm_decoder.cpp 
  RoutingKit/src/osm_graph_builder.cpp 
  RoutingKit/src/osm_profile.cpp 
  RoutingKit/src/osm_simple.cpp 
  RoutingKit/src/protobuf.cpp 
  RoutingKit/src/timer.cpp 
  RoutingKit/src/vector_io.cpp
  RoutingKit/src/verify.cpp
)

target_include_directories(routingkit PUBLIC RoutingKit/include/)

target_link_libraries(routingkit
PUBLIC
  Threads::Threads
  ZLIB::ZLIB
)

set_property(TARGET routingkit PROPERTY POSITION_INDEPENDENT_CODE ON)



# add_executable(compare_vector RoutingKit/src/compare_vector.cpp)
# add_executable(compute_contraction_hierarchy RoutingKit/src/compute_contraction_hierarchy.cpp)
# add_executable(compute_geographic_distance_weights RoutingKit/src/compute_geographic_distance_weights.cpp)
# add_executable(compute_nested_dissection_order RoutingKit/src/compute_nested_dissection_order.cpp)
# add_executable(convert_road_dimacs_coordinates RoutingKit/src/convert_road_dimacs_coordinates.cpp)
# add_executable(convert_road_dimacs_graph RoutingKit/src/convert_road_dimacs_graph.cpp)
# add_executable(decode_vector RoutingKit/src/decode_vector.cpp)
# add_executable(encode_vector RoutingKit/src/encode_vector.cpp)
# add_executable(examine_ch RoutingKit/src/examine_ch.cpp)
# add_executable(export_road_dimacs_graph RoutingKit/src/export_road_dimacs_graph.cpp)
# add_executable(generate_constant_vector RoutingKit/src/generate_constant_vector.cpp)
# add_executable(generate_dijkstra_rank_test_queries RoutingKit/src/generate_dijkstra_rank_test_queries.cpp)
# add_executable(generate_random_node_list RoutingKit/src/generate_random_node_list.cpp)
# add_executable(generate_random_source_times RoutingKit/src/generate_random_source_times.cpp)
# add_executable(generate_test_queries RoutingKit/src/generate_test_queries.cpp)
# add_executable(graph_to_dot RoutingKit/src/graph_to_dot.cpp)
# add_executable(graph_to_svg RoutingKit/src/graph_to_svg.cpp)
# add_executable(osm_extract RoutingKit/src/osm_extract.cpp)
# add_executable(randomly_permute_nodes RoutingKit/src/randomly_permute_nodes.cpp)
# add_executable(run_contraction_hierarchy_query RoutingKit/src/run_contraction_hierarchy_query.cpp)
# add_executable(run_dijkstra RoutingKit/src/run_dijkstra.cpp)
# add_executable(show_path RoutingKit/src/show_path.cpp)
# add_executable(test_basic_features RoutingKit/src/test_basic_features.cpp)
# add_executable(test_bit_vector RoutingKit/src/test_bit_vector.cpp)
# add_executable(test_buffered_asynchronous_reader RoutingKit/src/test_buffered_asynchronous_reader.cpp)
# add_executable(test_contraction_hierarchy_extra_weight RoutingKit/src/test_contraction_hierarchy_extra_weight.cpp)
# add_executable(test_contraction_hierarchy_path_query RoutingKit/src/test_contraction_hierarchy_path_query.cpp)
# add_executable(test_contraction_hierarchy_pinned_query RoutingKit/src/test_contraction_hierarchy_pinned_query.cpp)
# add_executable(test_customizable_contraction_hierarchy RoutingKit/src/test_customizable_contraction_hierarchy.cpp)
# add_executable(test_customizable_contraction_hierarchy_customization RoutingKit/src/test_customizable_contraction_hierarchy_customization.cpp)
# add_executable(test_customizable_contraction_hierarchy_path_query RoutingKit/src/test_customizable_contraction_hierarchy_path_query.cpp)
# add_executable(test_customizable_contraction_hierarchy_perfect_customization RoutingKit/src/test_customizable_contraction_hierarchy_perfect_customization.cpp)
# add_executable(test_customizable_contraction_hierarchy_pinned_query RoutingKit/src/test_customizable_contraction_hierarchy_pinned_query.cpp)
# add_executable(test_customizable_contraction_hierarchy_reset RoutingKit/src/test_customizable_contraction_hierarchy_reset.cpp)
# add_executable(test_dijkstra RoutingKit/src/test_dijkstra.cpp)
# add_executable(test_geo_dist RoutingKit/src/test_geo_dist.cpp)
# add_executable(test_id_mapper RoutingKit/src/test_id_mapper.cpp)
# add_executable(test_id_set_queue RoutingKit/src/test_id_set_queue.cpp)
# add_executable(test_inverse_vector RoutingKit/src/test_inverse_vector.cpp)
# add_executable(test_nearest_neighbor RoutingKit/src/test_nearest_neighbor.cpp)
# add_executable(test_nested_dissection RoutingKit/src/test_nested_dissection.cpp)
# add_executable(test_osm_simple RoutingKit/src/test_osm_simple.cpp)
# add_executable(test_permutation RoutingKit/src/test_permutation.cpp)
# add_executable(test_protobuf RoutingKit/src/test_protobuf.cpp)
# add_executable(test_sort RoutingKit/src/test_sort.cpp)
# add_executable(test_tag_map RoutingKit/src/test_tag_map.cpp)



# target_link_libraries(compare_vector routingkit)
# target_link_libraries(compute_contraction_hierarchy routingkit)
# target_link_libraries(compute_geographic_distance_weights routingkit)
# target_link_libraries(compute_nested_dissection_order routingkit)
# target_link_libraries(convert_road_dimacs_coordinates routingkit)
# target_link_libraries(convert_road_dimacs_graph routingkit)
# target_link_libraries(decode_vector routingkit)
# target_link_libraries(encode_vector routingkit)
# target_link_libraries(examine_ch routingkit)
# target_link_libraries(export_road_dimacs_graph routingkit)
# target_link_libraries(generate_constant_vector routingkit)
# target_link_libraries(generate_dijkstra_rank_test_queries routingkit)
# target_link_libraries(generate_random_node_list routingkit)
# target_link_libraries(generate_random_source_times routingkit)
# target_link_libraries(generate_test_queries routingkit)
# target_link_libraries(graph_to_dot routingkit)
# target_link_libraries(graph_to_svg routingkit)
# target_link_libraries(osm_extract routingkit)
# target_link_libraries(randomly_permute_nodes routingkit)
# target_link_libraries(run_contraction_hierarchy_query routingkit)
# target_link_libraries(run_dijkstra routingkit)
# target_link_libraries(show_path routingkit)
# target_link_libraries(test_basic_features routingkit)
# target_link_libraries(test_bit_vector routingkit)
# target_link_libraries(test_buffered_asynchronous_reader routingkit)
# target_link_libraries(test_contraction_hierarchy_extra_weight routingkit)
# target_link_libraries(test_contraction_hierarchy_path_query routingkit)
# target_link_libraries(test_contraction_hierarchy_pinned_query routingkit)
# target_link_libraries(test_customizable_contraction_hierarchy routingkit)
# target_link_libraries(test_customizable_contraction_hierarchy_customization routingkit)
# target_link_libraries(test_customizable_contraction_hierarchy_path_query routingkit)
# target_link_libraries(test_customizable_contraction_hierarchy_perfect_customization routingkit)
# target_link_libraries(test_customizable_contraction_hierarchy_pinned_query routingkit)
# target_link_libraries(test_customizable_contraction_hierarchy_reset routingkit)
# target_link_libraries(test_dijkstra routingkit)
# target_link_libraries(test_geo_dist routingkit)
# target_link_libraries(test_id_mapper routingkit)
# target_link_libraries(test_id_set_queue routingkit)
# target_link_libraries(test_inverse_vector routingkit)
# target_link_libraries(test_nearest_neighbor routingkit)
# target_link_libraries(test_nested_dissection routingkit)
# target_link_libraries(test_osm_simple routingkit)
# target_link_libraries(test_permutation routingkit)
# target_link_libraries(test_protobuf routingkit)
# target_link_libraries(test_sort routingkit)
# target_link_libraries(test_tag_map routingkit)