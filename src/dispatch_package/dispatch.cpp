#include <algorithm>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <tuple>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "routingkit.hpp"
#include "csv.hpp"

namespace py = pybind11;

const auto dinf = std::numeric_limits<double>::infinity();
const auto dnan = std::numeric_limits<double>::quiet_NaN();

struct ClosestDepotInfo {
  std::vector<int>    depot_id;
  std::vector<double> time_to_depot;
  std::vector<double> dist_to_depot;
  ClosestDepotInfo(const int N){
    depot_id.resize(N, -1);
    time_to_depot.resize(N, dnan);
    dist_to_depot.resize(N, dnan);
  }
};

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
  int    depot_id;
  double depot_time;
  double depot_distance;
};
typedef std::unordered_map<size_t, StopInfo> stops_t;

stops_t csv_stops_to_internal(const std::string &inpstr){
  stops_t stops;

  std::stringstream ss;
  ss<<inpstr;

  io::CSVReader<4> in("stops.csv", ss);
  in.read_header(io::ignore_extra_column, "stop_id", "depot_id", "depot_time", "depot_distance");

  StopInfo temp;
  while(in.read_row(temp.stop_id, temp.depot_id, temp.depot_time, temp.depot_distance)){
    if(stops.count(temp.stop_id)!=0)
      throw std::runtime_error("stop_id was in the table twice!");
    stops[temp.stop_id] = temp;
  }

  return stops;
}



