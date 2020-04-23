#include <algorithm>

#include "data_frames.hpp"
#include "data_structures.hpp"
#include "routingkit.hpp"

class ModelInfo {
 private:
  static trips_t load_and_sort_trips(const std::string &trips_csv);

 public:
  Parameters params;
  const trips_t trips_template;
  const stops_t stops;

  ModelInfo(
    const Parameters &params,
    const std::string &trips_csv,
    const std::string &stops_csv
  ) : params(params),
      trips_template(load_and_sort_trips(trips_csv)),
      stops(csv_stops_to_internal(stops_csv))
  {}

  void update_params(const Parameters &new_params);
};



//Returns a vector of DepotID, Travel time (s), Travel distane (m)
ClosestDepotInfo GetClosestDepot(
  const Router &router,
  const std::vector<double> &stop_lat,
  const std::vector<double> &stop_lon,
  const std::vector<double> &depot_lat,
  const std::vector<double> &depot_lon,
  const double search_radius_m
);



std::unordered_map<depot_id_t, int> count_buses(const trips_t &trips);

void run_block(
  const ModelInfo &model_info,
  trips_t::iterator block_start,
  trips_t::iterator block_end,
  int32_t &next_bus_id
);

trips_t run_model(const ModelInfo &model_info);