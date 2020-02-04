#pragma once

#include <routingkit/osm_simple.h>
#include <routingkit/contraction_hierarchy.h>
#include <routingkit/geo_position_to_node.h>

class Router {
 private:
  RoutingKit::ContractionHierarchy ch;
  RoutingKit::GeoPositionToNode map_geo_position;
  RoutingKit::SimpleOSMCarRoutingGraph graph;

 public:
  Router(const std::string &pbf_filename);

  //Returns the travel time in seconds
  unsigned getTravelTime(const float from_lat, const float from_lon, const float to_lat, const float to_lon, const int search_radius_m) const;
};
