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
  params.battery_cap_kwh       = 200     #kWh
  params.kwh_per_km            = 1.2     #kWh_per_km
  params.bus_cost              = 500_000 #dollars
  params.battery_cost_per_kwh  = 100     #dollars
  params.depot_charger_cost    = 50_000  #dollars
  params.depot_charger_rate    = 125     #kW
  params.nondepot_charger_cost = 600_000 #dollars
  params.nondepot_charger_rate = 500     #kW
  params.chargers_per_depot    = 1       #TODO: Bad default
  params.generations           = [ 50,  50] #,1000]
  params.mutation_rate         = [0.1,0.05] #,0.01]
  params.keep_top              = [  5,   5] #,   5]
  params.spawn_size            = [100, 100] #,  50]
  params.restarts              = 1

  #Ensure that depots are near a node in the road network
  print("Testing to see if all depots are near nodes...")
  if not DepotsHaveNodes(router, depots, search_radius_m=1000):
    raise Exception("One or more of the depots don't have road network nodes! Quitting.")

  print("Creating model...")
  model_info = dispatch.ModelInfo(params, trips.to_csv(), stops.to_csv())

  print("Cost with no chargers...")
  no_charger_scenario = dispatch.ModelResults()
  no_charger_scenario.has_charger = {x:False for x in stops['stop_id']}
  dispatch.run_model(model_info, no_charger_scenario)
  ncbuses = dispatch.count_buses(no_charger_scenario.trips)
  print(f"NC ${no_charger_scenario.cost:,.2f}")
  print("NC Total buses: {0}".format(sum([x for x in ncbuses.values()])))
  print("NC Total chargers: {0}".format(sum([x for x in no_charger_scenario.has_charger.values()])))

  print("Cost with all chargers...")
  all_charger_scenario = dispatch.ModelResults()
  all_charger_scenario.has_charger = {x:True for x in set(trips['start_stop_id'].tolist() + trips['end_stop_id'].tolist())}
  dispatch.run_model(model_info, all_charger_scenario)
  ncbuses = dispatch.count_buses(all_charger_scenario.trips)
  print(f"NC Cost ${all_charger_scenario.cost:,.2f}")
  print("NC Total buses: {0}".format(sum([x for x in ncbuses.values()])))
  print("NC Total chargers: {0}".format(sum([x for x in all_charger_scenario.has_charger.values()])))

  print("Optimizing with chargers...")
  results = dispatch.optimize_model(model_info)
  tripsdf = ConvertVectorOfStructsToDataFrame(results.trips)
  buses = dispatch.count_buses(results.trips)
  print(f"${results.cost:,.2f}")
  print("Total buses: {0}".format(sum([x for x in buses.values()])))
  print("Total chargers: {0}".format(sum([x for x in results.has_charger.values()])))
  code.interact(local=dict(globals(), **locals()))
  return tripsdf, buses


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

bus_assignments, bus_counts = main(args.parsed_gtfs_prefix, router, args.depots_filename, args.output_filename)
print(f"bus_counts={bus_counts}")