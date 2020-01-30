# Dispatch

A Salabim-based model for simulating deployment of EV bus fleets, using GTFS data

Installation/Compilation
===========================

    mkdir build
    cd build
    cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=`pwd` ..
    ninja
    ninja install

Example Usage
===========================

    ./parse_gtfs.py data/gtfs_minneapolis.zip msp3.pickle
    ./find_chargers.py msp3.pickle
    ./sim.py msp3.pickle

Data Acquisition
===========================

Acquire GTFS transit feed data from "https://transitfeeds.com".

