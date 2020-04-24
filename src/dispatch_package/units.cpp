#include "units.hpp"

kilowatt_hours operator*(const meters &meters_in, const kWh_per_km &kwh_per_km_in){
    const auto kilometers = static_cast<meters::type>(meters_in)/1000;
    const auto kwh_per_km = static_cast<kWh_per_km::type>(kwh_per_km_in);
    return kilowatt_hours(kilometers/kwh_per_km);
}