cmake_minimum_required(VERSION 3.10)

find_package(pybind11 REQUIRED)
find_package(OpenMP)

pybind11_add_module(dispatch dispatch.cpp)
target_link_libraries(dispatch PRIVATE rkrouter OpenMP::OpenMP_CXX)
target_compile_features(dispatch PUBLIC cxx_std_17)
install(TARGETS dispatch LIBRARY DESTINATION bin)
