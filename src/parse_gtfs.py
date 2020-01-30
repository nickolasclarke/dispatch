#!/usr/bin/env python3
import collections
from functools import partial
import itertools
import math
import pickle
import sys

import numpy as np
import pandas as pd
import partridge as ptg
import pyproj
import shapely as shp
import shapely.ops

from utility import *


def pairwise(iterable):
  """
  s -> (s0,s1), (s1,s2), (s2, s3), ...
  """
  a, b = itertools.tee(iterable)
  next(b, None)
  return zip(a, b)



def dedupe_adjacent(iterable):
  prev = None
  for item in iterable:
    if prev is None or item != prev:
      prev = item
      yield item



wgs_to_aea = partial(
    pyproj.transform,
    pyproj.Proj(init='epsg:4326'), # source coordinate system
    pyproj.Proj(init='esri:102003')) # destination coordinate system
    #Converts to a US Contiguous Albert Equal Area projection. TODO: This fails on pyproj>2.0. 



def ChangeProjection(geom):
    geom = shapely.ops.transform(wgs_to_aea, geom)  # apply projection
    #Something for getting spherical distances from linestrings of lat-long coordinates goes here
    return geom



def GetGeoDistanceFromLineString(line_string):
    line_string = shapely.ops.transform(wgs_to_aea, line_string)  # apply projection
    #Something for getting spherical distances from linestrings of lat-long coordinates goes here
    return line_string.length



def MatchColumn(df, colname):
  """Turns start_x and end_x into x while ensuring they were the same"""
  assert (df['start_'+colname]==df['end_'+colname]).all()
  df = df.drop(columns=['end_'+colname])
  df = df.rename(index=str, columns={'start_'+colname: colname})
  return df



def GenerateTrips(gtfs, date, service_ids):
  """TODO:
  """
  # filter for bus routes only
  bus_routes = gtfs.routes[gtfs.routes['route_type'] == 3]['route_id']
  trips      = gtfs.trips[gtfs.trips.route_id.isin(bus_routes)]

  #Select the service ids from that date. Note that trip_ids are still unique
  #because they include service_ids as a substring

  trips = gtfs.trips[gtfs.trips.service_id.isin(service_ids)]

  #Change the projection of the stops
  gtfs.stops['geometry'] = gtfs.stops['geometry'].map(ChangeProjection)

  #Clean up stops data
  gtfs.stops['lat'] = gtfs.stops.geometry.y
  gtfs.stops['lng'] = gtfs.stops.geometry.x

  #Combine trips with stop data
  trips = trips.merge(gtfs.stop_times, left_on='trip_id', right_on='trip_id')

  #Filter trips to only regularly scheduled stops
  trips = trips[trips.pickup_type.astype(int)==0] #TODO: What does this mean?
  trips = trips[trips.drop_off_type.astype(int)==0] #TODO: What does this mean?

  trips = trips.sort_values(['trip_id','route_id','service_id','stop_sequence'])

  #TODO: Maybe concatenate trip_id with a service_id and route_id and direction_id to ensure it is unique?

  #Drop unneeded columns
  trips = DropColumnsIfPresent(trips, [ #refactor to be exclusive see issue #4
    'wheelchair_accessible',
    'pickup_type',
    'drop_off_type',
    'service_id',       #Trip ids include this as a substring
    'direction_id',
    'route_id',
    'stop_sequence'     #This is held by the sorted order
  ])

  #TODO: block_id is supposed to indicate continuous travel by a *single vehicle*
  #and, thus, might provide a good way of simplifying the problem

  #We need to get information about when each trip begins and ends, so we sort and
  #then group by trip. Each group is then ordered such that its first and last
  #entries contain the needed information.
  first_stops = trips.groupby(by='trip_id').first().reset_index()
  last_stops  = trips.groupby(by='trip_id').last().reset_index()

  first_stops = first_stops.merge(gtfs.stops[['stop_id','lat','lng']], how='left', on='stop_id')
  last_stops  = last_stops.merge (gtfs.stops[['stop_id','lat','lng']], how='left', on='stop_id')

  trips = first_stops.merge(last_stops, on="trip_id")

  #Now, we need to clean up the merge

  #Rename columns
  to_rename = dict()
  for c in trips.columns:
    if c.endswith("_x"):
      to_rename[c] = "start_"+c.replace("_x","")
    elif c.endswith("_y"):
      to_rename[c] = "end_"+c.replace("_y","")

  trips = trips.rename(index=str,columns=to_rename)

  #Ensure that start and end values are the same and reduce them to a single
  #column
  trips = MatchColumn(trips,'shape_id')
  trips = MatchColumn(trips,'block_id')
  trips = MatchColumn(trips,'trip_headsign')

  #Merge in distances
  gtfs.shapes['distance'] = gtfs.shapes['geometry'].map(GetGeoDistanceFromLineString)
  trips = trips.merge(gtfs.shapes[['shape_id', 'distance']], how='left', on='shape_id')
  trips = trips.drop(columns='shape_id')
  trips['duration'] = trips['end_arrival_time']-trips['start_departure_time']

  #Get wait time between trips
  trips = trips.sort_values(["block_id", "start_arrival_time"])
  next_trip = trips.shift(-1).copy()
  trips['wait_time'] = next_trip['start_departure_time']-trips['end_arrival_time']
  trips['wait_time'][trips['block_id']!=next_trip['block_id']]=np.nan

  return trips



