#pragma once

#include <unordered_map>
#include <vector>

#include "units.hpp"
#include "identifiers.hpp"

const auto dinf = std::numeric_limits<double>::infinity();
const auto dnan = std::numeric_limits<double>::quiet_NaN();

typedef int32_t depot_id_t;
typedef std::string trip_id_t;
typedef int32_t block_id_t;



//TODO: Set all parameters to invalid values to ensure we are passed good ones
struct Parameters {
  kilowatt_hours battery_cap_kwh       = 200.0_kWh;
  kWh_per_km     kwh_per_km            = 1.2_kWh_per_km;
  dollars        bus_cost              = 500'000.0_dollars;
  dollars        battery_cost_per_kwh  = 100.0_dollars;
  dollars        depot_charger_cost    = 50'000.0_dollars;
  kilowatts      depot_charger_rate    = 125.0_kW;
  dollars        nondepot_charger_cost = 600'000.0_dollars;
  kilowatts      nondepot_charger_rate = 500.0_kW;
  int32_t        chargers_per_depot    = 1; //TODO: Bad default
  std::string repr() const {
    return std::string("<dispatch.Parameters ")
      + "battery_cap_kwh="         + std::to_string(battery_cap_kwh)
      + ", kwh_per_km="            + std::to_string(kwh_per_km)
      + ", bus_cost="              + std::to_string(bus_cost)
      + ", battery_cost_per_kwh="  + std::to_string(battery_cost_per_kwh)
      + ", depot_charger_cost="    + std::to_string(depot_charger_cost)
      + ", depot_charger_rate="    + std::to_string(depot_charger_rate)
      + ", nondepot_charger_cost=" + std::to_string(nondepot_charger_cost)
      + ", nondepot_charger_rate=" + std::to_string(nondepot_charger_rate)
      + ", chargers_per_depot="    + std::to_string(chargers_per_depot)
      + ">";
  }
};



struct StopInfo {
  stop_id_t  stop_id;
  depot_id_t depot_id;
  seconds    depot_time;
  meters     depot_distance;
};



//TODO: Assumes that trip from trip start to depot and from depot to
//trip start is the same length
struct ClosestDepotInfo {
  std::vector<depot_id_t> depot_id;
  std::vector<double>     time_to_depot;
  std::vector<double>     dist_to_depot;
  ClosestDepotInfo(const int N){
    depot_id.resize(N, depot_id_t::invalid());
    time_to_depot.resize(N, dnan);
    dist_to_depot.resize(N, dnan);
  }
  std::string repr() const {
    return std::string("<dispatch.ClosestDepotInfo of length "+std::to_string(depot_id.size())+" with properties depot_id, time_to_depot, dist_to_depot>");
  }
};



struct TripInfo {
  trip_id_t   trip_id;
  block_id_t  block_id;
  seconds     start_arrival_time;
  stop_id_t   start_stop_id;
  seconds     end_arrival_time;
  stop_id_t   end_stop_id;
  meters      distance;

  seconds        bus_busy_start = seconds::invalid();        //Time at which the bus becomes busy on this trip
  seconds        bus_busy_end   = seconds::invalid();        //Time at which the bus becomes unbusy on this trip
  int32_t        bus_id         = -1;                        //Bus id (unique across all trips) of bus serving this trip
  depot_id_t     start_depot_id = depot_id_t::invalid();     //ID of the depot from which the bus leaves to start this trip. -1 indicates no depot (starts from some previous trip)
  depot_id_t     end_depot_id   = depot_id_t::invalid();     //ID of the depot to which the bus goes when it's done with this trip. -1 indicates no depot (continues on to another trip)
  kilowatt_hours energy_left    = kilowatt_hours::invalid();

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
typedef std::unordered_map<stop_id_t, StopInfo> stops_t;
