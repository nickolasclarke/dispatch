cmake_minimum_required(VERSION 3.13)

add_subdirectory(routingkit)

install(TARGETS pyroutingkit DESTINATION bin)

install(FILES find_chargers.py optimize_bus_distribution.py osrm_server.py parse_gtfs.py sim.py DESTINATION bin)