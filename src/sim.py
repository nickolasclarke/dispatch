#!/usr/bin/env python3

import argparse
import collections
import yaml
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
      print(f"Depot {x.name} ({x.lat},{x.lng}) is not near a road network node!")
      good = False
  return good


def generateParams(**kwargs):
  """
  Generate a Dispatch parameters object

  """
  #TODO enforce types
  # simulator parameters
  params = dispatch.Parameters()
  params.battery_cap_kwh       = kwargs.get('battery_cap_kwh',      200)        #kWh
  params.nondepot_charger_rate = kwargs.get('nondepot_charger_rate',500)      #kW
# params.nondepot_charger_den  = kwargs.get(street_charger_den, 0.25)           #ratio TODO implement support   
  params.depot_charger_rate    = kwargs.get('depot_charger_rate',   125)        #kW
  params.bus_cost              = kwargs.get('bus_cost',             500_000)    #dollars
  params.battery_cost_per_kwh  = kwargs.get('battery_cost_per_kwh', 100)        #dollars
  params.depot_charger_cost    = kwargs.get('depot_charger_cost',   50_000)     #dollars
  params.nondepot_charger_cost = kwargs.get('nondepot_charger_cost',600_000)  #dollars
  params.kwh_per_km            = kwargs.get('kwh_per_km',           1.2)        #kWh_per_km
  params.chargers_per_depot    = kwargs.get('chargers_per_depot',   1)          #TODO: Bad default
  # optimizer parameters
  params.generations           = kwargs.get('generations',          [ 50,  50]) #,1000]
  params.mutation_rate         = kwargs.get('mutation_rate',        [0.1,0.05]) #,0.01]
  params.keep_top              = kwargs.get('keep_top',             [  5,   5]) #,   5]
  params.spawn_size            = kwargs.get('spawn_size',           [100, 100]) #,  50]
  params.restarts              = kwargs.get('restarts',             1)
  params.seed                  = kwargs.get('seed',                 0) #Initialize differently each time
  return params

