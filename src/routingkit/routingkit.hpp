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

  //Return the id of the nearest node to a lat,lon point
  unsigned getNearestNode(const float lat, const float lon, const int search_radius_m);

  //Returns <travel time (s), travel distance (m)>
  std::vector<double> getTravelTime(const double from_lat, const double from_lon, const double to_lat, const double to_lon, const int search_radius_m) const;
};
