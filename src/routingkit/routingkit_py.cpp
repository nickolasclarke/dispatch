#include <pybind11/pybind11.h>
#include <routingkit/osm_simple.h>
#include <routingkit/contraction_hierarchy.h>
#include <routingkit/geo_position_to_node.h>
#include <routingkit/inverse_vector.h>
#include <iostream>
#include <limits>
#include <stdexcept>

typedef RoutingKit::ContractionHierarchyQuery route_t;

namespace py = pybind11;
namespace rk = RoutingKit;



class Router {
 private:
  RoutingKit::ContractionHierarchy ch;
  RoutingKit::GeoPositionToNode map_geo_position;
  RoutingKit::SimpleOSMCarRoutingGraph graph;

 public:
  Router(const std::string &pbf_filename){
    // Load a car routing graph from OpenStreetMap-based data
    graph = rk::simple_load_osm_car_routing_graph_from_pbf(pbf_filename);
    auto tail  = rk::invert_inverse_vector(graph.first_out);

    // Build the shortest path index
    ch = rk::ContractionHierarchy::build(
        graph.node_count(), 
        tail, graph.head, 
        graph.travel_time
    );

    map_geo_position = rk::GeoPositionToNode(graph.latitude, graph.longitude);
  }

  //Returns the travel time in seconds
  auto getTravelTime(const float from_lat, const float from_lon, const float to_lat, const float to_lon, const int search_radius_m) const {
    const auto from = map_geo_position.find_nearest_neighbor_within_radius(from_lat, from_lon, search_radius_m).id;
    const auto to   = map_geo_position.find_nearest_neighbor_within_radius(to_lat, to_lon, search_radius_m).id;

    if(from==rk::invalid_id || to==rk::invalid_id)
      throw std::runtime_error("No node near target position.");

    // Besides the CH itself we need a query object. 
    rk::ContractionHierarchyQuery ch_query(ch);
    ch_query.reset().add_source(from).add_target(to).run();

    //Use ch_query.get_node_path() to get the path

    return ch_query.get_distance()/1000;
  }
};



PYBIND11_MODULE(pyroutingkit, m) {
  m.doc() = "RoutingKit interface"; // optional module docstring

  py::class_<Router>(m, "Router")
    .def(py::init<const std::string &>())
    .def("getTravelTime", &Router::getTravelTime);
}
