# Dispatch

A Salabim-based model for simulating deployment of EV bus fleets, using GTFS data

Installation/Compilation
===========================

    mkdir build
    cd build
    cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=`pwd` ..
    ninja
    ninja install

# Data Structures

The data is stored in several tables:

## `trips`:

A trip follows a route from the beginning of a route to its end in one
direction. Many different trips take place on each route each day. Trips may
follows subtly different paths which are still labeled as the same route. We
parse the GTFS data into the following format:

 * `trip_id`:              A unique identifier for the trip. Taken from GTFS
 * `trip_headsign`:        The bus's public LED readout for the trip. Taken from 
                           GTFS. Used for debugging.
 * `block_id`:             A unique identifier indicating a string of sequential 
                           trips which are nominally served by the same bus.
 * `start_arrival_time`:   Time at which the bus is supposed to be present at 
                           the starting stop of the trip.
 * `start_departure_time`: Time at which the bus is supposed to depart the 
                           starting stop of the trip. May be the same as
                           `start_arrival_time`.
 * `start_stop_id`:        Unique identifier for the stop at which the trip begins.
 * `start_lat`:            Latitude of the stop idenicated by `start_stop_id`.
 * `start_lng`:            Longitude of the stop idenicated by `start_stop_id`.
 * `end_arrival_time`:     Time at which the bus is supposed to end its trip.
 * `end_departure_time`:   Time at which the bus is supposed to depart its last 
                           stop. May be the same as `end_arrival_time`. Kind of
                           unclear what this is about. TODO
 * `end_stop_id`:          Unique identifier for the stop at which the trip ends.
 * `end_lat`:              Latitude of the stop idenicated by `end_stop_id`.
 * `end_lng`:              Longitude of the stop idenicated by `end_stop_id`.
 * `distance`:             Travel distance of the trip.
 * `duration`:             Travel time of the trip.
 * `wait_time`:            Total time spent sitting at stops during the trip.

## `stops`

## `stop_times`

## `road_segs`

## `seg_props`





Data Flow, Data Structures
===========================

1. `parse_gtfs.py`
---------------------------

`parse_gtfs.py` reads a GTFS file and converts it into an internal data
structure that can be read and understood by the project.



# Data Structures

The data is stored in several tables:

## `trips`:

A trip follows a route from the beginning of a route to its end in one
direction. Many different trips take place on each route each day. Trips may
follows subtly different paths which are still labeled as the same route. We
parse the GTFS data into the following format:

 * `trip_id`:              A unique identifier for the trip. Taken from GTFS
 * `trip_headsign`:        The bus's public LED readout for the trip. Taken from 
                           GTFS. Used for debugging.
 * `block_id`:             A unique identifier indicating a string of sequential 
                           trips which are nominally served by the same bus.
 * `start_arrival_time`:   Time at which the bus is supposed to be present at 
                           the starting stop of the trip.
 * `start_departure_time`: Time at which the bus is supposed to depart the 
                           starting stop of the trip. May be the same as
                           `start_arrival_time`.
 * `start_stop_id`:        Unique identifier for the stop at which the trip begins.
 * `start_lat`:            Latitude of the stop idenicated by `start_stop_id`.
 * `start_lng`:            Longitude of the stop idenicated by `start_stop_id`.
 * `end_arrival_time`:     Time at which the bus is supposed to end its trip.
 * `end_departure_time`:   Time at which the bus is supposed to depart its last 
                           stop. May be the same as `end_arrival_time`. Kind of
                           unclear what this is about. TODO
 * `end_stop_id`:          Unique identifier for the stop at which the trip ends.
 * `end_lat`:              Latitude of the stop idenicated by `end_stop_id`.
 * `end_lng`:              Longitude of the stop idenicated by `end_stop_id`.
 * `distance`:             Travel distance of the trip.
 * `duration`:             Travel time of the trip.
 * `wait_time`:            Total time spent sitting at stops during the trip.

## `stops`

## `stop_times`

## `road_segs`

## `seg_props`





Example Usage
===========================

    ./parse_gtfs.py data/gtfs_minneapolis.zip msp3.pickle
    ./find_chargers.py msp3.pickle
    ./sim.py msp3.pickle

Data Acquisition
===========================

Acquire GTFS transit feed data from "https://transitfeeds.com".

