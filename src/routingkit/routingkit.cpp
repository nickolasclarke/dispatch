#include <routingkit/osm_simple.h>
#include <routingkit/contraction_hierarchy.h>
#include <routingkit/geo_position_to_node.h>
#include <routingkit/inverse_vector.h>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "routingkit.hpp"

typedef RoutingKit::ContractionHierarchyQuery route_t;

namespace rk = RoutingKit;



Router::Router(const std::string &pbf_filename){
  // Load a car routing graph from OpenStreetMap-based data
  graph = rk::simple_load_osm_car_routing_graph_from_pbf(pbf_filename);
  auto tail = rk::invert_inverse_vector(graph.first_out);

  // Build the shortest path index
  ch = rk::ContractionHierarchy::build(
      graph.node_count(), 
      tail, graph.head, 
      graph.travel_time
  );

  map_geo_position = rk::GeoPositionToNode(graph.latitude, graph.longitude);
}



//Returns <travel time (s), travel distance (m)>
std::vector<double> Router::getTravelTime(const double from_lat, const double from_lon, const double to_lat, const double to_lon, const int search_radius_m) const {
  const auto from = map_geo_position.find_nearest_neighbor_within_radius(from_lat, from_lon, search_radius_m).id;
  const auto to   = map_geo_position.find_nearest_neighbor_within_radius(to_lat, to_lon, search_radius_m).id;

  if(from==rk::invalid_id || to==rk::invalid_id)
    throw std::runtime_error("No node near target position.");

  // Besides the CH itself we need a query object. 
  rk::ContractionHierarchyQuery ch_query(ch);
  ch_query.reset().add_source(from).add_target(to).run();

  double travel_time = ch_query.get_distance()/1000.0;

  double distance = 0;
  for(const auto &x: ch_query.get_arc_path())
    distance += graph.geo_distance[x];

  return std::vector<double>{{travel_time,distance}};
}
