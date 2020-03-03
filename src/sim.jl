#!/usr/bin/env julia
using Distributions
using JuliaDB
using Unitful

include("RoutingKit.jl")



"""
For a given trip_id, determine how much time is spent waiting at stops during 
that trip.

Args:
  stop_prob  - Probability of stopping at a given stop with zero duration
  zstop_time - Seconds spent at a stop that is otherwise given zero duration
"""
function GetStopChargingTime(stops, trip_id; stop_prob=0.3, zstop_time=10)
    #Get list of stops used by this trip and merge in the stop data
    trip_stops = filter(row->row[:trip_id]==trip_id, stops)
    trip_stops = filter(row->row[:inductive_charging]==true, trip_stops)
    #Total time we are scheduled to be stopped
    stopped_time = sum(trip_stops.stop_duration)

    #Some of the stops with zero duration we do end up stopping at briefly
    zero_count = sum(trip_stops.stop_duration.==0)     #Number of zero-duration stops
    zero_count = rand(Binomial(zero_count, stop_prob)) #Number we actually stop at
    stopped_time += zero_count*zstop_time              #Add time spent at them

    return stopped_time
end



"""
Determine <travel time (s), travel distance (m)> between two latlng points using
the road network held in router.

Args:
    search_radius - Snap stop to closest road network node searching within
                    this radius.
"""
function TimeDistanceBetweenPoints(router, ll1, ll2; search_radius=1000)
    try
        RoutingKit.getTravelTime(router, ll1[:lat], ll1[:lng], ll2[:lat], ll2[:lng], 1000)
        return (time=time_dist[1]u"s", dist=time_dist[2]u"m")
    catch
        println("\nWarning: Couldn't find a node near either ",ll1," or ",ll2)
        return (time=15u"minute", dist=5u"km") 
    end
end



"""
Simulate bus movement along a route. Information about the journey is stored by
modifying "block".

Args:
    block   - Block of trips to be simulated (MODIFIED WITH OUTPUT)
    stop_ll - Dictionary containing the latitudes and longitudes of stops
    router  - Router object used to determine network distances between stops
    depots  - List of depot locations
    params  - Model parameters
"""
function RunBlock(block, stop_ll, router, depots, params)
    prevtrip = block[1]

    #Get time, distance, and energy from depot to route
    bstart_depot = FindClosestDepotByTime(router, stop_ll[prevtrip.start_stop_id], depots)
    block_energy_depot_to_start = bstart_depot[:dist] * params[:kwh_per_km]

    #Modify first trip of block appropriately
    block[1] = merge(block[1], (;
        bus_id=1, 
        energy_left = params[:battery_cap_kwh] - block_energy_depot_to_start, 
        depot = bstart_depot[:id]
    ))

    previ=1
    #Run through all the trips in the block
    for tripi in 1:length(block)
        trip = block[tripi]
        prevtrip = block[previ]

        #TODO: Incorporate charging at the beginning of trip if there is a charger present
        trip_energy = trip.distance * params[:kwh_per_km]

        #TODO: Incorporate energetics of stopping sporadically along the trip
        #trip_energy -= GetStopChargingTime(stops, trip.trip_id) #TODO: Convert to energy

        #Get energetics of completing this trip and then going to a depot
        end_depot = FindClosestDepotByTime(router, stop_ll[trip.end_stop_id], depots)
        energy_from_end_to_depot = end_depot[:dist] * params[:kwh_per_km]

        #Energy left to perform this trip
        energy_left_this_trip = prevtrip.energy_left

        #Do we have enough energy to complete this trip and then go to the depot?
        if energy_left_this_trip - trip_energy - energy_from_end_to_depot < 0u"kW*hr"
            #We can't complete this trip and get back to the depot, so it was better to end the block after the previous trip
            #TODO: Assert previous trip ending stop_id is same as this trip's starting stop_id
            #Get closest depot and energetics to the start of this trip (which is also the end of the previous trip)
            start_depot = FindClosestDepotByTime(router, stop_ll[trip.start_stop_id], depots)
            energy_from_start_to_depot = start_depot[:dist] * params[:kwh_per_km]
            
            #TODO: Something bad has happened if we've reached this: we shouldn't have even made the last trip.
            if energy_from_start_to_depot < prevtrip.energy_left
                println("\nEnergy trap found!")
            end

            #Alter the previous trip to note that we ended it
            energy_left_last_trip = prevtrip.energy_left-energy_from_start_to_depot
            charge_time = (params[:battery_cap_kwh]-energy_left_last_trip)/params[:charging_rate]
            block[previ] = merge(block[previ], (;
                energy_left = energy_left_last_trip,
                bus_busy_end = prevtrip.bus_busy_end+start_depot[:time] + charge_time,
                depot = start_depot[:id]
            ))

            #TODO: Assumes that trip from trip start to depot and from depot to trip start is the same length
            energy_left_this_trip = params[:battery_cap_kwh]-energy_from_start_to_depot

            #Alter this trip so that we start it with a fresh bus
            block[tripi] = merge(block[tripi], (;
                bus_busy_start = trip.start_arrival_time - start_depot[:time],
                bus_id = prevtrip.bus_id+1
            ))
        else
            #We have enough energy to make the trip, so let's start it!
            block[tripi] = merge(block[tripi], (;
                bus_busy_start = trip.start_arrival_time,
                bus_id = prevtrip.bus_id
            ))
        end

        #We have enough energy to finish the trip
        block[tripi] = merge(block[tripi], (;
            bus_busy_end = trip.end_arrival_time,
            energy_left = energy_left_this_trip-trip_energy
        ))

        #This trip is now the previous trip
        previ = tripi
    end

    #Get energetics of getting from the final trip to its depot
    bend_depot = FindClosestDepotByTime(router, stop_ll[prevtrip.start_stop_id], depots)
    block_energy_end_to_depot = bend_depot[:dist] * params[:kwh_per_km]

    #Adjust beginning and end
    block[1]   = merge(block[1], (;bus_busy_start = block[1].bus_busy_start - bstart_depot[:time]))
    block[end] = merge(block[end], (; energy_left = block[end].energy_left - block_energy_end_to_depot))
