#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "pybind11_conversions.hpp"

#include "dispatch.hpp"

namespace py = pybind11;

PYBIND11_MODULE(dispatch, m) {
  m.doc() = "Dispatch Python module"; // optional module docstring

  py::class_<Router>(m, "Router")
    .def(py::init<const std::string &>())
    .def(py::init<const std::string &, const std::string &>())
    .def("getTravelTime",  &Router::getTravelTime)
    .def("getNearestNode", &Router::getNearestNode)
    .def("save_ch",        &Router::save_ch);

  py::class_<TripInfo>(m, "TripInfo")
    .def(py::init<>())
    .def("__repr__",                     &TripInfo::repr)
    .def_readwrite("trip_id",            &TripInfo::trip_id)
    .def_readwrite("block_id",           &TripInfo::block_id)
    .def_readwrite("start_arrival_time", &TripInfo::start_arrival_time)
    .def_readwrite("start_stop_id",      &TripInfo::start_stop_id)
    .def_readwrite("end_arrival_time",   &TripInfo::end_arrival_time)
    .def_readwrite("end_stop_id",        &TripInfo::end_stop_id)
    .def_readwrite("distance",           &TripInfo::distance)
    .def_readwrite("bus_busy_start",     &TripInfo::bus_busy_start)
    .def_readwrite("bus_busy_end",       &TripInfo::bus_busy_end)
    .def_readwrite("bus_id",             &TripInfo::bus_id)
    .def_readwrite("start_depot_id",     &TripInfo::start_depot_id)
    .def_readwrite("end_depot_id",       &TripInfo::end_depot_id)
    .def_readwrite("energy_left",        &TripInfo::energy_left);

  py::class_<Parameters>(m, "Parameters")
    .def(py::init<>())
    .def("__repr__", &Parameters::repr)
    .def_readwrite("battery_cap_kwh",       &Parameters::battery_cap_kwh)
    .def_readwrite("kwh_per_km",            &Parameters::kwh_per_km)
    .def_readwrite("bus_cost",              &Parameters::bus_cost)
    .def_readwrite("battery_cost_per_kwh",  &Parameters::battery_cost_per_kwh)
    .def_readwrite("depot_charger_cost",    &Parameters::depot_charger_cost)
    .def_readwrite("depot_charger_rate",    &Parameters::depot_charger_rate)
    .def_readwrite("nondepot_charger_cost", &Parameters::nondepot_charger_cost)
    .def_readwrite("nondepot_charger_rate", &Parameters::nondepot_charger_rate)
    .def_readwrite("chargers_per_depot",    &Parameters::chargers_per_depot);

  py::class_<ClosestDepotInfo>(m, "ClosestDepotInfo")
    .def(py::init<const int>())
    .def_readwrite("depot_id",      &ClosestDepotInfo::depot_id)
    .def_readwrite("time_to_depot", &ClosestDepotInfo::time_to_depot)
    .def_readwrite("dist_to_depot", &ClosestDepotInfo::dist_to_depot)
    .def("__repr__",                &ClosestDepotInfo::repr);

  py::class_<StopInfo>(m, "StopInfo")
    .def(py::init<>())
    .def("__repr__", &StopInfo::repr)
    .def_readwrite("stop_id",        &StopInfo::stop_id)
    .def_readwrite("depot_id",       &StopInfo::depot_id)
    .def_readwrite("depot_time",     &StopInfo::depot_time)
    .def_readwrite("depot_distance", &StopInfo::depot_distance);

  py::class_<ModelInfo>(m, "ModelInfo")
    .def(py::init<const Parameters&, const std::string&, const std::string&>())
    .def("update_params", &ModelInfo:: update_params)
    .def_readonly("params", &ModelInfo::params)
    .def_readonly("trips",  &ModelInfo::trips)
    .def_readonly("stops",  &ModelInfo::stops);

  m.def("GetClosestDepot", &GetClosestDepot, "TODO");
  m.def("count_buses", &count_buses, "TODO");
  m.def("run_model", &run_model, "TODO");
}