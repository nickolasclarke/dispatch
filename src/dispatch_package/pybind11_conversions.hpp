#pragma once

#include "identifiers.hpp"
#include "units.hpp"

//Reference: https://pybind11.readthedocs.io/en/stable/advanced/cast/custom.html

#define CastTSV(XXX)                                                                          \
    template <> struct type_caster<XXX> {                                                     \
    public:                                                                                   \
        PYBIND11_TYPE_CASTER(XXX, _(#XXX));                                                   \
        bool load(handle src, bool) {                                                         \
            PyObject *source = src.ptr();                                                     \
            PyObject *tmp = PyNumber_Float(source);                                           \
            if (!tmp) return false;                                                           \
            value = XXX(PyFloat_AsDouble(tmp));                                               \
            Py_DECREF(tmp);                                                                   \
            return !PyErr_Occurred();                                                         \
        }                                                                                     \
        static handle cast(XXX src, return_value_policy /* policy */, handle /* parent */) {  \
            return PyFloat_FromDouble(static_cast<XXX::type>(src));                           \
        }                                                                                     \
    };                                                                                        \

#define CastTSI(XXX)                                                                          \
    template <> struct type_caster<XXX> {                                                     \
    public:                                                                                   \
        PYBIND11_TYPE_CASTER(XXX, _(#XXX));                                                   \
        bool load(handle src, bool) {                                                         \
            PyObject *source = src.ptr();                                                     \
            PyObject *tmp = PyNumber_Long(source);                                            \
            if (!tmp) return false;                                                           \
            value = XXX(PyLong_AsLong(tmp));                                                  \
            Py_DECREF(tmp);                                                                   \
            return !PyErr_Occurred();                                                         \
        }                                                                                     \
        static handle cast(XXX src, return_value_policy /* policy */, handle /* parent */) {  \
            return PyLong_FromLong(static_cast<XXX::type>(src));                              \
        }                                                                                     \
    };                                                                                        \


namespace pybind11 { namespace detail {
    CastTSV(kilowatt_hours)
    CastTSV(kWh_per_km)
    CastTSV(seconds)
    CastTSV(meters)
    CastTSV(dollars)
    CastTSV(kilowatts)
    CastTSI(stop_id_t)
    CastTSI(block_id_t)
    CastTSI(depot_id_t)
}}
