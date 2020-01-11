cmake_minimum_required(VERSION 3.13)

find_package(pybind11)

pybind11_add_module(pyroutingkit routingkit_py.cpp)
target_link_libraries(pyroutingkit PRIVATE routingkit)

install(TARGETS pyroutingkit DESTINATION bin)