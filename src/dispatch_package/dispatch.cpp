#include <iostream>
#include <limits>
#include <queue>
#include <random>
#include <sstream>
#include <stdexcept>
#include <tuple>

#include "dispatch.hpp"
#include "utility.hpp"

std::unordered_map<depot_id_t, int> count_buses(const trips_t &trips){
  //Tupe of Time, Depot, Count(-1 or 1)
  typedef std::tuple<double, depot_id_t, int8_t> DepotEvent;

  //Load the priority queue
  std::priority_queue<DepotEvent, std::vector<DepotEvent>, std::greater<DepotEvent>> pq;
  for(const auto &t: trips){
    if(t.start_depot_id.is_valid())
      pq.emplace(t.bus_busy_start, t.start_depot_id, 1); //Going out
    if(t.end_depot_id.is_valid())
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
  for(const auto &mb: max_buses){
    if(mb.first.is_valid())
      max_buses_out[mb.first] = mb.second.max();
  }

  return max_buses_out;
}



trips_t ModelInfo::load_and_sort_trips(const std::string &trips_csv){
  trips_t trips = csv_trips_to_internal(trips_csv);

  //Sort trips into blocks where each block is ordered by start arrival time
  std::stable_sort(trips.begin(), trips.end(), [](const auto &a, const auto &b){ return a.start_arrival_time<b.start_arrival_time; });
  std::stable_sort(trips.begin(), trips.end(), [](const auto &a, const auto &b){ return a.block_id<b.block_id; });

  return trips;
}

void ModelInfo::update_params(const Parameters &new_params){
  params = new_params;
}

std::string ModelResults::repr() const {
  std::ostringstream oss;
  oss<<"<dispatch.ModelResults "
      <<", trips=[vec]"
      <<", has_charger={dict}"
      <<", cost="<<cost
      <<">";
  return oss.str();
}



template<class T>
int dict_value_sum(const T &dict){
  return std::accumulate(dict.begin(), dict.end(), (int)0,
    [](const int &a, const auto &b){
      return a + b.second;
    }
  );
}


void calculate_costs(const ModelInfo &model_info, ModelResults &results){
  const auto &p = model_info.params;

  const auto nondepot_charger_count = dict_value_sum(results.has_charger);
  const auto nondepot_charger_cost = nondepot_charger_count * p.nondepot_charger_cost;

  const auto buses_per_depot = count_buses(results.trips);

  const auto bus_count = dict_value_sum(buses_per_depot);
  const auto bus_cost = bus_count * p.bus_cost;

  const auto battery_cost = bus_count * (p.battery_cap_kwh * p.battery_cost_per_kwh);

  const auto depot_charger_cost = static_cast<int>(buses_per_depot.size())*p.chargers_per_depot * p.depot_charger_cost;

  results.cost = nondepot_charger_cost + bus_cost + battery_cost + depot_charger_cost;
}



void run_block(
  const ModelInfo &model_info,
  const HasCharger &has_charger,
  trips_t::iterator block_start,
  trips_t::iterator block_end,
  int32_t &next_bus_id
){
  const auto &params = model_info.params;
  const auto &stops = model_info.stops;

  const auto nrg_to_depot = [&](stop_id_t stop_id) -> kilowatt_hours {
    return stops.at(stop_id).depot_distance * params.kwh_per_km;
  };

  const auto time_to_depot = [&](stop_id_t stop_id) -> seconds {
    return stops.at(stop_id).depot_time;
  };

  const auto depot_id = [&](stop_id_t stop_id) -> depot_id_t {
    return stops.at(stop_id).depot_id;
  };

  const auto end_trip = [&](trips_t::iterator trip, kilowatt_hours energy_left) -> void {
    const seconds charge_time = std::min(params.battery_cap_kwh, params.battery_cap_kwh - energy_left)/params.depot_charger_rate;
    trip->energy_left = energy_left;
    trip->bus_busy_end = trip->end_arrival_time + time_to_depot(trip->end_stop_id) + charge_time;
    trip->end_depot_id = depot_id(trip->end_stop_id);
  };

  bool new_bus = true;
  kilowatt_hours energy_left = params.battery_cap_kwh;

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
      #ifdef DISPATCH_DEBUG
      std::cerr<<"Start a new trip.\n";
      #endif
    } else {
      // Set trip information if it hasn't already been done (by starting a new bus)
      trip->bus_id = next_bus_id;
      trip->bus_busy_start = trip->start_arrival_time;
    }

    // Note the next_trip
    const auto next_trip = trip+1;
    auto trip_energy = trip->distance * params.kwh_per_km;
    const auto energy_end_to_depot = nrg_to_depot(trip->end_stop_id);

    //If the charger at the end of this trip has a charger and there is a subsequent trip,
    //then we'll use the charger
    if(has_charger.at(trip->end_stop_id) && next_trip!=block_end){
      const auto charge_amount = (trip->wait_time * params.nondepot_charger_rate);
      #ifdef DISPATCH_DEBUG
      std::cerr<<"\tCharging by "<<charge_amount<<" kWh\n"<<std::endl;
      #endif
      trip_energy -= charge_amount;
    }

    // Do we have enough energy to do this trip and get to a depot?
    if(energy_left<trip_energy+energy_end_to_depot){
      // No: That's a problem. Raise a flag. Do the trip and end negative.
      #ifdef DISPATCH_DEBUG
      std::cerr<<"\tEnergy trap found at trip="<<trip->trip_id<<" block_id="<<trip->block_id<<std::endl;
      #endif
      end_trip(trip, energy_left - trip_energy - energy_end_to_depot);
      new_bus = true;
      continue;
    }

    // Note: We have enough energy to complete the trip!

    // If there is no next trip
    if(next_trip==block_end){
      // Finish this trip and go to the nearest depot
      #ifdef DISPATCH_DEBUG
      std::cerr<<"\tNo next trip. End it now.\n";
      #endif
      end_trip(trip, energy_left - trip_energy - energy_end_to_depot);
      return;
    }

    // Do we have enough energy to do a trip after this?
    const auto next_trip_energy = next_trip->distance * params.kwh_per_km;
    const auto energy_next_end_to_depot = nrg_to_depot(next_trip->end_stop_id);
    if(energy_left<trip_energy + next_trip_energy + energy_next_end_to_depot){
      // NO: Do just this trip and then go to a depot.
      #ifdef DISPATCH_DEBUG
      std::cerr<<"\tInsufficient energy, get a new bus.\n";
      #endif
      end_trip(trip, energy_left - trip_energy - energy_end_to_depot);
      new_bus = true;
    } else {
      // YES: Do this trip and continue on to next iteration of the loop
      #ifdef DISPATCH_DEBUG
      std::cerr<<"\tContinue with this bus.\n";
      #endif
      trip->energy_left = energy_left = energy_left - trip_energy;
      trip->bus_busy_end = trip->end_arrival_time;
      new_bus = false;
    }
  }
}



