#include <routingkit/osm_simple.h>
#include <routingkit/contraction_hierarchy.h>
#include <routingkit/geo_position_to_node.h>
#include <routingkit/inverse_vector.h>
#include <limits>
#include <iostream>
#include <stdexcept>
#include <string>

#include "routingkit.hpp"

typedef RoutingKit::ContractionHierarchyQuery route_t;

namespace rk = RoutingKit;



int main(int argc, char **argv){
  if(argc!=3){
    std::cout<<"Syntax: "<<argv[0]<<" <Input File .osm.pbf> <Output File>"<<std::endl;
    return -1;
  }

  Router router(argv[1]);
  router.save_ch(argv[2]);
}