end



"""

Simulate a set of trips to determine which buses are needed when and how much
energy is used.

Args:
    trips   - List of trips to be taken
    stop_ll - Dictionary containing the latitudes and longitudes of stops
    router  - Router object used to determine network distances between stops
    depots  - List of depot locations
    params  - Model parameters
"""
function Model(trips, stop_ll, router, depots, params)
    trips = sort(trips, :start_arrival_time)

    #Add/zero out the bus_id column
    trips = transform(trips, :bus_id         => -1          *ones(Int64,   length(trips)))
    trips = transform(trips, :energy_left    => -1.0u"kW*hr"*ones(Float64, length(trips)))
    trips = transform(trips, :bus_busy_start => -1.0u"s"    *ones(Float64, length(trips)))
    trips = transform(trips, :bus_busy_end   => -1.0u"s"    *ones(Float64, length(trips)))
    trips = transform(trips, :depot          => -1          *ones(Int64,   length(trips)))

    bus_swaps = []
    for (block_id,block) in groupby(identity, trips, :block_id)
        print(".")
        RunBlock(block, stop_ll, router, depots, params)
    end
    println("done")

    return trips
end




let                                #Local scope for memoization
    global FindClosestDepotByTime  #Allow function to escape local scope
    cached = Dict()                #Memoization dictionary
    """
    Given a set of depots, find the closest one to a query point.

    Args:
        router   - Router object used to determine network distances between stops
        query_ll - LatLng point to find closest depot to
        depots   - List of depot locations

    Returns:
        (seconds to nearest depot, meters to nearest depot, id of nearest depot)
    """
    function FindClosestDepotByTime(router, query_ll, depots)
        if haskey(cached, query_ll) #Return cached answer, if we have one
            return cached[query_ll]
        end
        mintime = (time=Inf*u"s",dist=Inf*u"m",id=0) #Infinitely bad choice of depot
        for i in 1:length(depots)                    #Search all depots for a better one
            timedist = TimeDistanceBetweenPoints(router, query_ll, depots[i])
            if timedist[:time]<mintime[:time]
                mintime = (timedist..., id=i)
            end
        end
        cached[query_ll] = mintime
        return mintime
    end
end


#TODO: Used for testing
#ARGS = ["../../temp/minneapolis", "../../data/minneapolis-saint-paul_minnesota.osm.pbf", "../../data/depots_minneapolis.csv", "/z/out"]
#julia -i sim.jl "../../temp/minneapolis" "../../data/minneapolis-saint-paul_minnesota.osm.pbf" "../../data/depots_minneapolis.csv" "/z/out"

if length(ARGS)!=4
    println("Syntax: <Program> <Parsed GTFS Output Prefix> <OSM Data> <Depots File> <Model Output>")
    exit(0)
end

input_prefix    = ARGS[1]
osm_data        = ARGS[2]
depots_filename = ARGS[3]
output_filename = ARGS[4]

router     = RoutingKit.Router(osm_data)
trips      = loadtable(input_prefix * "_trips.csv")
stops      = loadtable(input_prefix * "_stops.csv")
stop_times = loadtable(input_prefix * "_stop_times.csv")
depots     = loadtable(depots_filename)

#Apply units to tables
trips = transform(trips, :distance             => :distance             => x->x*u"m")
trips = transform(trips, :start_arrival_time   => :start_arrival_time   => x->x*u"s")
trips = transform(trips, :start_departure_time => :start_departure_time => x->x*u"s")
trips = transform(trips, :end_arrival_time     => :end_arrival_time     => x->x*u"s")
trips = transform(trips, :end_departure_time   => :end_departure_time   => x->x*u"s")
trips = transform(trips, :wait_time            => :wait_time            => x->x*u"s")

stop_times = transform(stop_times, :stop_duration => :stop_duration => x->x*u"s")

#TODO: Get these from command-line args or a config file
params = (
  battery_cap_kwh = 200u"kW*hr", 
  kwh_per_km      = 1.2u"kW*hr/km",
  charging_rate   = 150u"kW"
)

#Convert stops table into dictionary for fast lookups.
#TODO: less verbose way to do this?
stop_ll = Dict(select(stops, :stop_id)[i] => (lat=select(stops, :lat)[i], lng=select(stops, :lng)[i]) for i in 1:length(stops))

#Run the model
bus_assignments = Model(trips, stop_ll, router, depots, params);