def simulate(input_prefix,
             osm_data,
             depots_filename,
             parameters=None
            ):
  """ TODO Performs an optimized simulation
      parameters, dict of simulation and optimizer parameters to be used, 
  """

  print("Parsing OSM data into router...")
  router = dispatch.Router(osm_data)


  trips      = pd.read_csv(f"{input_prefix}_trips.csv")
  stops      = pd.read_csv(f"{input_prefix}_stops.csv")
  stop_times = pd.read_csv(f"{input_prefix}_stop_times.csv")
  depots     = pd.read_csv(depots_filename)

  #TODO error handling for malformed dict, maybe should be in generateParams()?
  params = generateParams(**parameters)

  #TODO prettify output
  print(f'Scenario Parameters: {params}')
  #TODO: Apply units to tables?

  #Modify the stops table to include depot_id, depot_distance, and depot_time
  #columns
  print("Getting nearest depots...")
  stops = GetNearestDepots(router, trips, stops, depots, search_radius_m=1000)

  #Ensure that depots are near a node in the road network
  print("Testing to see if all depots are near nodes...")
  if not DepotsHaveNodes(router, depots, search_radius_m=1000):
    raise Exception("One or more of the depots don't have road network nodes! Quitting.")

  print("Creating model...")
  model_info = dispatch.ModelInfo(params, trips.to_csv(), stops.to_csv())

  print("Cost with no chargers...")
  no_charger_scenario = dispatch.ModelResults()
  no_charger_scenario.has_charger = {x:False for x in set(trips['start_stop_id'].tolist() + trips['end_stop_id'].tolist())}
  dispatch.run_model(model_info, no_charger_scenario)
  tripsdf = ConvertVectorOfStructsToDataFrame(no_charger_scenario.trips)
  ncbuses = dispatch.count_buses(no_charger_scenario.trips)
  nccost = no_charger_scenario.cost
  nc_tot_buses = sum([x for x in ncbuses.values()])
  nc_tot_chargers = sum([x for x in no_charger_scenario.has_charger.values()])
  print(f"No Chargers Cost ${no_charger_scenario.cost:,.2f}")
  print(f"No Chargers Total buses: {nc_tot_buses}")
  print(f"No Chargers Total chargers: {nc_tot_chargers}")
  
  print("Cost with all chargers...")
  all_charger_scenario = dispatch.ModelResults()
  all_charger_scenario.has_charger = {x:True for x in set(trips['start_stop_id'].tolist() + trips['end_stop_id'].tolist())}
  dispatch.run_model(model_info, all_charger_scenario)
  tripsdf = ConvertVectorOfStructsToDataFrame(all_charger_scenario.trips)
  acbuses = dispatch.count_buses(all_charger_scenario.trips)
  accost = all_charger_scenario.cost
  ac_tot_buses = sum([x for x in acbuses.values()])
  ac_tot_chargers = sum([x for x in all_charger_scenario.has_charger.values()])
  print(f"No Chargers Cost ${all_charger_scenario.cost:,.2f}")
  print(f"No Chargers Total buses: {ac_tot_buses}")
  print(f"No Chargers Total chargers: {ac_tot_chargers}")

  print("Optimizing with chargers...")
  results = dispatch.optimize_model(model_info)
  tripsdf = ConvertVectorOfStructsToDataFrame(results.trips)
  optibuses = dispatch.count_buses(results.trips)
  opti_tot_buses = sum([x for x in optibuses.values()])
  opti_tot_chargers = sum([x for x in results.has_charger.values()])
  cost = results.cost
  print(f"Optimized Cost ${results.cost:,.2f}")
  print(f"Optimized buses: {opti_tot_buses}")
  print(f"Optimized chargers: {opti_tot_chargers}")
  
  full_results = {'opti_trips':tripsdf,
                  'opti_buses':opti_tot_buses,'opti_chargers':opti_tot_chargers,
                  'opti_cost':cost,'opti_depot_counts':optibuses,
                  'nc_buses':nc_tot_buses,'nc_chargers':nc_tot_chargers,
                  'nc_cost':nccost,'nc_depot_counts':ncbuses,
                  'ac_buses':ac_tot_buses,'ac_chargers':ac_tot_chargers,
                  'ac_cost':accost,'ac_depot_counts':acbuses,
                  }
  # code.interact(local=dict(globals(), **locals())) #TODO
  return full_results


#TODO: Used for testing
#python3 sim.py "../../data/parsed_minneapolis" "../../data/minneapolis-saint-paul_minnesota.osm.pbf" "../../data/depots_minneapolis.csv" "/z/out"
#python3 sim.py "../../data/parsed_utahtransportationauthority59" "../../data/osm_utahtransportationauthority59.osm.pbf" "../../data/depots_utahtransportationauthority59.csv" "/z/out"
#python3 sim.py "../../data/parsed_vegas" "../../data/osm_rtcsouthernnevada47.osm.pbf" "../../data/depots_vegas.csv" "/z/out"

def main():
  parser = argparse.ArgumentParser(description='Run the model TODO.')
  parser.add_argument('parsed_gtfs_prefix', type=str, help='TODO')
  parser.add_argument('osm_data',           type=str, help='TODO')
  parser.add_argument('depots_filename',    type=str, help='TODO')
  parser.add_argument('--sim-parameters',   type=str, help='TODO') # a yaml config file
  args = parser.parse_args()

  print(f"parsed_gtfs_prefix: {args.parsed_gtfs_prefix}")
  print(f"osm_data:           {args.osm_data}")
  print(f"depots_filename:    {args.depots_filename}")
  print(f"sim_parameters:     {args.sim_parameters}")

  # sim_parameters assumed to be a yaml config file of k:v pairs mapping to the 
  # potential inputs to generateParams()
  params = None
  if args.sim_parameters is not None:
    with open(args.sim_parameters,'r') as f:
      params = yaml.full_load(f)

  bus_assignments, bus_counts, cost = simulate(args.parsed_gtfs_prefix, 
                                          args.osm_data,
                                          args.depots_filename,
                                          parameters=params
                                        )
if __name__ == '__main__':
  main()
