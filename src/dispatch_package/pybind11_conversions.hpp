#pragma once

#include "units.hpp"

namespace pybind11 { namespace detail {
    template <> struct type_caster<kilowatt_hours> {
    public:
        PYBIND11_TYPE_CASTER(kilowatt_hours, _("kilowatt_hours"));

        bool load(handle src, bool) {
            PyObject *source = src.ptr();
            PyObject *tmp = PyNumber_Float(source);
            if (!tmp) return false;
            value = kilowatt_hours::make(PyFloat_AsDouble(tmp));
            Py_DECREF(tmp);
            return !PyErr_Occurred();
        }

        static handle cast(kilowatt_hours src, return_value_policy /* policy */, handle /* parent */) {
            return PyFloat_FromDouble(src.raw());
        }
    };

    template <> struct type_caster<kWh_per_km> {
    public:
        PYBIND11_TYPE_CASTER(kWh_per_km, _("kWh_per_km"));

        bool load(handle src, bool) {
            PyObject *source = src.ptr();
            PyObject *tmp = PyNumber_Float(source);
            if (!tmp) return false;
            value = kWh_per_km::make(PyFloat_AsDouble(tmp));
            Py_DECREF(tmp);
            return !PyErr_Occurred();
        }

        static handle cast(kWh_per_km src, return_value_policy /* policy */, handle /* parent */) {
            return PyFloat_FromDouble(src.raw());
        }
    };

    template <> struct type_caster<seconds> {
    public:
        PYBIND11_TYPE_CASTER(seconds, _("seconds"));

        bool load(handle src, bool) {
            PyObject *source = src.ptr();
            PyObject *tmp = PyNumber_Float(source);
            if (!tmp) return false;
            value = seconds::make(PyFloat_AsDouble(tmp));
            Py_DECREF(tmp);
            return !PyErr_Occurred();
        }

        static handle cast(seconds src, return_value_policy /* policy */, handle /* parent */) {
            return PyFloat_FromDouble(src.raw());
        }
    };

    template <> struct type_caster<meters> {
    public:
        PYBIND11_TYPE_CASTER(meters, _("meters"));

        bool load(handle src, bool) {
            PyObject *source = src.ptr();
            PyObject *tmp = PyNumber_Float(source);
            if (!tmp) return false;
            value = meters::make(PyFloat_AsDouble(tmp));
            Py_DECREF(tmp);
            return !PyErr_Occurred();
        }

        static handle cast(meters src, return_value_policy /* policy */, handle /* parent */) {
            return PyFloat_FromDouble(src.raw());
        }
    };
}} 