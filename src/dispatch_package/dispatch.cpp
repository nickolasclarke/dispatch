#include <algorithm>
#include <iostream>
#include <limits>
#include <queue>
#include <random>
#include <sstream>
#include <stdexcept>
#include <tuple>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "pybind11_conversions.hpp"

<<<<<<< HEAD
#include "csv.hpp"
#include "identifiers.hpp"
=======
#include "data_frames.hpp"
#include "data_structures.hpp"
>>>>>>> 9c975f9... Rearranging code
#include "routingkit.hpp"
#include "utility.hpp"

namespace py = pybind11;

<<<<<<< HEAD
const auto dinf = std::numeric_limits<double>::infinity();
const auto dnan = std::numeric_limits<double>::quiet_NaN();

typedef std::string trip_id_t;

=======
>>>>>>> 9c975f9... Rearranging code
////////////////////////////////
//RANDOM NUMBERS
////////////////////////////////

typedef std::mt19937 our_random_engine;

our_random_engine& rand_engine(){
  thread_local our_random_engine eng;
  return eng;
}

void seed_rand(unsigned long seed){
  #pragma omp critical
  if(seed==0){
    std::uint_least32_t seed_data[std::mt19937::state_size];
    std::random_device r;
    std::generate_n(seed_data, std::mt19937::state_size, std::ref(r));
    std::seed_seq q(std::begin(seed_data), std::end(seed_data));
    rand_engine().seed(q);
  } else {
    rand_engine().seed(seed);
  }
}

double r_uniform(double from, double thru){
  std::uniform_real_distribution<double> distribution(from,thru);
  return distribution(rand_engine());
}



std::unordered_map<depot_id_t, int> count_buses(const trips_t &trips){
  //Tupe of Time, Depot, Count(-1 or 1)
  typedef std::tuple<double, depot_id_t, int8_t> DepotEvent;

  //Load the priority queue
  std::priority_queue<DepotEvent, std::vector<DepotEvent>, std::greater<DepotEvent>> pq;
  for(const auto &t: trips){
    if(t.start_depot_id!=-1)
      pq.emplace(t.bus_busy_start, t.start_depot_id, 1); //Going out
    if(t.end_depot_id!=-1)
      pq.emplace(t.bus_busy_end,   t.end_depot_id,  -1);  //Coming in
  }

  //Run the priority queue
  std::unordered_map<depot_id_t, RangeTracking<int>> max_buses;
  while(!pq.empty()){
    const auto c = pq.top();
    pq.pop();
    max_buses[std::get<1>(c)] += std::get<2>(c);
  }

  //Get the counts
  std::unordered_map<depot_id_t, int> max_buses_out;
  for(const auto &mb: max_buses)
    max_buses_out[mb.first] = mb.second.max();

  return max_buses_out;
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

  kilowatt_hours nrg_to_depot(stop_id_t stop_id) const {
    return stops.at(stop_id).depot_distance * params.kwh_per_km;
  }

  seconds time_to_depot(stop_id_t stop_id) const {
    return stops.at(stop_id).depot_time;
  }

  auto depot_id(stop_id_t stop_id) const {
    return stops.at(stop_id).depot_id;
  }

  void end_trip(trips_t::iterator trip, kilowatt_hours energy_left) const {
    trip->energy_left = energy_left;
    trip->bus_busy_end = trip->end_arrival_time + time_to_depot(trip->end_stop_id);
    trip->end_depot_id = depot_id(trip->end_stop_id);
  }

  ///Simulate bus movement along a route. Information about the journey is
  ///stored by modifying the block.
  ///
  ///@param block_start Iterator pointing to the start of the block
  ///@param block_end   Iterator pointing one past the end of the block
  ///@param next_bus_id ID to assign to the next bus that's scheduled. Note that
  ///                   this is passed by reference because it is incremented
  ///                   across all the blocks `run_block` is called on.
  void run_block(
    const HasCharger  &has_charger,
    trips_t::iterator block_start,
    trips_t::iterator block_end,
    int32_t &next_bus_id
  ) const {
    bool new_bus = true;
    auto energy_left = params.battery_cap_kwh;

    for(auto trip=block_start;trip!=block_end;trip++){ // For each trip in block
      if(new_bus){
        // Initially our energy is battery capacity minus what we need to get to the trip
        energy_left = params.battery_cap_kwh - nrg_to_depot(trip->start_stop_id);
        // Bus becomes busy when we leave the depot.
        trip->bus_busy_start = trip->start_arrival_time - time_to_depot(trip->start_stop_id);

        // Note that we assume the bus doesn't charge at the start depot.

        // Identify the bus
        trip->bus_id = next_bus_id++;
        // Note the depot
        trip->start_depot_id = depot_id(trip->start_stop_id);
        new_bus = false;
        std::cerr<<"Start a new trip.\n";
      } else {
        // Set trip information if it hasn't already been done (by starting a new bus)
        trip->bus_id = next_bus_id;
        trip->bus_busy_start = trip->start_arrival_time;
      }

      // Note the next_trip
      const auto next_trip = trip+1;
      const auto trip_energy = trip->distance * params.kwh_per_km;
      const auto energy_end_to_depot = nrg_to_depot(trip->end_stop_id);

      // Do we have enough energy to do this trip and get to a depot?
      if(energy_left<trip_energy+energy_end_to_depot){
        // No: That's a problem. Raise a flag. Do the trip and end negative.
        std::cerr<<"\tEnergy trap found at trip="<<trip->trip_id<<" block_id="<<trip->block_id<<std::endl;
        end_trip(trip, energy_left - trip_energy - energy_end_to_depot);
        new_bus = true;
        continue;
      }

      // Note: We have enough energy to complete the trip!

      // If there is no next trip
      if(next_trip==block_end){
        // Finish this trip and go to the nearest depot
        std::cerr<<"\tNo next trip. End it now.\n";
        end_trip(trip, energy_left - trip_energy - energy_end_to_depot);
        return;
      }

      // Do we have enough energy to do a trip after this?
      const auto next_trip_energy = next_trip->distance * params.kwh_per_km;
      const auto energy_next_end_to_depot = nrg_to_depot(next_trip->end_stop_id);
      if(energy_left<trip_energy + next_trip_energy + energy_next_end_to_depot){
        // NO: Do just this trip and then go to a depot.
        std::cerr<<"\tEnd the trip.\n";
        end_trip(trip, energy_left - trip_energy - energy_end_to_depot);
        new_bus = true;
      } else {
        // YES: Do this trip and continue on to next iteration of the loop
        std::cerr<<"\tContinue the trip.\n";
        trip->energy_left = energy_left = energy_left - trip_energy;
        new_bus = false;
      }
    }
  }

  trips_t run() const {
    auto trips = trips_template;
    auto start_of_block = trips.begin();
    int32_t next_bus_id = 1;
    for(auto trip=trips.begin();trip!=trips.end();trip++){
      //Have we found a new block?
      if(trip->block_id!=start_of_block->block_id){
        //If so, the trip is a valid end iterator for the previous block
        run_block(start_of_block, trip, next_bus_id);
        //Current trip starts a new block
        start_of_block=trip;
      }
    }
    //Run the last block
    run_block(start_of_block, trips.end(), next_bus_id);

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
        closest_depot.depot_id[si] = depot_id_t::make(di);
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


  py::class_<Model>(m, "Model")
    .def(py::init<const Parameters&, const std::string&, const std::string&>())
    .def("run", &Model::run)
    .def("update_params", &Model:: update_params);

  m.def("GetClosestDepot", &GetClosestDepot, "TODO");

  m.def("count_buses", &count_buses, "TODO");
}
