import pickle
import sys
import itertools

import contextily        as ctx
import numpy             as np
import pandas            as pd
import shapely           as shp
import geopandas         as gpd
import partridge         as ptg
import matplotlib.pyplot as plt

transit_sys = ['uta','mn','vegas','ac']
sys_names   = {'uta'  : 'Utah Transit Authority',
               'mn'   : 'Minneapolis Metro Transit',
               'vegas': 'Las Vegas Regional Transit Commission',
               'ac'   : 'Alameda-Contra Costa Transit District'
               }
# A pickle of dfs that have been parsed and used in the sim
feeds       = {key:pd.read_pickle(f'../data/{key}.pickle')      for key in transit_sys}
# The GTFS feeds, loaded as gdfs
geo_feeds   = {key:ptg.load_geo_feed(f'../data/gtfs_{key}.zip') for key in transit_sys}
# the DF of results from the sim
bus_swaps   = {key:pd.read_pickle(f'../data/{key}.results')     for key in transit_sys}

def print_results(key):
  '''Print the BEV to ICE buses required and the resulting ratio.
  '''
  print(f'{key}:\n'
        f'    BEV Buses: {sim_bus[key]}\n'
        f'    ICE Buses: {min_bus[key]}\n'
        f'BEV/ICE ratio: {ratio  [key]}\n'
      )

def filter_results(tid):
  ''' get the filtered gdfs of all various results we want to plot
  '''
  #TODO THIS IS HACKKKK, refactor to be cleaner...if possible.
  gdf = geo_feeds[tid]
  filtered = {}
  # all termini stop_ids
  filtered['all_termini']    = gdf.stops[gdf.stops['stop_id'].isin(feeds[tid]['stops']['stop_id'][feeds[tid]['stops']['evse'] == True ])]
  # All termini where a swap occurs
  filtered['swap_termini']   = gdf.stops[gdf.stops['stop_id'].isin(bus_swaps[tid]['stop_id'])]
  # block_ids that had a replacement swap
  filtered['swap_blocks']  = bus_swaps[tid][bus_swaps[tid]['replacement'] == True]
  # use the above block_ids to filter for any trips that were on a block that 
  # experienced a replacement swap, use that to extract their route geometry
  filtered['swap_routes'] = gdf.trips[gdf.trips['block_id'].isin( filtered['swap_blocks']['block_id'])]['shape_id'].unique()
  filtered['swap_shapes'] = gdf.shapes[gdf.shapes['shape_id'].isin( filtered['swap_routes'])]
  return filtered

def plot_charging(tid, results):
  '''TODO
    '''
  gdf = geo_feeds[tid]
  all_termini  = results['all_termini']
  swap_termini = results['swap_termini']
  swap_shapes  = results['swap_shapes']

  fig, ax = plt.subplots(figsize=(20,20), dpi=350)

  ax.set_aspect('equal')
  #TODO moving reprojection above causes zoom at continental scale
  gdf.shapes.to_crs(epsg=3857).plot(   ax=ax,lw=0.3,
                                        color='k',
                                        zorder=1,
                                        label='Bus Routes'
                                        )
  swap_shapes.to_crs(epsg=3857).plot(  ax=ax,
                                        lw=1,
                                        color='r',
                                        zorder=2,
                                        label ='Swapped Bus Routes'
                                        )
  all_termini.to_crs(epsg=3857).plot(  ax=ax,
                                        zorder=3,
                                        label ='Bus Termini'
                                        )
  swap_termini.to_crs(epsg=3857).plot( ax=ax,
                                        marker='o',
                                        color='#ef8a62',
                                        markersize=5, 
                                        zorder=4,
                                        label ='Swapped Termini'
                                        )
  #pull tiles to add a basemap
  ctx.add_basemap(ax, url=ctx.sources.ST_TERRAIN_BACKGROUND)

  ax.set_title(f'{sys_names[tid]}')
  # ax.set_xlabel("Longitude")
  # ax.set_ylabel("Latitude")
  plt.legend()
  plt.axis('off')
  plt.savefig(f'{tid}.png',dpi=350)
  plt.show()
  

#min buses required to meet service level, regardless of range
min_bus = {key:feeds[key]['trips'].groupby(['block_id']).ngroups for key in transit_sys}
#total buses created by sim
sim_bus = {key:bus_swaps[key].shape[0] for key in transit_sys}
ratio   = {key:sim_bus[key] / min_bus[key] for key in transit_sys}

#filter all the results, mostly to extract geometries
filtered = {tid: filter_results(tid) for tid in transit_sys}

#print the quantitative results
[print_results(key) for key in transit_sys]
# plot the charging
[plot_charging(tid,filtered[tid]) for tid in transit_sys]

# bus_swaps['ac'].shape
# #feeds['ac']['trips'].shape
# #feeds['ac']['trips'][feeds['ac']['trips']['block_id'].isin(bus_swaps['ac']['block_id'])].shape
# feeds['ac']['trips'][['block_id']].isin(bus_swaps['ac']['block_id'])
# #feeds['ac']['trips'][['block_id','start_route_id']].groupby('block_id')['start_route_id']
