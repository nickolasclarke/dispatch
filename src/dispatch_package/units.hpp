#pragma once

#include "TypeSafeValues.hpp"

struct KilometersTag{};
struct MetersTag{};

struct SecondsTag{};
struct HoursTag{};

struct Kilowatts{};
struct KilowattHoursTag{};
struct KilowattHoursPerKilometerTag{};

struct DollarsTag{};

using kilometers     = TSV<KilometersTag>;
using meters         = TSV<MetersTag>;
using seconds        = TSV<SecondsTag>;
using hours          = TSV<HoursTag>;
using kilowatts      = TSV<Kilowatts>;
using kilowatt_hours = TSV<KilowattHoursTag>;
using kWh_per_km     = TSV<KilowattHoursPerKilometerTag>;
using dollars        = TSV<DollarsTag>;

constexpr kilometers     operator"" _km         ( long double x ) { return kilometers::make(x); }
constexpr meters         operator"" _m          ( long double x ) { return meters::make(x); }
constexpr seconds        operator"" _s          ( long double x ) { return seconds::make(x); }
constexpr hours          operator"" _hr         ( long double x ) { return hours::make(x); }
constexpr kilowatts      operator"" _kW         ( long double x ) { return kilowatts::make(x); }
constexpr kilowatt_hours operator"" _kWh        ( long double x ) { return kilowatt_hours::make(x); }
constexpr kWh_per_km     operator"" _kWh_per_km ( long double x ) { return kWh_per_km::make(x); }
constexpr dollars        operator"" _dollars    ( long double x ) { return dollars::make(x); }

kilowatt_hours operator*(const meters &meters_in, const kWh_per_km &kwh_per_km);