def GenerateStops(gtfs):
  """TODO:
  """
  #TODO: Make sure this table doesn't have weird extra columns
  #Change the projection of the stops
  stops = gtfs.stops.copy()

  stops['geometry'] = stops['geometry'].map(ChangeProjection)

  #Clean up stops data
  stops['lat'] = stops.geometry.y
  stops['lng'] = stops.geometry.x

  stops['inductive_charging'] = False # Whether there's an inductive charger
  stops['evse'] = False               # Whether there is a traditional EVSE charger
  stops['nearest_charger'] = None     # StopID if there is a nearby charger the bus can go to (mostly applicable to terminii)

  return stops



def GenerateRoadSegments(gtfs):
  """TODO:
  """
  #TODO: Make sure this table doesn't have weird extra columns
  #Change the projection of the stops
  def SegLength(p1,p2):
    xd = p1[0]-p2[0]
    yd = p1[1]-p2[1]
    return math.sqrt(xd*xd+yd*yd)

  shapes = gtfs.shapes.copy()
  shapes['geometry'] = shapes['geometry'].map(ChangeProjection)
  #NOTE: `hash(x)` is optional, though potentially slightly more performant. Could use `x` instead.
  shapes['seg_hash'] = [[x for x in pairwise(dedupe_adjacent(geom.coords))] for geom in shapes['geometry']]
  shapes = shapes[['shape_id', 'seg_hash']]

  all_shape_segs = [item for sublist in shapes['seg_hash'] for item in sublist]

  #Print count of how many times each road segment is used
  print(collections.Counter(all_shape_segs))

  #Flatten list for segments
  shape_props = [{"seg_hash": seg_hash, "charging":False} for seg_hash in set(all_shape_segs)]
  shape_props = pd.DataFrame(shape_props)
  shape_props['distance'] = [[SegLength(*x)] for x in shape_props['seg_hash']]
  return shapes, shape_props



def GenerateStopTimes(gtfs):
  #TODO: Worry about pickup type and dropoff type?
  stop_times = gtfs.stop_times[['trip_id', 'stop_id']]
  stop_times['stop_duration'] = gtfs.stop_times['departure_time'] - gtfs.stop_times['arrival_time']
  return stop_times


if len(sys.argv)!=3:
  print("Syntax: {0} <GTFS File> <Output Pickle Name>".format(sys.argv[0]))
  sys.exit(-1)

feed_file   = sys.argv[1]
output_file = sys.argv[2]

#######################
# Filter for service ids

# find the busiest date
date, service_ids = ptg.read_busiest_date(feed_file)

print("Service id chosen = {0}".format(service_ids))

#Load file twice so that we don't modify it within these functions
trips                = GenerateTrips(ptg.load_geo_feed(feed_file), date, service_ids)
stops                = GenerateStops(ptg.load_geo_feed(feed_file))
stop_times           = GenerateStopTimes(ptg.load_geo_feed(feed_file))
road_segs, seg_props = GenerateRoadSegments(ptg.load_geo_feed(feed_file))

data = {
  "trips":      trips,
  "stops":      stops,
  "stop_times": stop_times,
  "road_segs":  road_segs,
  "seg_props":  seg_props
}

#Output
#trips.drop(columns=['trip_id']).to_csv(output_file, index=False) #TODO?
with open(output_file, 'wb') as handle:
  pickle.dump(data, handle, protocol=4)
