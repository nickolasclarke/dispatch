#include <sstream>

#include "csv.hpp"
#include "data_frames.hpp"
#include "data_structures.hpp"

stops_t csv_stops_to_internal(const std::string &inpstr){
  stops_t stops;

  std::stringstream ss;
  ss<<inpstr;

  io::CSVReader<4> in("stops.csv", ss);
  in.read_header(io::ignore_extra_column, "stop_id", "depot_id", "depot_time", "depot_distance");

  stop_id_t::type  stop_id_in;
  depot_id_t::type depot_id;
  seconds::type    depot_time;
  meters::type     depot_distance;
  while(in.read_row(stop_id_in, depot_id, depot_time, depot_distance)){
    stop_id_t stop_id(stop_id_in);
    if(stops.count(stop_id)!=0)
      throw std::runtime_error("stop_id was in the table twice!");
    stops[stop_id] = StopInfo{
      stop_id,
      depot_id_t(depot_id),
      seconds(depot_time),
      meters(depot_distance)
    };
  }

  return stops;
}



trips_t csv_trips_to_internal(const std::string &inpstr){
  trips_t trips;

  std::stringstream ss;
  ss<<inpstr;

  io::CSVReader<8> in("trips.csv", ss);
  in.read_header(io::ignore_extra_column, "trip_id","block_id","start_arrival_time","start_stop_id","end_arrival_time","end_stop_id","distance","wait_time");

  std::string      trip_id;
  block_id_t::type block_id;
  seconds::type    start_arrival_time;
  stop_id_t::type  start_stop_id;
  seconds::type    end_arrival_time;
  stop_id_t::type  end_stop_id;
  meters::type     distance;
  seconds::type    wait_time;
  while(in.read_row(trip_id,block_id,start_arrival_time,start_stop_id,end_arrival_time,end_stop_id,distance,wait_time)){
    trips.push_back(TripInfo{
      trip_id,
      block_id_t(block_id),
      seconds(start_arrival_time),
      stop_id_t(start_stop_id),
      seconds(end_arrival_time),
      stop_id_t(end_stop_id),
      meters(distance),
      seconds(wait_time),
      seconds::invalid(),        //bus_busy_start
      seconds::invalid(),        //bus_busy_end
      -1,                        //bus_id
      depot_id_t::invalid(),     //start_depot_id
      depot_id_t::invalid(),     //end_depot_id
      kilowatt_hours::invalid(), //energy_left
    });
  }

  return trips;
}