#include <limits>
#include <tuple>

#include <pybind11/pybind11.h>

#include "routingkit.hpp"

namespace py = pybind11;

typedef std::vector<std::tuple<size_t, double, double>> closest_depot_t;
const auto dinf = std::numeric_limits<double>::infinity();

//Returns a vector of DepotID, Travel time (s), Travel distane (m)
closest_depot_t GetClosestDepot(
  const Router &router,
  const std::vector<double> &stop_lat,
  const std::vector<double> &stop_lon,
  const std::vector<double> &depot_lat,
  const std::vector<double> &depot_lon,
  const double search_radius_m
){
  closest_depot_t closest_depot;
  for(size_t si=0;si<stop_lat.size();si++){
    closest_depot.emplace_back(-1, dinf, dinf);
    for(size_t di=0;di<depot_lat.size();di++){
      const auto [time, distance] = router.getTravelTime(
        stop_lat[si],
        stop_lon[si],
        depot_lat[di],
        depot_lon[di],
        (int)search_radius_m
      );
      if(time<std::get<1>(closest_depot.back())){
        closest_depot.back() = std::make_tuple(di, time, distance);
      }
    }
  }
  return closest_depot;
}

PYBIND11_MODULE(dispatch, m) {
  m.doc() = "Dispatch Python module"; // optional module docstring

  py::class_<Router>(m, "Router")
    .def(py::init<const std::string &>())
    .def(py::init<const std::string &, const std::string &>())
    .def("getTravelTime",  &Router::getTravelTime)
    .def("getNearestNode", &Router::getNearestNode)
    .def("save_ch",        &Router::save_ch);

  m.def("GetClosestDepot", &GetClosestDepot, "TODO");
}
