#!/usr/bin/env python3

import argparse
import collections
import code #TODO

import numpy as np
import pandas as pd

import dispatch



def ConvertVectorOfStructsToDataFrame(vos):
  """Converts a vector of structures to a dataframe"""
  #Sample the first struct to get the column names
  column_names = [x for x in dir(vos[0]) if not x.startswith('__')]
  data = collections.defaultdict(list)
  for x in vos:
    for c in column_names:
      data[c].append(getattr(x,c))
  return pd.DataFrame(data)



def GetInductiveChargeTimes(dp):
  """
  For a given trip_id, determine how much time is spent waiting at stops during
  that trip.

  Args:
    stop_prob  - Probability of stopping at a given stop with zero duration
    dp - Data pack
  """
  pass
    #Create a set of stops with inductive charging
      # stops = Set(select(filter(row->row[:inductive_charging]==true, dp.stops), :stop_id))
    #Filter stop_times to those with inductive charigng
      # stop_times = filter(row->in(row[:stop_id], stops), dp.stop_times)
    #Summarize stop time and number of stops by trip
        # stop_times = groupby(
        #     (
        #         stop_duration = :stop_duration=>sum,
        #         stops         = :stop_duration=>x->length(x),
        #         zero_stops    = :stop_duration=>x->sum(x.==0)
        #     ),
        #     stop_times,  #Table to groupby
        #     :trip_id     #Groupby key
        # )
    #Number we actually stop at
      # stop_times = transform(stop_times, :zero_stops => dp.params.zstops_frac_stopped_at*select(stop_times, :zero_stops))
    #How long we spend stopped, in total
      #stop_times = transform(stop_times, :stop_duration => select(stop_times, :stop_duration) .+ select(stop_times, :zero_stops).*dp.params.zstops_average_time)
      # stop_times = select(stop_times, (:trip_id, :stop_duration))
      # return Dict(x.trip_id=>x for x in stop_times)




def GetNearestDepots(router, trips, stops, depots, search_radius_m=1000):
  stops = stops.copy()
  #Get set of stops that are actually at the end of trips
  trip_stops = set(trips.start_stop_id.tolist() + trips.end_stop_id.tolist())
  #Filter stops to this list
  trip_stops = stops['stop_id'].isin(trip_stops)
  #For each trip stop, find its closest depot
  ret = dispatch.GetClosestDepot(
    router,
    stops[trip_stops]['lat'].to_numpy(),
    stops[trip_stops]['lng'].to_numpy(),
    depots['lat'].to_numpy(),
    depots['lng'].to_numpy(),
    search_radius_m
  )
  #Add columns to stops containing the depot information
  stops['depot_id']                      = -1
  stops['depot_distance']                = np.nan
  stops['depot_time']                    = np.nan
  stops.loc[trip_stops,'depot_id']       = ret.depot_id
  stops.loc[trip_stops,'depot_distance'] = ret.dist_to_depot
  stops.loc[trip_stops,'depot_time']     = ret.time_to_depot
  return stops



def DepotsHaveNodes(router, depots, search_radius_m=1000):
  """Tests to see if depots are near road network nodes.

  Args:
    router: A `dispatch.Router()` object
    depots (DataFrame): A DataFrame of depots
    search_radius_m (int): How many metres around the depot we should search for
                           a road network node

  Returns: True if all depots are near nodes; otherwise, false.
  """
  good = True
  for _,x in depots.iterrows():
    try:
      router.getNearestNode(x.lat, x.lng, search_radius_m)
    except Exception as e:
      print(e)
      print(f"Depot {x.name} ({x.lat},{x.lng} is not near a road network node!")
      good = False
  return good



def main(
  input_prefix,
  router,
  depots_filename,
  output_filename
):
  trips      = pd.read_csv(f"{input_prefix}_trips.csv")
  stops      = pd.read_csv(f"{input_prefix}_stops.csv")
  stop_times = pd.read_csv(f"{input_prefix}_stop_times.csv")
  depots     = pd.read_csv(depots_filename)

  #TODO: Apply units to tables?

  #Modify the stops table to include depot_id, depot_distance, and depot_time
  #columns
  print("Getting nearest depots...")
  stops = GetNearestDepots(router, trips, stops, depots, search_radius_m=1000)

  params = dispatch.Parameters()
  params.battery_cap_kwh         = 240.0 #u"kW*hr",
  params.kwh_per_km              = 1.2/1000   #u"kW*hr/km", #TODO: Check units everywhere
  params.charging_rate           = 150.0 #u"kW",
  params.search_radius           = 1.0   #u"km",
  params.zstops_frac_stopped_at  = 0.2
  params.zstops_average_time     = 10    #u"s",

  #Ensure that depots are near a node in the road network
  print("Testing to see if all depots are near nodes...")
  if not DepotsHaveNodes(router, depots, search_radius_m=1000):
    raise Exception("One or more of the depots don't have road network nodes! Quitting.")
  
  data_pack = {
    "depots":     depots,     #List of depot locations
    "params":     params,     #Model parameters
    "router":     router,     #Router object used to determine network distances between stops
    "stop_times": stop_times,
    "stops":      stops
  }

  print("Creating model...")
  model = dispatch.Model(params, trips.to_csv(), stops.to_csv())
  ret = ConvertVectorOfStructsToDataFrame(model.run())
  code.interact(local=dict(globals(), **locals())) 
  return ret
  

#TODO: Used for testing
#python3 sim.py "../../data/parsed_minneapolis" "../../data/minneapolis-saint-paul_minnesota.osm.pbf" "../../data/depots_minneapolis.csv" "/z/out"

parser = argparse.ArgumentParser(description='Run the model TODO.')
parser.add_argument('parsed_gtfs_prefix', type=str, help='TODO')
parser.add_argument('osm_data',           type=str, help='TODO')
parser.add_argument('depots_filename',    type=str, help='TODO')
parser.add_argument('output_filename',    type=str, help='TODO')
args = parser.parse_args()

print(f"parsed_gtfs_prefix: {args.parsed_gtfs_prefix}")
print(f"osm_data:           {args.osm_data}")
print(f"depots_filename:    {args.depots_filename}")
print(f"output_filename:    {args.output_filename}")

print("Parsing OSM data into router...")
router = dispatch.Router(args.osm_data)

bus_assignments = main(args.parsed_gtfs_prefix, router, args.depots_filename, args.output_filename)
