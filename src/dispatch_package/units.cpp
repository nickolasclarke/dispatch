#include "units.hpp"

kilowatt_hours operator*(const meters &meters_in, const kWh_per_km &kwh_per_km){
    return kilowatt_hours::make(meters_in.raw()/1000*kwh_per_km.raw());
}