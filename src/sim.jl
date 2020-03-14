#!/usr/bin/env julia
using JuliaDB
using Unitful

include("RoutingKit.jl")



"""
For a given trip_id, determine how much time is spent waiting at stops during
that trip.

Args:
  stop_prob  - Probability of stopping at a given stop with zero duration
  dp - Data pack
"""
function GetInductiveChargeTimes(dp)
    #Create a set of stops with inductive charging
    stops = Set(select(filter(row->row[:inductive_charging]==true, dp.stops), :stop_id))
    #Filter stop_times to those with inductive charigng
    stop_times = filter(row->in(row[:stop_id], stops), dp.stop_times)
    #Summarize stop time and number of stops by trip
    stop_times = groupby(
        (
            stop_duration = :stop_duration=>sum,
            stops         = :stop_duration=>x->length(x), 
            zero_stops    = :stop_duration=>x->sum(x.==0)
        ),
        stops_times, #Table to groupby
        :trip_id     #Groupby key
    )
    #Number we actually stop at
    stop_times = transform(:zero_stops = dp.params.zstops_frac_stopped_at*zero_stops)
    #How long we spend stopped, in total
    stop_times = transform(:stop_duration => stop_duration .+ zero_stops.*dp.params.zstops_average_time)
    stop_times = select(stop_times, (:trip_id, :stop_duration))
    return Dict(x.trip_id=>x for x in stop_times)
end



"""
Determine <travel time (s), travel distance (m)> between two latlng points using
the road network held in router.

Args:
    params - Parameter pack including `search_radius` - Snap stop to closest
             road network node searching within this radius.
    dp     - A data pack
"""
function TimeDistanceBetweenPoints(ll1, ll2, dp)
    try
      time_dist = RoutingKit.getTravelTime(dp.router, ll1.lat, ll1.lng, ll2.lat, ll2.lng, Int32(ustrip(u"m", dp.params.search_radius)))
      return (time=time_dist[1]u"s", dist=time_dist[2]u"m")
    catch e
      errmsg = sprint(showerror, e)
      if occursin("start",errmsg)
        println("No node near start position: ", ll1)
        return dp.params.default_td_tofrom_depot
      elseif occursin("target",errmsg)
        println("No node near target position: ", ll2)
        return dp.params.default_td_tofrom_depot
      else
        println(e)
        println("\nWarning: Couldn't find a node near either ",ll1," or ",ll2)
      end
    end
end



"""
Simulate bus movement along a route. Information about the journey is stored by
modifying "block".

Args:
    block - Block of trips to be simulated (MODIFIED WITH OUTPUT)
    dp    - A data pack
"""
function RunBlock(block, dp)
    prevtrip = block[1]

    #Get time, distance, and energy from depot to route
    bstart_depot = FindClosestDepotByTime(router, stop_ll[prevtrip.start_stop_id], dp)
    block_energy_depot_to_start = bstart_depot[:dist] * dp.params[:kwh_per_km]

    #Modify first trip of block appropriately
    block[1] = merge(block[1], (;
        bus_id=1,
        energy_left = dp.params[:battery_cap_kwh] - block_energy_depot_to_start,
        depot = bstart_depot[:id]
    ))

    previ=1
    #Run through all the trips in the block
    for tripi in 1:length(block)
        trip = block[tripi]
        prevtrip = block[previ]

        #TODO: Incorporate charging at the beginning of trip if there is a charger present
        trip_energy = trip.distance * dp.params[:kwh_per_km]

        #TODO: Incorporate energetics of stopping sporadically along the trip
        #trip_energy -= GetStopChargingTime(stops, trip.trip_id) #TODO: Convert to energy

        #Get energetics of completing this trip and then going to a depot
        end_depot = FindClosestDepotByTime(router, stop_ll[trip.end_stop_id], dp)
        energy_from_end_to_depot = end_depot[:dist] * dp.params[:kwh_per_km]

        #Energy left to perform this trip
        energy_left_this_trip = prevtrip.energy_left

        #Do we have enough energy to complete this trip and then go to the depot?
        if energy_left_this_trip - trip_energy - energy_from_end_to_depot < 0u"kW*hr"
            #We can't complete this trip and get back to the depot, so it was better to end the block after the previous trip
            #TODO: Assert previous trip ending stop_id is same as this trip's starting stop_id
            #Get closest depot and energetics to the start of this trip (which is also the end of the previous trip)
            start_depot = FindClosestDepotByTime(router, stop_ll[trip.start_stop_id], dp)
            energy_from_start_to_depot = start_depot[:dist] * dp.params[:kwh_per_km]

            #TODO: Something bad has happened if we've reached this: we shouldn't have even made the last trip.
            if energy_from_start_to_depot < prevtrip.energy_left
                println("\nEnergy trap found!")
            end

            #Alter the previous trip to note that we ended it
            energy_left_last_trip = prevtrip.energy_left-energy_from_start_to_depot
            charge_time = (dp.params[:battery_cap_kwh]-energy_left_last_trip)/dp.params[:charging_rate]
            block[previ] = merge(block[previ], (;
                energy_left = energy_left_last_trip,
                bus_busy_end = prevtrip.bus_busy_end+start_depot[:time] + charge_time,
                depot = start_depot[:id]
            ))

            #TODO: Assumes that trip from trip start to depot and from depot to trip start is the same length
            energy_left_this_trip = dp.params[:battery_cap_kwh]-energy_from_start_to_depot

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
    bend_depot = FindClosestDepotByTime(router, stop_ll[prevtrip.start_stop_id], dp)
    block_energy_end_to_depot = bend_depot[:dist] * dp.params[:kwh_per_km]

    #Adjust beginning and end
    block[1]   = merge(block[1], (;bus_busy_start = block[1].bus_busy_start - bstart_depot[:time]))
    block[end] = merge(block[end], (; energy_left = block[end].energy_left - block_energy_end_to_depot))
