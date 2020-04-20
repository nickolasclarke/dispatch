#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <tuple>

#include <pybind11/pybind11.h>

#include "routingkit.hpp"
#include "csv.hpp"

namespace py = pybind11;

typedef std::vector<std::tuple<size_t, double, double>> closest_depot_t;
const auto dinf = std::numeric_limits<double>::infinity();
const auto dnan = std::numeric_limits<double>::quiet_NaN();


struct Parameters {
  double battery_cap_kwh         = 200.0; //kW*hr
  double kwh_per_km              = 1.2;   //kW*hr/km
  double charging_rate           = 150;   //kW
  double search_radius           = 1000;  //m //TODO
  double zstops_frac_stopped_at  = 0.2;
  double zstops_average_time     = 10;    //seconds
};



struct StopInfo {
  size_t stop_id;
  size_t depot_id;
  double time;
  double distance;
};
typedef std::unordered_map<size_t, StopInfo> stops_t;

stops_t csv_stops_to_internal(const std::string &inpstr){
  stops_t stops;

  std::stringstream ss;
  ss<<inpstr;

  io::CSVReader<4> in("stops.csv", ss);
  in.read_header(io::ignore_extra_column, "stop_id", "depot_id", "time", "distance");

  StopInfo temp;
  while(in.read_row(temp.stop_id, temp.depot_id, temp.time, temp.distance)){
    if(stops.count(temp.stop_id)!=0)
      throw std::runtime_error("stop_id was in the table twice!");
    stops[temp.stop_id] = temp;
  }

  return stops;
}



struct TripInfo {
  size_t trip_id;
  size_t block_id;
  double start_arrival_time;
  double start_stop_id;
  double end_arrival_time;
  double end_stop_id;
  double distance;

  double bus_busy_start;
  double bus_busy_end;
  double bus_id;
  double energy_left;
};
typedef std::vector<TripInfo> trips_t;

trips_t csv_trips_to_internal(const std::string &inpstr){
  trips_t trips;

  std::stringstream ss;
  ss<<inpstr;

  io::CSVReader<7> in("trips.csv", ss);
  in.read_header(io::ignore_extra_column, "trip_id","block_id","start_arrival_time","start_stop_id","end_arrival_time","end_stop_id","distance");

  TripInfo temp;
  while(in.read_row(temp.trip_id,temp.block_id,temp.start_arrival_time,temp.start_stop_id,temp.end_arrival_time,temp.end_stop_id,temp.distance)){
    trips.emplace_back(temp);
  }

  return trips;
}










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
    closest_depot.emplace_back(-1, dnan, dnan);
    for(size_t di=0;di<depot_lat.size();di++){
      try {
        const auto [time, distance] = router.getTravelTime(
          stop_lat[si],
          stop_lon[si],
          depot_lat[di],
          depot_lon[di],
          (int)search_radius_m
        );
        const auto current_best = std::get<1>(closest_depot.back());
        if(std::isnan(current_best) || time<current_best){
          closest_depot.back() = std::make_tuple(di, time, distance);
        }
      } catch (const std::exception &e) {
        //pass
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

  py::class_<Parameters>(m, "Parameters")
    .def(py::init<>())
    .def_readwrite("battery_cap_kwh",        &Parameters::battery_cap_kwh)
    .def_readwrite("kwh_per_km",             &Parameters::kwh_per_km)
    .def_readwrite("charging_rate",          &Parameters::charging_rate)
    .def_readwrite("search_radius",          &Parameters::search_radius)
    .def_readwrite("zstops_frac_stopped_at", &Parameters::zstops_frac_stopped_at)
    .def_readwrite("zstops_average_time",    &Parameters::zstops_average_time);

  m.def("GetClosestDepot", &GetClosestDepot, "TODO");
}
