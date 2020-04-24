#pragma once

#include <cmath>
#include <limits>

#include <type_safe/strong_typedef.hpp>

template<class Tag, class T>
struct TSV : type_safe::strong_typedef<TSV<Tag, T>, T>,
                    type_safe::strong_typedef_op::equality_comparison<TSV<Tag, T>>,
                    type_safe::strong_typedef_op::output_operator<TSV<Tag, T>>,
                    type_safe::strong_typedef_op::floating_point_arithmetic<TSV<Tag, T>>,
                    type_safe::strong_typedef_op::relational_comparison<TSV<Tag,T>>
{
    using type_safe::strong_typedef<TSV<Tag, T>, T>::strong_typedef;
    typedef T type;
    static constexpr TSV<Tag, T> invalid() { return TSV<Tag,T>(std::numeric_limits<T>::quiet_NaN()); }
    bool is_valid()   const { return !std::isnan(*this); }
    bool is_invalid() const { return  std::isnan(*this); }
};

struct KilometersTag{};
struct MetersTag{};

struct SecondsTag{};
struct HoursTag{};

struct Kilowatts{};
struct KilowattHoursTag{};
struct KilowattHoursPerKilometerTag{};

struct DollarsTag{};

using kilometers     = TSV<KilometersTag, double>;
using meters         = TSV<MetersTag, double>;
using seconds        = TSV<SecondsTag, double>;
using hours          = TSV<HoursTag, double>;
using kilowatts      = TSV<Kilowatts, double>;
using kilowatt_hours = TSV<KilowattHoursTag, double>;
using kWh_per_km     = TSV<KilowattHoursPerKilometerTag, double>;
using dollars        = TSV<DollarsTag, double>;

constexpr kilometers     operator"" _km         ( long double x ) { return kilometers(x); }
constexpr meters         operator"" _m          ( long double x ) { return meters(x); }
constexpr seconds        operator"" _s          ( long double x ) { return seconds(x); }
constexpr hours          operator"" _hr         ( long double x ) { return hours(x); }
constexpr kilowatts      operator"" _kW         ( long double x ) { return kilowatts(x); }
constexpr kilowatt_hours operator"" _kWh        ( long double x ) { return kilowatt_hours(x); }
constexpr kWh_per_km     operator"" _kWh_per_km ( long double x ) { return kWh_per_km(x); }
constexpr dollars        operator"" _dollars    ( long double x ) { return dollars(x); }

kilowatt_hours operator*(const meters &meters_in, const kWh_per_km &kwh_per_km);
kilowatt_hours operator*(const seconds &seconds_in, const kilowatts &kilowatts_in);