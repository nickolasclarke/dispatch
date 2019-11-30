#!/usr/bin/env python3
import copy
import math
import pickle
import sys

import numpy as np
import pandas as pd
import geopandas as gpd

from multiprocessing import Pool
from more_itertools  import peekable


class BusModel:
    """
    TODO
    """
    def __init__(self, gtfs, battery_cap_kwh=200, kwh_per_km=1.2, charging_rate=150):
        #TODO: Is an explicit copy needed here?
        self.gtfs            = copy.deepcopy(gtfs)
        self.gtfs['trips']   = gtfs['trips'].sort_values(['block_id', 'start_arrival_time'])
        self.stops           = self.gtfs['stops']
        self.battery_cap_kwh = battery_cap_kwh
        self.kwh_per_km      = kwh_per_km
        self.charging_rate   = charging_rate
        self.bus_swaps       = pd.DataFrame(columns=['datetime', 'stop_id', 'block_id'])

    def run(self):
        """
        Run the model
        """
        p = Pool()
        #Return list of lists where each list is a block's list of bus swaps 
        bus_swaps = p.starmap(self.run_block, groups)
        #Flatten list of lists
        bus_swaps = [y for x in bus_swaps for y in x]
        self.bus_swaps = self.bus_swaps.append(bus_swaps, ignore_index=True)

    def GetStopChargingTime(self, trip_id):
        #TODO: Could preprocess this
        #Shortcuts to needed information
        stop_times = self.gtfs['stop_times']
        #Get list of stops used by this trip and merge in the stop data
        stop_times = stop_times[stop_times.trip_id==trip_id].merge(self.stops, how='left', on='stop_id')
        #Extract only those stops with inductive charging
        inductive_charging = self.stops[self.stops.inductive_charging]
        #Total time stopped
        stopped_time = np.sum(stop_times.stop_duration)
        #Some of the stops with zero duration we do end up stopping at briefly
        stop_prob  = 0.3 #Probability of stopping at a stop with zero duration
        zstop_time = 10  #Time spent stopped at the stop
        zero_count = sum(stop_times.stop_duration==0) #Number of zero-duration stops
        #Number we actually stop at
        #zero_count = np.random.binomial(zero_count, stop_prob, size=None) #Non-deterministic
        zero_count = int(stop_prob*zero_count) #Deterministic
        zero_time = zero_count*zstop_time
        stopped_time += zero_time

        return zero_time

    def GetRouteChargingTime(self, shape_id):
        #TODO: Could preprocess this
        #Shortcuts to needed information
        shape = self.gtfs['shapes'][self.gtfs['shapes'].shape_id==shape_id]
        assert len(shape)==1
        seg_hash = shape['seg_hash'][0]

        #Extract segments in this route
        segs = self.gtfs['shape_props'][self.gtfs['shape_props'].isin(seg_hash)]
        charge_distance = sum(segs[segs.charging].distance)

        return charge_distance/11.176 #25MPH in m/s. TODO: Fix

    def SwapBus(self, time, stop_id, block_id):
        """
        Return an object logging the time, departure stop, and block id of the 
        bus that needs swapping
        """
        return {
            'datetime': time,
            'stop_id':  stop_id,
            'block_id': block_id
        }

    def run_block(self, block_id, trips):
        """
        TODO 
        """
        #Set up the initial parameters of a bus
        bus = {'energy': self.battery_cap_kwh}
        bus_swaps = []
        # filter stops for only those in the block
        block_stops_ids = trips.start_stop_id.unique()
        block_stops     = self.stops[self.stops['stop_id'].isin(block_stops_ids)]
        p = peekable(trips.itertuples()) #create a peekable iterator so we can look ahead at upcoming trips
        trip_start_time = p.peek().start_arrival_time #TODO -travel_time_to_trip
        trip_end_time   = p.peek().start_arrival_time #TODO +travel_time_to_trip

        #Run through all the trips in the block
        for trip in p: #TODO trips is the column names of the groups, not a group itself. How to properly iterate through a groupby?
            trip_start_time = trip.start_arrival_time #TODO -travel_time_to_trip
            trip_end_time   = trip.end_arrival_time   #TODO +travel_time_to_trip
            #charging at the beginning of trip if there is a charger present
            if block_stops.loc[block_stops['stop_id'] == trip.start_stop_id]['evse'].bool():
                next_trip   = p.peek() # find the next trip. TODO add a default value for the end of the block
                charge_time =  trip_end_time  - next_trip.start_arrival_time# find the time available for charging between trips. What format is time in?
                energy_between_trips = charge_time * (self.charging_rate / 60)
                trip_energy_req      = (trip.distance * self.kwh_per_km) - energy_between_trips #TODO (trip.distance + route_to_start.distance ) * self.kwh_per_km - energy_between_trips
            else:
                trip_energy_req = (trip.distance) * self.kwh_per_km #TODO (trip.distance + route_to_start.distance) * self.kwh_per_km

            stop_charge  = self.GetStopChargingTime(trip.trip_id)
            # enroute_charge = self.GetRouteChargingTime(trip.shape_id)

            trip_energy_req -= stop_charge #- enroute_charge

            if bus['energy'] <= trip_energy_req:
                bus_swaps.append(self.SwapBus(trip.start_arrival_time, trip.start_stop_id, trip.block_id))
                bus['energy'] = self.battery_cap_kwh

            bus['energy'] = bus['energy'] - trip_energy_req
        print(f'Block {block_id} complete')
        return bus_swaps


if len(sys.argv)!=3:
  print("Syntax: {0} <Parsed GTFS File> <Model Output>".format(sys.argv[0]))
  sys.exit(-1)

gtfs_filename   = sys.argv[1]
output_filename = sys.argv[2]
gtfs_input      = pd.read_pickle(gtfs_filename, compression='infer')
model           = BusModel(gtfs_input)
model.run()

with open(output_filename, 'wb') as handle:
  pickle.dump(model.bus_swaps, handle, protocol=4)
