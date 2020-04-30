import argparse
import csv
import yaml

import numpy as np
from itertools import product

from sim import simulate as sim

#TODO add charger_density once supported in sim.py
def genScenarios(battery_cap_kwh,nondepot_charger_rate,):
    '''Generates a list of tuples of scenarios
        args: all tuples of a start (inclusive), end (exclusive), and step 
        size values. i.e. battery_cap_kwh=(125,700,100) 
    '''
    #TODO include error handling
    bat_range = np.arange(*battery_cap_kwh)
    non_depot_range = np.arange(*nondepot_charger_rate)
    scen_opts = [bat_range,non_depot_range,]
    scens = list(product(*scen_opts))
    keys = list(range(len(scens)))
    scens = dict(zip(keys, scens))
    return scens

def main():
  parser = argparse.ArgumentParser(description='TODO')
  parser.add_argument('parsed_gtfs_prefix', type=str, help='TODO')
  parser.add_argument('osm_data',           type=str, help='TODO')
  parser.add_argument('depots_filename',    type=str, help='TODO')
  parser.add_argument('output_dir',         type=str, help='TODO')
  parser.add_argument('--sim-parameters',   type=str, help='TODO')
  parser.add_argument('-b','--battery-cap-kwh',      nargs=3,type=int,help='TODO')
  parser.add_argument('-c','--nondepot-charger-rate',nargs=3,type=int,help='TODO')
  #TODO add charger_density when supported by sim.py
  # parser.add_argument('-s','--save_csv',nargs='?',const='scenarios_results',type=str,help='TODO')
  args = parser.parse_args()

  #extract scenario-related arguments and generate scenario array
  list_args = set(['battery_cap_kwh','nondepot_charger_rate',])
  scen_args = {k:v for (k,v) in vars(args).items() if k in list_args}
  scenarios = genScenarios(**scen_args)
  scen_costs = []
  params = {}
  if args.sim_parameters is not None:
    with open(args.sim_parameters, 'r') as f:
      params = yaml.full_load(f)

  for key,val in scenarios.items():
    params['battery_cap_kwh'],params['nondepot_charger_rate'] = val[0], val[1]
    results = sim(args.parsed_gtfs_prefix, 
                  args.osm_data,
                  args.depots_filename,
                  parameters=params)
    # parse results and write to file
    scen_costs.append([val[0],val[1],
      results['opti_buses'],results['opti_chargers'],results['opti_cost'],
      results['nc_buses'],results['nc_chargers'],results['nc_cost'],
      results['ac_buses'],results['ac_chargers'],results['ac_cost']]
      )

    prefix = f'{val[0]}kwh_{val[1]}_kw'
    results['opti_trips'].to_csv(f'{args.output_dir}/{prefix}_trips.csv')

    depot_res_name = f'{args.output_dir}/{prefix}_depot_counts.csv'
    with open(depot_res_name,'w',newline='') as csvfile:
      fieldnames = ['depot','bus_count']
      writer = csv.DictWriter(csvfile,fieldnames=fieldnames)
      writer.writeheader()
      #TODO include nc and ac depots as well
      for key, val in results['opti_depot_counts'].items():
        writer.writerow({'depot': key, 'bus_count': val})

  #TODO results in cleaner way to do this?
  results_headers = 'battery_cap_kwh,nondepot_charger_rate,\
optimized_buses,optimized_chargers,optimized_cost,\
nc_buses,nc_chargers,nc_cost,\
ac_buses,ac_chargers,ac_costs'

  np.savetxt(f'{args.output_dir}/scenarios_results.csv',scen_costs,
    header=results_headers,delimiter=',',comments='',fmt='%1.2f')

if __name__ == '__main__':
  main()
