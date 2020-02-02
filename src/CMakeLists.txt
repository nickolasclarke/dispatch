cmake_minimum_required(VERSION 3.13)

add_subdirectory(routingkit)

install(TARGETS pyroutingkit DESTINATION bin)

set(PYFILES
  find_chargers.py optimize_bus_distribution.py osrm_server.py parse_gtfs.py sim.py
)
 
foreach(pyfile ${PYFILES})
  message(STATUS "output ${CMAKE_BINARY_DIR}/bin/${pyfile}")
  ADD_CUSTOM_COMMAND(OUTPUT "${CMAKE_BINARY_DIR}/bin/${pyfile}"
    COMMAND cmake -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/${pyfile}"
    "${CMAKE_BINARY_DIR}/bin/${pyfile}"
    DEPENDS   "${CMAKE_CURRENT_SOURCE_DIR}/${pyfile}"
    )
   list(APPEND pyfile_dest "${CMAKE_BINARY_DIR}/bin/${pyfile}")
endforeach(pyfile)
ADD_CUSTOM_TARGET(python ALL DEPENDS ${pyfile_dest})
