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
