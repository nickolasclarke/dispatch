#!/usr/bin/env python3
import geopandas as gpd
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import pickle
import sys

from shapely.geometry import Point, MultiPoint
from shapely.ops import transform
from sklearn.cluster import DBSCAN

PROJECTION_NAME = 'esri:102003'

# TODO: import pickle file from sys
if len(sys.argv)!=2:
  print("Syntax: {0} <Parsed GTFS File>".format(sys.argv[0]))
  sys.exit(-1)

input_file = sys.argv[1]
# with open(input_file, 'rb') as handle:
#   data = pickle.load(handle)
data = pd.read_pickle(input_file)

trips      = data['trips']
stops      = data['stops']
stop_times = data['stop_times']
road_segs  = data['road_segs']
seg_props  = data['seg_props']

departure_stops = trips[['start_stop_id','start_lat','start_lng']]
arrival_stops   = trips[['end_stop_id','end_lat','end_lng']]

#rename columns
#TODO Refactor later
departure_stops = departure_stops.rename(
                    columns={'start_stop_id': 'stop_id', 'start_lat': 'lat', 'start_lng': 'lng'})
arrival_stops = arrival_stops.rename(
                    columns={'end_stop_id': 'stop_id', 'end_lat': 'lat', 'end_lng': 'lng'})
# create a list of all stops
stops_with_nearby_charger = departure_stops.append(arrival_stops).reset_index(drop=True)
# find stops that should have nearby chargers
stops_with_nearby_charger = stops_with_nearby_charger.groupby(['stop_id','lat','lng']).size().reset_index(name='trip_count').sort_values('trip_count')
#Filter by frequency TODO: (update >0 to be a variable to filter for most popular termini)
stops_with_nearby_charger = stops_with_nearby_charger[stops_with_nearby_charger['trip_count']>0] 
stops_with_nearby_charger = gpd.GeoDataFrame(stops_with_nearby_charger, crs={'init': PROJECTION_NAME}, geometry=gpd.points_from_xy(stops_with_nearby_charger.lng, stops_with_nearby_charger.lat))
# Cluster stops within 250m with DBSCAN. Derived from https://geoffboeing.com/2014/08/clustering-to-reduce-spatial-data-set-size/
epsilon = 250 #this in meters because the euclidean distance is in metres due to the ESRI:102003 projection
db = DBSCAN(eps=epsilon, min_samples=1, algorithm='ball_tree', metric='euclidean').fit(stops_with_nearby_charger[['lng','lat']])
stops_with_nearby_charger['stop_cluster'] = db.labels_

#Arbitrarily take the first point having a particular cluster_id and extract that information as the representative point
stops_with_charger = stops_with_nearby_charger.groupby('stop_cluster').first().reset_index()
stops_with_charger = gpd.GeoDataFrame(stops_with_charger, crs={'init': PROJECTION_NAME}, geometry=gpd.points_from_xy(stops_with_charger.lng, stops_with_charger.lat))

# convert stops_with_nearby_charger into a geodataframe
# fig, ax = plt.subplots(figsize=[15, 15])
# stops_with_nearby_charger.plot(ax=ax, color='black')
# stops_with_charger.plot(ax=ax, column='stop_cluster', cmap='jet')
# ax.set_title('Full data set vs DBSCAN reduced set')
# ax.set_xlabel('Longitude')
# ax.set_ylabel('Latitude')
# plt.show()


# If the stop is a termini, update stop to reflect it has a charger.
stops['evse'] = False # Whether there is a traditional EVSE charger
for idx, row in stops_with_charger.iterrows():
  stops['evse'][stops['stop_id']==row['stop_id']] = True

#TODO: Refactor naming. Append the id of the nearest charger, if any, to the full stops df.  
stops['nearest_charger'] = None
for idx, row in stops_with_nearby_charger.iterrows():
  this_stop_id = row['stop_id']
  this_stop_cluster = row['stop_cluster']
  stopid_of_stop_in_cluster_with_charger = stops_with_charger[stops_with_charger['stop_cluster']==this_stop_cluster]['stop_id'].item()
  stops['nearest_charger'][stops['stop_id']==this_stop_id] = stopid_of_stop_in_cluster_with_charger

#Output

data = {
  "trips":      trips,
  "stops":      stops,
  "stop_times": stop_times,
  "road_segs":  road_segs,
  "seg_props":  seg_props
}

with open(input_file, 'wb') as handle:
  pickle.dump(data, handle, protocol=4)
