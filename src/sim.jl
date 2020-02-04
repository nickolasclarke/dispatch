#!/usr/bin/env julia
using CSV
using DataFrames
using Distributions



"""
Return an object logging the time, departure stop, and block id of the 
bus that needs swapping
"""
function SwapBus(time, stop_id, block_id)
    return Dict(
        :datetime=> time,
        :stop_id=>  stop_id,
        :block_id=> block_id
    )
end



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
    #Total time we are scheduled to be stopped
    stopped_time = sum(trip_stops.stop_duration)

    #Some of the stops with zero duration we do end up stopping at briefly
    zero_count = sum(trip_stops.stop_duration.==0)     #Number of zero-duration stops
    zero_count = rand(Binomial(zero_count, stop_prob)) #Number we actually stop at
    stopped_time += zero_count*zstop_time              #Add time spent at them

    return stopped_time
end



function RunBlock(block, stops; battery_cap_kwh)
    #Set up the initial parameters of a bus
    energy = battery_cap_kwh
    bus_swaps = []
    block_id = first(block[!, :block_id])

    trip_energy_req = 0

    #Run through all the trips in the block
    for trip in eachrow(block)
        #TODO: Incorporate charging at the beginning of trip if there is a charger present
        trip_energy_req = trip.distance # + route_to_start.distance) * self.kwh_per_km #TODO

        stop_charge  = GetStopChargingTime(stops, trip.trip_id)
        # route_charge = self.GetRouteChargingTime(trip.shape_id)

        trip_energy_req -= stop_charge #- route_charge

        if energy <= trip_energy_req
            push!(bus_swaps, SwapBus(trip.start_arrival_time, trip.start_stop_id, block_id))
            energy = battery_cap_kwh
        end

        energy = energy - trip_energy_req
    end

    return bus_swaps
end



function Model(trips, stops, stop_times, battery_cap_kwh=200, kwh_per_km=1.2, charging_rate=150)
    stops = join(stop_times, stops, on=:stop_id, kind=:left)
    stops = filter(row->row[:inductive_charging] == true, stops)
    trips = sort(trips, [:block_id, :start_arrival_time])

    bus_swaps = []
    print("Simulating...\n")
    @time for block in DataFrames.groupby(trips, :block_id)
        append!(bus_swaps, RunBlock(block, stops, battery_cap_kwh=200))
    end

    #Construct output DataFrame
    #From: https://stackoverflow.com/a/60049139/752843
    return DataFrame([NamedTuple{Tuple(keys(d))}(values(d)) for d in bus_swaps])
end



if length(ARGS)!=2
    println("Syntax: <Program> <Parsed GTFS Output Prefix> <Model Output>")
    exit(0)
end

input_prefix    = ARGS[1]
output_filename = ARGS[2]

trips      = CSV.read(input_prefix * "_trips.csv")
stops      = CSV.read(input_prefix * "_stops.csv")
stop_times = CSV.read(input_prefix * "_stop_times.csv")

@time bus_swaps = Model(trips, stops, stop_times)
print(bus_swaps)

CSV.write(output_filename, bus_swaps)
