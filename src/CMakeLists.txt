cmake_minimum_required(VERSION 3.10)

if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -pedantic)
endif()

add_subdirectory(cpp_utilities)
add_subdirectory(dispatch_package)

add_library(rkrouter rkrouter/routingkit.cpp)
target_include_directories(rkrouter PUBLIC rkrouter)
target_link_libraries(rkrouter PUBLIC routingkit)
set_property(TARGET rkrouter PROPERTY POSITION_INDEPENDENT_CODE ON)



set(SCRIPT_FILES
  find_chargers.py optimize_bus_distribution.py parse_gtfs.py pull_gtfs.py sim.py scenarios.py
)

foreach(pyfile ${SCRIPT_FILES})
  message(STATUS "output ${CMAKE_BINARY_DIR}/bin/${pyfile}")
  ADD_CUSTOM_COMMAND(OUTPUT "${CMAKE_BINARY_DIR}/bin/${pyfile}"
    COMMAND cmake -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/${pyfile}"
    "${CMAKE_BINARY_DIR}/bin/${pyfile}"
    DEPENDS   "${CMAKE_CURRENT_SOURCE_DIR}/${pyfile}"
    )
   list(APPEND pyfile_dest "${CMAKE_BINARY_DIR}/bin/${pyfile}")
endforeach(pyfile)
ADD_CUSTOM_TARGET(python ALL DEPENDS ${pyfile_dest})
