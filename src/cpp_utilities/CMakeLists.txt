cmake_minimum_required(VERSION 3.10)

find_package(Osmium REQUIRED COMPONENTS pbf)
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
find_package(BZip2)
find_package(EXPAT)

add_executable(osm_splitter osm_splitter.cpp)
install(TARGETS osm_splitter RUNTIME DESTINATION DESTINATION bin)
target_include_directories(osm_splitter PRIVATE ${OSMIUM_INCLUDE_DIRS})
target_link_libraries(osm_splitter PRIVATE Threads::Threads ZLIB::ZLIB BZip2::BZip2 EXPAT::EXPAT)

add_executable(routing_preprocess routing_preprocess.cpp)
target_link_libraries(routing_preprocess PRIVATE routingkit rkrouter)
install(TARGETS routing_preprocess RUNTIME DESTINATION DESTINATION bin)