struct TripInfo {
  std::string trip_id;
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



class Model {
 private:
  Parameters params;
  trips_t trips;
  stops_t stops;
 public:
  Model(
    const Parameters  &params0,
    const std::string &trips_csv,
    const std::string &stops_csv
  ){
    params = params0;
    trips  = csv_trips_to_internal(trips_csv);
    stops  = csv_stops_to_internal(stops_csv);

    //Sort trips into blocks where each block is ordered by start arrival time
    std::stable_sort(trips.begin(), trips.end(), [](const auto &a, const auto &b){ return a.start_arrival_time<b.start_arrival_time; });
    std::stable_sort(trips.begin(), trips.end(), [](const auto &a, const auto &b){ return a.block_id<b.block_id; });
  }
/*
  void runblock(){
    function RunBlock(
      """
      Simulate bus movement along a route. Information about the journey is stored by
      modifying "block".

      Args:
          block - Block of trips to be simulated (MODIFIED WITH OUTPUT)
          dp    - A data pack
      """

      prevtrip = block[1]

      //Get time, distance, and energy from depot to route
      bstart_depot = FindClosestDepotByTime(dp.stop_ll[prevtrip.start_stop_id], dp)
      block_energy_depot_to_start = bstart_depot[:dist] * dp.params[:kwh_per_km]

      //Modify first trip of block appropriately
      block[1] = merge(block[1], (;
          bus_id=1,
          energy_left = dp.params[:battery_cap_kwh] - block_energy_depot_to_start,
          depot = bstart_depot[:id]
      ))

      if length(block)==1
          println("Block has length 1!")
      end

      previ=1
      //Run through all the trips in the block
      for tripi in 1:length(block)
          trip = block[tripi]
          prevtrip = block[previ]

          //TODO: Incorporate charging at the beginning of trip if there is a charger present
          trip_energy = trip.distance * dp.params[:kwh_per_km]

          //Subtract energy gained through inductive charging from the energetic
          //cost of the trip
          trip_energy -= get(dp.inductive_charge_time, trip.trip_id, 0u"kW*hr")

          //TODO: Incorporate energetics of stopping sporadically along the trip
          //trip_energy -= GetStopChargingTime(stops, trip.trip_id) //TODO: Convert to energy

          //Get energetics of completing this trip and then going to a depot
          end_depot = FindClosestDepotByTime(dp.stop_ll[trip.end_stop_id], dp)
          energy_from_end_to_depot = end_depot[:dist] * dp.params[:kwh_per_km]

          //Energy left to perform this trip
          energy_left_this_trip = prevtrip.energy_left

          //Do we have enough energy to complete this trip and then go to the depot?
          if energy_left_this_trip - trip_energy - energy_from_end_to_depot < 0u"kW*hr"
              //We can't complete this trip and get back to the depot, so it was better to end the block after the previous trip
              //TODO: Assert previous trip ending stop_id is same as this trip's starting stop_id
              //Get closest depot and energetics to the start of this trip (which is also the end of the previous trip)
              start_depot = FindClosestDepotByTime(dp.stop_ll[trip.start_stop_id], dp)
              energy_from_start_to_depot = start_depot[:dist] * dp.params[:kwh_per_km]

              //TODO: Something bad has happened if we've reached this: we shouldn't have even made the last trip.
              if energy_from_start_to_depot < prevtrip.energy_left
                  println("\nEnergy trap found!")
              end

              //Alter the previous trip to note that we ended it
              energy_left_last_trip = prevtrip.energy_left-energy_from_start_to_depot
              charge_time = (dp.params[:battery_cap_kwh]-energy_left_last_trip)/dp.params[:charging_rate]
              block[previ] = merge(block[previ], (;
                  energy_left = energy_left_last_trip,
                  bus_busy_end = prevtrip.bus_busy_end+start_depot[:time] + charge_time,
                  depot = start_depot[:id]
              ))

              //TODO: Assumes that trip from trip start to depot and from depot to trip start is the same length
              energy_left_this_trip = dp.params[:battery_cap_kwh]-energy_from_start_to_depot

              //Alter this trip so that we start it with a fresh bus
              block[tripi] = merge(block[tripi], (;
                  bus_busy_start = trip.start_arrival_time - start_depot[:time],
                  bus_id = prevtrip.bus_id+1
              ))
          else
              //We have enough energy to make the trip, so let's start it!
              block[tripi] = merge(block[tripi], (;
                  bus_busy_start = trip.start_arrival_time,
                  bus_id = prevtrip.bus_id
              ))
          end

          //We have enough energy to finish the trip
          block[tripi] = merge(block[tripi], (;
              bus_busy_end = trip.end_arrival_time,
              energy_left = energy_left_this_trip-trip_energy
          ))

          //This trip is now the previous trip
          previ = tripi
      end

      //Get energetics of getting from the final trip to its depot
      bend_depot = FindClosestDepotByTime(dp.stop_ll[prevtrip.start_stop_id], dp)
      block_energy_end_to_depot = bend_depot[:dist] * dp.params[:kwh_per_km]

      //Adjust beginning and end
      block[1]   = merge(block[1], (;bus_busy_start = block[1].bus_busy_start - bstart_depot[:time]))
      block[end] = merge(block[end], (; energy_left = block[end].energy_left - block_energy_end_to_depot))
  }

  void run(){
    // #Add/zero out the bus_id column
    // trips = transform(trips, :bus_id         => -1          *ones(Int64,   length(trips)))
    // trips = transform(trips, :energy_left    => -1.0u"kW*hr"*ones(Float64, length(trips)))
    // trips = transform(trips, :bus_busy_start => -1.0u"s"    *ones(Float64, length(trips)))
    // trips = transform(trips, :bus_busy_end   => -1.0u"s"    *ones(Float64, length(trips)))
    // trips = transform(trips, :depot          => -1          *ones(Int64,   length(trips)))
  // #Add the inductive charging time to the data pack
  // data_pack = (data_pack..., inductive_charge_time = GetInductiveChargeTimes(data_pack))

  }*/
};






//Returns a vector of DepotID, Travel time (s), Travel distane (m)
ClosestDepotInfo GetClosestDepot(
  const Router &router,
  const std::vector<double> &stop_lat,
  const std::vector<double> &stop_lon,
  const std::vector<double> &depot_lat,
  const std::vector<double> &depot_lon,
  const double search_radius_m
){
  ClosestDepotInfo closest_depot(stop_lat.size());

  #pragma omp parallel for
  for(size_t si=0;si<stop_lat.size();si++)
  for(size_t di=0;di<depot_lat.size();di++){
    try {
      const auto [time, distance] = router.getTravelTime(
        stop_lat[si],
        stop_lon[si],
        depot_lat[di],
        depot_lon[di],
        (int)search_radius_m
      );
      const auto current_best = closest_depot.time_to_depot[si];
      if(std::isnan(current_best) || time<current_best){
        closest_depot.depot_id[si] = di;
        closest_depot.time_to_depot[si] = time;
        closest_depot.dist_to_depot[si] = distance;
      }
    } catch (const std::exception &e) {
      //pass
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

  py::class_<ClosestDepotInfo>(m, "ClosestDepotInfo")
    .def(py::init<const int>())
    .def_readwrite("depot_id",      &ClosestDepotInfo::depot_id)
    .def_readwrite("time_to_depot", &ClosestDepotInfo::time_to_depot)
    .def_readwrite("dist_to_depot", &ClosestDepotInfo::dist_to_depot);

  py::class_<Model>(m, "Model")
    .def(py::init<const Parameters&, const std::string&, const std::string&>());

  m.def("GetClosestDepot", &GetClosestDepot, "TODO");
}
