#!/usr/bin/env python3

"""Read a GTFS and transform it into standardized internal data structures.

The output is stored in several tables.

trips
=========================

All of the trips (from one end of a route to the other) that are made.

Keys:
* `trip_id`:              A unique identifier for the trip. Opaque.
* `trip_headsign`:        Human-readable trip description. Only for debugging.
* `block_id`:             Trips with the same block_id can, in theory, be 
                          conveniently served by the same bus.
* `start_arrival_time'`:  When the bus needs to arrive at the start of the trip.
* `start_departure_time`: When the bus depats the start to being the trip.
* `start_stop_id`:        `stop_id` of the start of the trip.
* `start_lat`:            Latitude of the start of the trip.
* `start_lng'`:           Longitude of the start of the trip.
* `end_arrival_time`:     When the bus arrives at the end of the trip.
* `end_departure_time`:   When the bus can move on from the end of the trip.
* `end_stop_id`:          `stop_id` of the end of the trip.
* `end_lat'`:             Latitude of the end of the trip.
* `end_lng`:              Longitude of the end of the trip.
* `distance`:             Length of the trip in meters.
* `duration`:             Duration of the trip in seconds.
* `wait_time`:            How long the bus is scheduled to spend waiting at 
                          stops during the trip.

stops
=========================

All of the stops.

Keys:
* `stop_id`:              Unique id for the stop.
* `lat`:                  Latitude of the stop.
* `lng`:                  Longitude of the stop.
* `x`:                    Projected x coordinate of the stop.
* `y`:                    Projected y coordinate of the stop.
* `inductive_charging`:   Whether inductive charging is available at the stop
* `nearest_charger`:      StopID if there is a nearby charger the bus can go to (mostly applicable to terminii)
* `evse`:                 Whether there is a traditional EVSE charger
* `stop_name`:            Human-readable name of the stop. For debugging.
* `geometry`:             TODO

stop_times
=========================

How long a bus is scheduled to spend at a stop during each trip.

Keys:
* `trip_id`:              Unique id of the trip.
* `stop_id`:              Unique id of the stop.
* `stop_duration`:        How long, in seconds, the bus is scheduled to spend at the stop.

road_segs
=========================

TODO: Test table for putting in overhead wiring.

seg_props
=========================

TODO: Test table for apply properties to segments

Keys:
* `seg_hash`:             Hash of the segment in question
* `charging`:             Whether a bus can charge while traverse this segment.
* `distance`:             Length of the segment. TODO: need traversal time.
"""

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



#GTFS uses numeric identifiers to indicate what kind of vehicles serve a route.
#The relevant bus-like identifiers for us are below.
route_types = list(itertools.chain.from_iterable([
  [3], # Standard Route Type code

  [700, 701, #Extended Bus codes
  702, 703, 
  704, 705, 
  706, 707, 
  708, 709, 
  710, 711, 
  712, 713, 
  714, 715, 716],

  [200, 201, #Extended Coach codes
  202, 203, 
  204, 205, 
  206, 207, 
  208, 209],

  [800] #Extended Trollybus codes
]))



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



def wgs_to_aea(x,y):
  """Converts to a US Contiguous Albert Equal Area projection."""
  # TODO: This fails on pyproj>2.0. 
  return pyproj.transform(
    pyproj.Proj(init='epsg:4326'),    # source coordinate system
    pyproj.Proj(init='esri:102003'),  # destination coordinate system
    x,
    y
  )
    


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
  if not f'start_{colname}' in df.columns:
    raise Exception(f'start_{colname} is not a column!')
  if not f'end_{colname}' in df.columns: 
    raise Exception(f'end_{colname} is not a column!')
  if not (df[f'start_{colname}']==df[f'end_{colname}']).all():
    raise Exception("Could not match start and end columns!")
  df = df.drop(columns=[f'end_{colname}'])
  df = df.rename(index=str, columns={f'start_{colname}': colname})
  return df