///Simulate bus movement along a route. Information about the journey is
///stored by modifying the block.
///
///@param block_start Iterator pointing to the start of the block
///@param block_end   Iterator pointing one past the end of the block
///@param next_bus_id ID to assign to the next bus that's scheduled. Note that
///                   this is passed by reference because it is incremented
///                   across all the blocks `run_block` is called on.
void run_model(const ModelInfo &model_info, ModelResults &results){
  results.trips = model_info.trips;
  auto start_of_block = results.trips.begin();
  int32_t next_bus_id = 1;
  for(auto trip=results.trips.begin();trip!=results.trips.end();trip++){
    //Have we found a new block?
    if(trip->block_id!=start_of_block->block_id){
      //If so, the trip is a valid end iterator for the previous block
      run_block(model_info, results.has_charger, start_of_block, trip, next_bus_id);
      //Current trip starts a new block
      start_of_block=trip;
    }
  }
  //Run the last block
  run_block(model_info, results.has_charger, start_of_block, results.trips.end(), next_bus_id);

  calculate_costs(model_info, results);
}



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
        closest_depot.depot_id[si] = depot_id_t(di);
        closest_depot.time_to_depot[si] = time;
        closest_depot.dist_to_depot[si] = distance;
      }
    } catch (const std::exception &e) {
      //pass
    }
  }
  return closest_depot;
}