end



"""

Simulate a set of trips to determine which buses are needed when and how much
energy is used.

Args:
    trips     - List of trips to be taken
    data_pack - A data pack
"""
function Model(trips, data_pack)
    trips = sort(trips, :start_arrival_time)

    #Add/zero out the bus_id column
    trips = transform(trips, :bus_id         => -1          *ones(Int64,   length(trips)))
    trips = transform(trips, :energy_left    => -1.0u"kW*hr"*ones(Float64, length(trips)))
    trips = transform(trips, :bus_busy_start => -1.0u"s"    *ones(Float64, length(trips)))
    trips = transform(trips, :bus_busy_end   => -1.0u"s"    *ones(Float64, length(trips)))
    trips = transform(trips, :depot          => -1          *ones(Int64,   length(trips)))

    #Add the inductive charging time to the data pack
    data_pack = (data_pack..., inductive_charge_time = GetInductiveChargeTimes(data_pack))

    bus_swaps = []
    for (block_id,block) in groupby(identity, trips, :block_id)
        print(".")
        RunBlock(block, data_pack)
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
        query_ll - LatLng point to find closest depot to
        dp       - A data pack

    Returns:
        (seconds to nearest depot, meters to nearest depot, id of nearest depot)
    """
    function FindClosestDepotByTime(query_ll, dp)
        if haskey(cached, query_ll) #Return cached answer, if we have one
            return cached[query_ll]
        end
        mintime = (time=Inf*u"s",dist=Inf*u"m",id=0) #Infinitely bad choice of depot
        for i in 1:length(depots)                    #Search all depots for a better one
            timedist = TimeDistanceBetweenPoints(query_ll, depots[i], dp)
            if timedist[:time]<mintime[:time]
                mintime = (timedist..., id=i)
            end
        end
        cached[query_ll] = mintime
        return mintime
    end
end



function DepotsHaveNodes(router, depots, params)
    good = true
    for i in 1:length(depots)
        try
            road_node = RoutingKit.getNearestNode(router, depots[i].lat, depots[i].lng, Int32(ustrip(u"m", params.search_radius)))
        catch e
            println(e)
            println("Depot ",depots[i].name," (",depots[i].lat,",",depots[i].lng,") is not near a road network node!")
            good=false
        end
    end
    return good
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
  battery_cap_kwh         = 200.0u"kW*hr",
  kwh_per_km              = 1.2u"kW*hr/km",
  charging_rate           = 150.0u"kW",
  search_radius           = 1.0u"km",
  zstops_frac_stopped_at  = 0.2,
  zstops_average_time     = 10u"s",
  default_td_tofrom_depot = (time=15u"minute", dist=5u"km")
)

#Convert stops table into dictionary for fast lookups.
#TODO: less verbose way to do this?
stop_ll = Dict(select(stops, :stop_id)[i] => (lat=select(stops, :lat)[i], lng=select(stops, :lng)[i]) for i in 1:length(stops))

#Ensure that depots are near a node in the road network
if ~DepotsHaveNodes(router, depots, params)
    throw("One or more of the depots don't have road network nodes! Quitting.")
end

data_pack = (
    stops      = stops,      
    stop_times = stop_times,
    router     = router,     #Router object used to determine network distances between stops
    stop_ll    = stop_ll,    #Dictionary containing the latitudes and longitudes of stops
    depots     = depots,     #List of depot locations
    params     = params      #Model parameters
)

#Run the model
bus_assignments = Model(trips, data_pack);
