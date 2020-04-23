#pragma once

#include "data_structures.hpp"

#include <string>

stops_t csv_stops_to_internal(const std::string &inpstr);
trips_t csv_trips_to_internal(const std::string &inpstr);