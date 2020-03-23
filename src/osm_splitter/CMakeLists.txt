cmake_minimum_required(VERSION 3.10)

find_package(Osmium REQUIRED)
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
find_package(BZip2)
find_package(EXPAT)

if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

add_executable(osm_splitter osm_splitter.cpp)
install(TARGETS osm_splitter RUNTIME DESTINATION DESTINATION bin)
target_include_directories(osm_splitter PRIVATE ${OSMIUM_INCLUDE_DIRS})
target_link_libraries(osm_splitter PRIVATE Threads::Threads ZLIB::ZLIB BZip2::BZip2 EXPAT::EXPAT)