def GenerateTrips(gtfs, date, service_ids):
  """TODO:
  """
  # filter for bus routes only
  bus_routes = gtfs.routes[gtfs.routes.route_type.isin(route_types)]['route_id']
  trips      = gtfs.trips[gtfs.trips.route_id.isin(bus_routes)]

  #Select the service ids from that date. Note that trip_ids are still unique
  #because they include service_ids as a substring (TODO: Is this universal?)
  trips = gtfs.trips[gtfs.trips.service_id.isin(service_ids)]

  #Clean up stops data
  gtfs.stops['lat'] = gtfs.stops.geometry.y
  gtfs.stops['lng'] = gtfs.stops.geometry.x

  #Change the projection of the stops
  gtfs.stops['geometry'] = gtfs.stops['geometry'].map(ChangeProjection)

  gtfs.stops['x'] = gtfs.stops.geometry.x
  gtfs.stops['y'] = gtfs.stops.geometry.y

  #Combine trips with stop data
  trips = trips.merge(gtfs.stop_times, left_on='trip_id', right_on='trip_id')

  #Add missing pickup_type and drop_off_type columns
  if 'pickup_type' not in trips.columns:
    trips['pickup_type'] = 0
  if 'drop_off_type' not in trips.columns:
    trips['drop_off_type'] = 0
  if 'trip_headsign' not in trips.columns:
    trips['trip_headsign'] = "##none##"

  #0 or empty - Regularly scheduled drop off. (https://developers.google.com/transit/gtfs/reference)
  trips.pickup_type   = trips.pickup_type.fillna(0)
  trips.drop_off_type = trips.drop_off_type.fillna(0)
  trips.trip_headsign = trips.trip_headsign.fillna("##none##")

  #Filter trips to only regularly scheduled stops. Values are:
  #0 or empty - Regularly scheduled pickup.
  #1 - No pickup available.
  #2 - Must phone agency to arrange pickup.
  #3 - Must coordinate with driver to arrange pickup.
  trips = trips[trips.pickup_type.astype(int)==0]
  trips = trips[trips.drop_off_type.astype(int)==0]

  trips = trips.sort_values(['trip_id','route_id','service_id','stop_sequence'])

  #TODO: Maybe concatenate trip_id with a service_id and route_id and direction_id to ensure it is unique?

  #Drops 'route_id', 'timepoint', 'wheelchair_accessible', 'pickup_type',
  #'drop_off_type', 'service_id', 'direction_id', 'route_id', 'stop_sequence'
  trips = trips[[
                'service_id',
                'trip_id',
                'trip_headsign',
                'block_id',
                'shape_id',
                'arrival_time',
                'departure_time',
                'stop_id',
               ]]

  #TODO: block_id is supposed to indicate continuous travel by a *single vehicle*
  #and, thus, might provide a good way of simplifying the problem

  #We need to get information about when each trip begins and ends, so we sort and
  #then group by trip. Each group is then ordered such that its first and last
  #entries contain the needed information.
  first_stops = trips.groupby(by='trip_id').first().reset_index()
  last_stops  = trips.groupby(by='trip_id').last().reset_index()

  first_stops = first_stops.merge(gtfs.stops[['stop_id','lat','lng','x','y']], how='left', on='stop_id')
  last_stops  = last_stops.merge (gtfs.stops[['stop_id','lat','lng','x','y']], how='left', on='stop_id')

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

  has_next_trip = (trips['block_id']!=next_trip['block_id']).index
  trips.loc[has_next_trip, 'wait_time'] = np.nan

  return trips



def GenerateStops(gtfs):
  """TODO:
  """
  #TODO: Make sure this table doesn't have weird extra columns
  #Change the projection of the stops
  stops = gtfs.stops.copy()

  stops['lat'] = stops.geometry.y
  stops['lng'] = stops.geometry.x

  geom = stops['geometry'].copy().map(ChangeProjection)

  #Clean up stops data
  stops['x'] = geom.x
  stops['y'] = geom.y

  #Drop unneeded columns including 'wheelchair_boarding', 'stop_url', 'zone_id',
  #'stop_desc', 'location_type', 'stop_code', 'geometry'
  stops = stops[['stop_id', 'stop_name', 'lat', 'lng', 'x', 'y']]

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
  # print(collections.Counter(all_shape_segs))

  #Flatten list for segments
  shape_props = [{"seg_hash": seg_hash, "charging":False} for seg_hash in set(all_shape_segs)]
  shape_props = pd.DataFrame(shape_props)
  shape_props['distance'] = [[SegLength(*x)] for x in shape_props['seg_hash']]
  return shapes, shape_props



def GenerateStopTimes(gtfs):
  #TODO: Worry about pickup type and dropoff type?
  stop_times = gtfs.stop_times.copy()
  stop_times['stop_duration'] = stop_times['departure_time'] - stop_times['arrival_time']
  stop_times = stop_times[['trip_id', 'stop_id', 'stop_duration']]
  return stop_times



def DoesFeedLoad(gtfs):
  try:
    feed = ptg.load_feed(gtfs)
    return True
  except Exception as e:
    print(e)
    return False



def HasBusRoutes(gtfs_filename):
  #Check to see if the feed contains any buses
  feed = ptg.load_feed(gtfs_filename)
  return feed.routes.route_type.isin(route_types).any()



def HasBlockIDs(gtfs_filename):
  gtfs = ptg.load_feed(gtfs_filename)
  if 'block_id' not in gtfs.trips.columns: #block_id column missing
    return False
  if gtfs.trips['block_id'].isna().any():  #block_id column there but some are missing data
    return False
  return True



def ParseFile(gtfs_filename, output_prefix):
  # find the busiest date
  date, service_ids = ptg.read_busiest_date(gtfs_filename)

  print("Service id chosen = {0}".format(service_ids))

  #Load file twice so that we don't modify it within these functions
  trips      = GenerateTrips(ptg.load_geo_feed(gtfs_filename), date, service_ids)
  stops      = GenerateStops(ptg.load_geo_feed(gtfs_filename))
  stop_times = GenerateStopTimes(ptg.load_geo_feed(gtfs_filename))
  # road_segs, seg_props = GenerateRoadSegments(ptg.load_geo_feed(gtfs_filename))

  trips.to_csv(output_prefix+"_trips.csv", index=False)
  stops.to_csv(output_prefix+"_stops.csv", index=False)
  stop_times.to_csv(output_prefix+"_stop_times.csv", index=False)



def main():
  if len(sys.argv)!=3:
    print("Syntax: {0} <GTFS File> <Output Pickle Name>".format(sys.argv[0]))
    sys.exit(-1)

  ParseFile(gtfs_filename=sys.argv[1], output_prefix=sys.argv[2])



if __name__ == '__main__':
  main()
