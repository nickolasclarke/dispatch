#!/usr/bin/env python3
import copy
import math
import pickle
import sys

import numpy as np
import pandas as pd
import geopandas as gpd

from multiprocessing import Pool


class BusModel:
    """
    TODO
    """
    def __init__(self, gtfs, battery_cap_kwh=200, kwh_per_km=1.2, charging_rate=150):
        #TODO: Is an explicit copy needed here?
        self.gtfs            = copy.deepcopy(gtfs)
        self.gtfs['trips']   = gtfs['trips'].sort_values(['block_id', 'start_arrival_time'])
        self.battery_cap_kwh = battery_cap_kwh
        self.kwh_per_km      = kwh_per_km
        self.charging_rate   = charging_rate
        self.bus_swaps       = pd.DataFrame(columns=['datetime', 'stop_id', 'block_id'])

    def run(self):
        """
        Run the model
        """
        p = Pool()
        p.starmap(self.run_block, self.gtfs['trips'].groupby(['block_id']))

        # for block_id, group in self.gtfs['trips'].groupby(['block_id']):
        #     self.run_block(block_id, group)
        #     print(f'Block {block_id} complete')

    def GetStopChargingTime(self, trip_id):
        #TODO: Could preprocess this
        #Shortcuts to needed information
        stops      = self.gtfs['stops']
        stop_times = self.gtfs['stop_times']
        #Get list of stops used by this trip and merge in the stop data
        stop_times = stop_times[stop_times.trip_id==trip_id].merge(stops, how='left', on='stop_id')
        #Extract only those stops with inductive charging
        inductive_charging = stops[stops.inductive_charging]
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
        Log the time, departure stop, and block id of the bus that needs swapping
        """
        self.bus_swaps = self.bus_swaps.append({
            'datetime': time,
            'stop_id':  stop_id,
            'block_id': block_id
        }, ignore_index=True)  

    def run_block(self, block_id, trips):
        """
        TODO 
        """
        #Set up the initial parameters of a bus
        bus = {'energy': self.battery_cap_kwh}

        #Run through all the trips in the block
        for trip in trips.itertuples(): #TODO trips is the column names of the groups, not a group itself. How to properly iterate through a groupby?
            trip_start_time = trip.start_arrival_time #TODO -travel_time_to_trip
            trip_end_time   = trip.end_arrival_time   #TODO +travel_time_to_trip

            #TODO: Incorporate charging at the beginning of trip if there is a charger present

            # trip_start_coords = {'lat': trip.start_lat, 'lng': trip.start_lng}
            # trip_end_coords   = {'lat': trip.end_lat,   'lng': trip.end_lng  }

            trip_energy_req = trip.distance # + route_to_start.distance) * self.kwh_per_km

            stop_charge  = self.GetStopChargingTime(trip.trip_id)
            # route_charge = self.GetRouteChargingTime(trip.shape_id)

            trip_energy_req -= stop_charge #- route_charge

            if bus['energy'] <= trip_energy_req:
                self.SwapBus(trip.start_arrival_time, trip.start_stop_id, trip.block_id)
                bus['energy'] = self.battery_cap_kwh

            bus['energy'] = bus['energy'] - trip_energy_req
        print(f'Block {block_id} complete')


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
