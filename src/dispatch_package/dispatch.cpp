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


//TODO: Assumes that trip from trip start to depot and from depot to
//trip start is the same length
struct ClosestDepotInfo {
  std::vector<int>    depot_id;
  std::vector<double> time_to_depot;
  std::vector<double> dist_to_depot;
  ClosestDepotInfo(const int N){
    depot_id.resize(N, -1);
    time_to_depot.resize(N, dnan);
    dist_to_depot.resize(N, dnan);
  }
  std::string repr() const {
    return std::string("<dispatch.ClosestDepotInfo of length "+std::to_string(depot_id.size())+" with properties depot_id, time_to_depot, dist_to_depot>");
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

  double  bus_busy_start = -1;
  double  bus_busy_end   = -1;
  int32_t bus_id         = -1;
  int32_t start_depot_id = -1;
  int32_t end_depot_id   = -1;
  double  energy_left    = -1; //kW*hr

  std::string repr(){
    return std::string("<dispatch.TripInfo ")
      + "trip_id="            +trip_id                           +", "
      + "block_id="           +std::to_string(block_id)          +", "
      + "start_arrival_time=" +std::to_string(start_arrival_time)+", "
      + "start_stop_id="      +std::to_string(start_stop_id)     +", "
      + "end_arrival_time="   +std::to_string(end_arrival_time)  +", "
      + "end_stop_id="        +std::to_string(end_stop_id)       +", "
      + "distance="           +std::to_string(distance)          +", "
      + "bus_busy_start="     +std::to_string(bus_busy_start)    +", "
      + "bus_busy_end="       +std::to_string(bus_busy_end)      +", "
      + "bus_id="             +std::to_string(bus_id)            +", "
      + "start_depot_id="     +std::to_string(start_depot_id)    +", "
      + "end_depot_id="       +std::to_string(end_depot_id)      +", "
      + "energy_left="        +std::to_string(energy_left)
      + ">";
  }
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
  const trips_t trips_template;
  const stops_t stops;

  static trips_t load_and_sort_trips(const std::string &trips_csv){
    trips_t trips = csv_trips_to_internal(trips_csv);

    //Sort trips into blocks where each block is ordered by start arrival time
    std::stable_sort(trips.begin(), trips.end(), [](const auto &a, const auto &b){ return a.start_arrival_time<b.start_arrival_time; });
    std::stable_sort(trips.begin(), trips.end(), [](const auto &a, const auto &b){ return a.block_id<b.block_id; });

    return trips;
  }

 public:
  Model(
    const Parameters &params,
    const std::string &trips_csv,
    const std::string &stops_csv
  ) : params(params),
      trips_template(load_and_sort_trips(trips_csv)),
      stops(csv_stops_to_internal(stops_csv))
  {}

  void update_params(const Parameters &new_params){
    params = new_params;
  }

  ///Simulate bus movement along a route. Information about the journey is
  ///stored by modifying the block.
  ///
  ///@param block_start Iterator pointing to the start of the block
  ///@param block_end   Iterator pointing one past the end of the block
  void run_block(trips_t::iterator &block_start, trips_t::iterator &block_end) const {
    //Get time, distance, and energy from depot to route
    const auto start_depot = stops.at(block_start->start_stop_id);
    const auto energy_depot_to_start = start_depot.depot_distance*params.kwh_per_km;

    block_start->start_depot_id = start_depot.depot_id;
    block_start->energy_left    = params.battery_cap_kwh - energy_depot_to_start;
    block_start->bus_id         = 1;

    //Run through all the trips in the block
    auto prevtrip=block_start;
    for(auto trip=block_start;trip!=block_end;trip++){
      //TODO: Incorporate charging at the beginning of trip if there is a charger present
      auto trip_energy = trip->distance * params.kwh_per_km;

      //Subtract energy gained through inductive charging from the energetic
      //cost of the trip TODO
      // trip_energy -= get(dp.inductive_charge_time, trip->trip_id, 0u"kW*hr")

      //TODO: Incorporate energetics of stopping sporadically along the trip
      //trip_energy -= GetStopChargingTime(stops, trip->trip_id) //TODO: Convert to energy

      //Get energetics of completing this trip and then going to a depot
      const auto end_depot = stops.at(trip->end_stop_id);
      const auto energy_from_end_to_depot = end_depot.depot_distance * params.kwh_per_km;

      //Energy left to perform this trip
      auto energy_left_this_trip = prevtrip->energy_left;

      //Do we have enough energy to complete this trip and then go to the depot?
      if(energy_left_this_trip - trip_energy - energy_from_end_to_depot < 0){
        //We can't complete this trip and get back to the depot, so it was
        //better to end the block after the previous trip

        //TODO: Assert previous trip ending stop_id is same as this trip's
        //starting stop_id

        //Get closest depot and energetics to the start of this trip (which is
        //also the end of the previous trip)
        const auto start_depot = stops.at(trip->start_stop_id);
        const auto energy_from_start_to_depot = start_depot.depot_distance*params.kwh_per_km;

        //TODO: Something bad has happened if we've reached this: we shouldn't have even made the last trip->
        if(energy_from_start_to_depot < prevtrip->energy_left)
          std::cerr<<"\nEnergy trap found!"<<std::endl;

        //Alter the previous trip to note that we ended it
        prevtrip->energy_left = prevtrip->energy_left-energy_from_start_to_depot;
        const auto charge_time = (params.battery_cap_kwh-prevtrip->energy_left)/params.charging_rate;
        prevtrip->bus_busy_end += start_depot.depot_time + charge_time;
        prevtrip->end_depot_id = start_depot.depot_id;

        //Now that we've closed out the old bus/trip let's deal with the new one
        energy_left_this_trip = params.battery_cap_kwh-energy_from_start_to_depot;
        trip->bus_busy_start = trip->start_arrival_time - start_depot.depot_time;
        trip->bus_id = prevtrip->bus_id+1;
      } else {
        trip->bus_busy_start = trip->start_arrival_time;
        trip->bus_id = prevtrip->bus_id;
      }

      //We have enough energy to finish the trip
      trip->bus_busy_end = trip->end_arrival_time;
      trip->energy_left = energy_left_this_trip-trip_energy;
      prevtrip = trip;
    }

    //Get energetics of getting from the final trip to its depot
    const auto block_end_depot           = stops.at(prevtrip->end_stop_id);
    const auto block_energy_end_to_depot = block_end_depot.depot_distance * params.kwh_per_km;

    //Adjust beginning and end
    block_start->bus_busy_start -= start_depot.depot_time;
    prevtrip->energy_left       -= block_energy_end_to_depot;
    prevtrip->bus_busy_start    += block_end_depot.depot_time;
  }

  trips_t run() const {
    auto trips = trips_template;
    auto start_of_block = trips.begin();
    for(auto trip=trips.begin();trip!=trips.end();trip++){
      //Have we found a new block?
      if(trip->trip_id!=start_of_block->trip_id || trip->block_id!=start_of_block->block_id){
        //If so, the trip is a valid end iterator for the previous block
        run_block(start_of_block, trip);
        //Current trip starts a new block
        start_of_block=trip;
      }
    }

    return trips;
  }
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

  py::class_<TripInfo>(m, "TripInfo")
    .def(py::init<>())
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
    .def_readwrite("energy_left",        &TripInfo::energy_left)
    .def("__repr__",                     &TripInfo::repr);

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
    .def_readwrite("dist_to_depot", &ClosestDepotInfo::dist_to_depot)
    .def("__repr__",                &ClosestDepotInfo::repr);


  py::class_<Model>(m, "Model")
    .def(py::init<const Parameters&, const std::string&, const std::string&>())
    .def("run", &Model::run)
    .def("update_params", &Model:: update_params);

  m.def("GetClosestDepot", &GetClosestDepot, "TODO");
}
