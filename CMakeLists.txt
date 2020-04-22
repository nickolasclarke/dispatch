cmake_minimum_required(VERSION 3.10)

set(PROTOZERO_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/submodules/protozero/include")
set(OSMIUM_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/submodules/libosmium/include")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/submodules/libosmium/cmake/")

if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -pedantic)
endif()

add_subdirectory(submodules)
add_subdirectory(src)
