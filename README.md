# Dispatch

A Salabim-based model for simulating deployment of EV bus fleets, using GTFS data


Data
=============

Acquire OpenStreetMap road network data from: https://www.nextzen.org/metro-extracts/index.html


OSRM
=============

Building
-------------

See: https://github.com/Project-OSRM/osrm-backend/wiki/Building-on-Ubuntu

Install prerequisites:

    sudo apt install libboost-filesystem-dev libboost-iostreams-dev libboost-regex-dev libboost-test-dev libtbb-dev liblua5.3-dev ninja-build

Build OSRM

    cd submodules/osrm-backend
    mkdir build
    cd build
    cmake -GNinja -DCMAKE_BUILD_TYPE=Release ..
    ninja -k0 -j 4
    sudo ninja install

Processing Data
---------------

    #Requires about 1GB RAM for Minneapolis
    osrm-extract -t 3 -p /usr/local/share/osrm/profiles/car.lua minneapolis-saint-paul_minnesota.osm.pbf 
    osrm-contract -t 3 minneapolis-saint-paul_minnesota.osrm
    osrm-routed minneapolis-saint-paul_minnesota.osrm
