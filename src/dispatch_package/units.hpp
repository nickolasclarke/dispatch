#pragma once

#include "TypeSafeValues.hpp"

struct KilometersTag{};
struct MetersTag{};

struct SecondsTag{};
struct HoursTag{};

struct Kilowatts{};
struct KilowattHoursTag{};
struct KilowattHoursPerKilometerTag{};

using kilometers     = TSI<KilometersTag>;
using meters         = TSI<MetersTag>;
using seconds        = TSI<SecondsTag>;
using hours          = TSI<HoursTag>;
using kilowatts      = TSI<Kilowatts>;
using kilowatt_hours = TSI<KilowattHoursTag>;
using kWh_per_km     = TSI<KilowattHoursPerKilometerTag>;

constexpr kilometers     operator"" _km         ( long double x ) { return kilometers::make(x); }
constexpr meters         operator"" _m          ( long double x ) { return meters::make(x); }
constexpr seconds        operator"" _s          ( long double x ) { return seconds::make(x); }
constexpr hours          operator"" _hr         ( long double x ) { return hours::make(x); }
constexpr kilowatts      operator"" _kW         ( long double x ) { return kilowatts::make(x); }
constexpr kilowatt_hours operator"" _kWh        ( long double x ) { return kilowatt_hours::make(x); }
constexpr kWh_per_km     operator"" _kWh_per_km ( long double x ) { return kWh_per_km::make(x); }

kilowatt_hours operator*(const meters &meters_in, const kWh_per_km &kwh_per_km){
    return kilowatt_hours::make(meters_in.raw()/1000*kwh_per_km.raw());
}