cmake_minimum_required(VERSION 3.10)

find_package(pybind11 REQUIRED)
find_package(OpenMP)

pybind11_add_module(dispatch dispatch.cpp data_frames.cpp units.cpp pybind_module.cpp optimizer.cpp)
target_link_libraries(dispatch PRIVATE rkrouter OpenMP::OpenMP_CXX type_safe)
target_compile_features(dispatch PUBLIC cxx_std_17)
target_compile_options(dispatch PRIVATE -ffast-math -march=native)
install(TARGETS dispatch LIBRARY DESTINATION bin)