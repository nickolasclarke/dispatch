#include "units.hpp"

kilowatt_hours operator*(const meters &meters_in, const kWh_per_km &kwh_per_km_in){
    const auto dkilometers = static_cast<meters::type>(meters_in)/1000;
    const auto dkwh_per_km = static_cast<kWh_per_km::type>(kwh_per_km_in);
    return kilowatt_hours(dkilometers/dkwh_per_km);
}

kilowatt_hours operator*(const seconds &seconds_in, const kilowatts &kilowatts_in){
    const auto dhours     = static_cast<seconds::type>(seconds_in)/3600;
    const auto dkilowatts = static_cast<kilowatts::type>(kilowatts_in);
    return kilowatt_hours(dkilowatts*dhours);
}

seconds operator/(const kilowatt_hours &kilowatt_hours_in, const kilowatts &kilowatts_in){
    const auto dkilowatt_hrs = static_cast<kilowatt_hours::type>(kilowatt_hours_in);
    const auto dkilowatts    = static_cast<kilowatts::type>(kilowatts_in);
    const auto hours         = dkilowatt_hrs/dkilowatts;
    return seconds(3600*hours);
}

dollars operator*(const int &int_value, const dollars &dollars_in){
    const auto ddollars = static_cast<dollars::type>(dollars_in);
    return dollars(int_value*ddollars);
}

dollars operator*(const kilowatt_hours &kilowatt_hours_in, const dollars_per_kwh &dollars_per_kwh_in){
    const auto dkilowatt_hours = static_cast<kilowatt_hours::type>(kilowatt_hours_in);
    const auto ddollars_per_kwh = static_cast<dollars_per_kwh::type>(dollars_per_kwh_in);
    return dollars(dkilowatt_hours*ddollars_per_kwh);
}