cmake_minimum_required(VERSION 3.13)

find_package(pybind11)

#Use the default Julia to find the CxxWrap path
if(NOT DEFINED JL_CXX_WRAP_PATH)
  execute_process(COMMAND julia "-e" "using CxxWrap; print(CxxWrap.CxxWrapCore.prefix_path())"
    RESULT_VARIABLE julia_cxxwrap_result
    OUTPUT_VARIABLE JL_CXX_WRAP_PATH)

  if(NOT "${julia_cxxwrap_result}" STREQUAL "0")
      message(FATAL_ERROR "Could not find Julia's CxxWrap Location. Please set JL_CXX_WRAP_PATH variable.")
  endif()
endif()
message(STATUS "Julia CxxWrap path: " "${JL_CXX_WRAP_PATH}")
list(APPEND CMAKE_PREFIX_PATH "${JL_CXX_WRAP_PATH}")
find_package(JlCxx)



pybind11_add_module(pyroutingkit rk_python.cpp routingkit.cpp)
target_link_libraries(pyroutingkit PRIVATE routingkit)
install(TARGETS pyroutingkit DESTINATION bin)

add_library(jlroutingkit rk_julia.cpp routingkit.cpp)
target_link_libraries(jlroutingkit PRIVATE routingkit JlCxx::cxxwrap_julia)
install(TARGETS jlroutingkit DESTINATION bin)
