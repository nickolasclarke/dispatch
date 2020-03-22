# Dispatch

A [discrete-event simulation (DES)](https://en.wikipedia.org/wiki/Discrete-event_simulation) model for simulating deployment of EV bus fleets, using GTFS data

Installation/Compilation
===========================
Prerequisites:
  - Python 3.x, see requirements.txt for python package requirements
  - Julia 1.x, see Project.toml for Julia package requirements
  - Cmake
  - An API key from [OpenMobilityData](https://transitfeeds.com/api/keys)

Acquire necessary tools with:

    sudo apt install cmake zlib1g-dev

Clone the repo with all neccessary submodules

    git clone --recurse-submodules git@github.com:nickolasclarke/dispatch.git

Set up a Python environment if you wish, and install required python packages

    pip install -r requirements.txt

Set up Julia env and install required julia packages. In Julia's `Pkg` manager

    activate .
    instantiate

Now build with the following

    mkdir build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=`pwd` ..
    make
    make install


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

```bash
#Acquire data
./src/pull_gtfs.py acquire data/feeds.db data/gtfs_{feed}.zip

#Validate data and create inputs for model
./src/pull_gtfs.py validate data/feeds.db data/gtfs_{feed}.zip temp/{feed}

#Check validation status
./src/pull_gtfs.py show_validation data/feeds.db data/gtfs_{feed}.zip temp/{feed}

#Run model on a dataset
julia ./sim.jl ../../temp/minneapolis ../../data/minneapolis-saint-paul_minnesota.osm.pbf ../../data/depots_minneapolis.csv /z/out

#Create inputs for a particular dataset
./parse_gtfs.py data/gtfs_minneapolis.zip msp3.pickle

#Identify possible places to put chargers
#./find_chargers.py msp3.pickle
```


Data Acquisition
===========================

- GTFS transit feed data acquired from [OpenMobilityData](https://transitfeeds.com)
- OSM data for all validated feeds acquired from ?





Global Road Network
===========================

Acquire a planet.osm file
-------------------------

See directions [here](https://wiki.openstreetmap.org/wiki/Downloading_data) and
downloads [here](https://planet.openstreetmap.org/).

We got our data from [this link](https://free.nchc.org.tw/osm.planet/pbf/planet-latest.osm.pbf). The file was 50GB.


Extract roads from file
-------------------------

First, install OSM tools:
```bash
sudo apt install osmosis
```
Now, extract the roads:
```bash
osmosis --read-pbf planet-latest.osm.pbf --tf accept-ways highway=* --used-node --write-pbf planet-highways.osm.pbf
```
Running Osmosis on the global dataset will take a while. On our 32 core, 192 GB
RAM, SSD machine it took 8 hours 10 minutes. The resulting file `planet-highways.osm.pbf` was 17GB.

Next, build the contraction hierarchy:
```bash
./routing_preprocess planet-highways.osm.pbf planet-highways.ch
```
Unfortunately, our timer didn't work for this process, but it will take 12-45
hours and require 80+GB of RAM. The resulting file `planet-highways.ch` was 15GB.



Test Julia Routing
=========================

```julia
include("RoutingKit.jl")

router = RoutingKit.Router("/z/msp-highways.osm.pbf", "/z/msp.ch")

brooklyn_park = (lat=45.115208, lng=-93.373463)
south_st_paul = (lat=44.892850, lng=-93.051079)

time_dist = RoutingKit.getTravelTime(router, brooklyn_park.lat, brooklyn_park.lng, south_st_paul.lat, south_st_paul.lng, 3000)
```
