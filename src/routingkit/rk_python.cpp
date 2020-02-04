#include <pybind11/pybind11.h>
#include "routingkit.hpp"

namespace py = pybind11;

PYBIND11_MODULE(pyroutingkit, m) {
  m.doc() = "RoutingKit interface"; // optional module docstring

  py::class_<Router>(m, "Router")
    .def(py::init<const std::string &>())
    .def("getTravelTime", &Router::getTravelTime);
}
