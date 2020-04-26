#!/bin/bash
# A script to run Dispatch scenarios
SCENARIOS=$1
SIM_LOC=$2
PARSED_GTFS_PREFIX=$3
OSM_DATA=$4
DEPOTS_FILENAME=$5
OUTPUT_FILENAME='sim.out'
OLDIFS=$IFS
IFS=','
#TODO add error handling for all inputs
[ ! -f $INPUT ] && { echo "$SCENARIOS file not found"; exit 99; }
i=0
while read battery_kwh street_charger_kw
do
  ((i=i+1))
  #TODO unclear how to make this work with IFS=','. Use a space delimiter instead.
    # what is best pratice here?
  SCEN=\"${battery_kwh}\ ${street_charger_kw}\"
  echo "Scenario $i:"
  echo "  Battery: $battery_kwh kWh"
  echo "  Street Charger: $street_charger_kw kW"
  echo ""
  echo $SIM_LOC $SCEN $PARSED_GTFS_PREFIX $OSM_DATA $DEPOTS_FILENAME $OUTPUT_FILENAME

  python $SIM_LOC $SCEN $PARSED_GTFS_PREFIX $OSM_DATA $DEPOTS_FILENAME $OUTPUT_FILENAME
done < $SCENARIOS
IFS=$OLDIFS
