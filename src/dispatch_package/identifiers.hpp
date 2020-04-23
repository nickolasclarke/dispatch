#pragma once

#include "TypeSafeIdentifiers.hpp"

struct StopIDTag{};
struct DepotIDTag{};
struct BlockIDTag{};

using depot_id_t = TSI<DepotIDTag>;
using block_id_t = TSI<BlockIDTag>;
using stop_id_t  = TSI<StopIDTag>;