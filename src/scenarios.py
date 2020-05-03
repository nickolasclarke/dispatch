import argparse
import csv
import yaml

import numpy as np
from itertools import product

from sim import simulate as sim

#TODO add charger_density once supported in sim.py
#TODO include support for depot_chargers. Not sure the best way to scale this
# to the number of actual chargers that would be present at a depot.
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

#TODO move to np_financial or otherwise to implement PMT()
def genAnnualizedCosts(bat_range,non_depot_range,rate,years,bus_base_price,
                       bprice_kwh,cprice_bos=265,cprice_scaled=511,
                       bos_coeff=0.8,scaled_coeff=0.2
                      ):
  """Generates a vector of annualized prices for given ranges. Assumes costs scale
  linearly.

    Keyword Arguments: 
           bat_range -- np.array() of battery capacities (kWh)
    none_depot_range -- np.array() of non-depot charger power (kW)
                rate -- interest rate for PMT calculation
               years -- number of periods for PMT (years) 
      bus_base_price -- floor price of a bus without any battery capacity (currency)
          bprice_kwh -- Battery price per kWh, scales linearly
          cprice_bos -- Estimated "balance of system" EVSE price/kW, scales 
                        linearly. Default:$265/kW, see notes
       cprice_scaled -- Estimated EVSE price/kW, derived from average costs,
                        scales linearly. Default:$511/kW, see notes
           bos_coeff -- weight to give to BOS EVSE price. Default: 0.8, see notes.
        scaled_coeff -- weight to give to scaled avg EVSE price. Default: 0.2, 
                        see notes.

  --- NOTES: ---
  This is derived from the work of M. McCall et al in https://doi.org/10.1088/1748-9326/ab560d
  """
  bat_pv            = bat_range * bprice_kwh
  charger_pv_bos    = non_depot_range * cprice_bos
  charger_pv_scaled = non_depot_range * cprice_scaled

  annualized_base_bus        = -np.pmt(rate,years,bus_base_price)
  annualized_bat             = -np.pmt(rate,years / 2,bat_pv) * 2# assumes a mid-life battery replacement.
  annualized_charge_bos      = -np.pmt(rate,years,charger_pv_bos)
  annualized_charge_scaled   = -np.pmt(rate,years,charger_pv_scaled)

  anl_charge_weighted = (annualized_base_bus * bos_coeff) + (annualized_charge_scaled * scaled_coeff) 
  results = {'annualized_base_bus':annualized_base_bus,
             'annualized_bat':dict(zip(bat_range,annualized_bat)),
             'annualized_charger':dict(zip(non_depot_range,anl_charge_weighted)),
            }
  return results

def buildArgs():
  parser = argparse.ArgumentParser(description='TODO')
  parser.add_argument('parsed_gtfs_prefix',   type=str, help='TODO')
  parser.add_argument('osm_data',             type=str, help='TODO')
  parser.add_argument('depots_filename',      type=str, help='TODO')
  parser.add_argument('output_dir',           type=str, help='TODO')
  parser.add_argument('battery_cap_kwh',      type=str, help='TODO')
  parser.add_argument('nondepot_charger_rate',type=str, help='TODO')
  parser.add_argument('--sim-parameters',     type=str, help='TODO')
  #TODO add charger_density when supported by sim.py
  # parser.add_argument('-s','--save_csv',nargs='?',const='scenarios_results',type=str,help='TODO')
  args = parser.parse_args()
  return args

def main(args=None):
    #use for testing outside of CLI
  if args is None:
    args = buildArgs()
  #extract scenario-related arguments and generate scenario array
  list_args = set(['battery_cap_kwh','nondepot_charger_rate',])
  #extract only the relevant arguments for building scenario matrix
  scen_args = {k:v for (k,v) in vars(args).items() if k in list_args}
  # transform strings to tuples
  scen_args = {key:tuple(map(int,scen_args[key].split(','))) 
              for key,val in scen_args.items()}
  scenarios = genScenarios(**scen_args)
  b_caps, cpowers = list(zip(*scenarios.values()))
  an_costs = genAnnualizedCosts(b_caps,cpowers,0.07,14,500_000,100)
  params = {}
  #TODO Note that this will overwrite arguments passed at CLI, and annualized costs.
  # It may be best to just move all this out of CLI and into params exclusively?
  if args.sim_parameters is not None:
    with open(args.sim_parameters, 'r') as f:
      params = yaml.full_load(f)
  scen_costs = []
  for key,val in scenarios.items():
    bat_cap = val[0]
    cpower = val[1]
    #set up scenario parameters
    params['battery_cap_kwh'] = bat_cap
    params['nondepot_charger_rate'] = cpower
    params['bus_cost'] = an_costs['annualized_base_bus']
    params['battery_cost_per_kwh'] = an_costs['annualized_bat'][bat_cap]
    params['nondepot_charger_cost'] = an_costs['annualized_charger'][cpower]


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
