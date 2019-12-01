#!/usr/bin/env python3
import copy
import heapq
import sys

import numpy as np
import pandas as pd
import geopandas as gpd


if len(sys.argv)!=2:
  print("Syntax: {0} <Model Output>".format(sys.argv[0]))
  sys.exit(-1)

model_out_filename = sys.argv[1]
bus_swaps          = pd.read_pickle(model_out_filename, compression='infer')


charge_time=200/500 #Capacity/Rate
q = []
for idx,row in bus_swaps.iterrows():
    heapq.heappush(q, (row['datetime'], 'swap'))
    heapq.heappush(q, (row['datetime']+charge_time, 'free'))

maxbuses = 0
buses = 0
while len(q)>0:
    event = heapq.heappop(q)
    print(event)
    if event[1]=='swap':
        buses -= 1
    elif event[1]=='free':
        buses += 1
    maxbuses = min(buses,maxbuses)

print(maxbuses)

#bus_info = pd.read_pickle('msp3.pickle', compression='infer')
