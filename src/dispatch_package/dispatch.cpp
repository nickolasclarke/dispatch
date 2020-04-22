#include <algorithm>
#include <iostream>
#include <limits>
#include <random>
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

  double  bus_busy_start = -1; //Time at which the bus becomes busy on this trip
  double  bus_busy_end   = -1; //Time at which the bus becomes unbusy on this trip
  int32_t bus_id         = -1; //Bus id (unique across all trips) of bus serving this trip
  int32_t start_depot_id = -1; //ID of the depot from which the bus leaves to start this trip. -1 indicates no depot (starts from some previous trip)
  int32_t end_depot_id   = -1; //ID of the depot to which the bus goes when it's done with this trip. -1 indicates no depot (continues on to another trip)
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

  auto nrg_to_depot(int32_t stop_id) const {
    return stops.at(stop_id).depot_distance * params.kwh_per_km;
  }

  auto time_to_depot(int32_t stop_id) const {
    return stops.at(stop_id).depot_time;
  }

  auto depot_id(int32_t stop_id) const {
    return stops.at(stop_id).depot_id;
  }

  void end_trip(trips_t::iterator trip, double energy_left) const {
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
    trips_t::iterator block_start,
    trips_t::iterator block_end, 
    int32_t &next_bus_id
  ) const {
    bool new_bus = true;
    auto energy_left = params.battery_cap_kwh;

    for(auto trip=block_start;trip!=block_end;trip++){ // For each trip in block
      if(new_bus){
        // Start a new bus
        next_bus_id++;
        // Initially our energy is battery capacity minus what we need to get to the trip
        energy_left = params.battery_cap_kwh - nrg_to_depot(trip->start_stop_id);
        // Bus becomes busy when we leave the depot
        trip->bus_busy_start = trip->start_arrival_time - time_to_depot(trip->start_stop_id);
        // Identify the bus
        trip->bus_id = next_bus_id;   
        // Note the depot
        trip->start_depot_id = depot_id(trip->start_stop_id);
        new_bus = false;
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
        std::cerr<<"\nEnergy trap found at trip="<<trip->trip_id<<" block_id="<<trip->block_id<<std::endl;
        end_trip(trip, energy_left - trip_energy - energy_end_to_depot);
        new_bus = true;
        continue;
      }

      // Note: We have enough energy to complete the trip!

      // If there is no next trip
      if(next_trip==block_end){
        // Finish this trip and go to the nearest depot
        end_trip(trip, energy_left - trip_energy - energy_end_to_depot);
        return;
      }

      // Do we have enough energy to do a trip after this?
      const auto next_trip_energy = next_trip->distance * params.kwh_per_km;
      const auto energy_next_end_to_depot = nrg_to_depot(next_trip->end_stop_id);
      if(energy_left<trip_energy + next_trip_energy + energy_next_end_to_depot){
        // NO: Do just this trip and then go to a depot.
        end_trip(trip, energy_left - trip_energy - energy_end_to_depot);
        new_bus = true;
      } else {
        // YES: Do this trip and continue on to next iteration of the loop
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
      if(trip->trip_id!=start_of_block->trip_id || trip->block_id!=start_of_block->block_id){
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
