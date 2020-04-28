import argparse
import csv

import numpy as np
from itertools import product

from sim import simulate as sim

#TODO add charger_density once supported in sim.py
def genScenarios(battery_cap_kwh,nondepot_charger_rate,): #charger_density):
    '''Generates a list of tuples of scenarios
        args: all tuples of a start (inclusive), end (exclusive), and step 
        size values. i.e. battery_cap_kwh=(125,700,100) 
    '''
    #TODO include error handling
    bat_range = np.arange(*battery_cap_kwh)
    non_depot_range = np.arange(*nondepot_charger_rate)
    #charger_density = np.arange(*charger_density)

    scen_opts = [bat_range,non_depot_range,]# charger_density]
    scens = list(product(*scen_opts))
    return scens

def main():
  parser = argparse.ArgumentParser(description='TODO')
  parser.add_argument('parsed_gtfs_prefix', type=str, help='TODO')
  parser.add_argument('osm_data',           type=str, help='TODO')
  parser.add_argument('depots_filename',    type=str, help='TODO')
  parser.add_argument('output_filename',    type=str, help='TODO')
  parser.add_argument('-b','--battery-cap-kwh',      nargs=3,type=int,help='TODO')
  parser.add_argument('-c','--nondepot-charger-rate',nargs=3,type=int,help='TODO')
  #TODO add when supported by sim.py
  #parser.add_argument('-d','--charger-density',  nargs=3,type=int,help='TODO')
  parser.add_argument('-s','--save_csv',
    nargs='?',const='scenarios',type=str,help='TODO')
  args = parser.parse_args()

  #extract scenario-related arguments and generate scenario array
  list_args = set(['battery_cap_kwh','nondepot_charger_rate',])#'charger_density'])
  scen_args = {k:v for (k,v) in vars(args).items() if k in list_args}
  scenarios = genScenarios(**scen_args)

  if args.save_csv is not None:
    #TODO sync up with output_filename
    output_name = f'{args.save_csv}_scenarios.csv'
    np.savetxt(output_name,scenarios, header='battery_cap_kwh,nondepot_charger_rate',
      delimiter=',',comments='',fmt='%1.2f')

  for scen in scenarios:
    print(scen)
    params = {'battery_cap_kwh':scen[0], 'nondepot_charger_rate':scen[1]}
    print(params)

    bus_assignments_df, depot_counts = sim(args.parsed_gtfs_prefix, 
                                           args.osm_data,
                                           args.depots_filename,
                                           args.output_filename,
                                           parameters=params)
    # write results to file
    print(bus_assignments_df.head(), depot_counts)
    bus_assignments_df.to_csv(
      f'{args.output_filename}_{scen[0]}kwh_{scen[1]}kw.csv')
    
    print(depot_counts)
    depot_counts = {f'depot_{k}': v for k, v in depot_counts.items()}
    depot_res_name = f'{args.output_filename}_depot_counts.csv'

    with open(depot_res_name, 'w', newline='') as csvfile:
      fieldnames = list(depot_counts.keys())
      writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
      writer.writeheader()
      for key, val in depot_counts.items():
        writer.writerow({key: val})

  return bus_assignments_df, depot_counts

if __name__ == '__main__':
  main()
