#include <jlcxx/jlcxx.hpp>
#include "routingkit.hpp"

JLCXX_MODULE define_julia_module(jlcxx::Module& mod)
{
  mod.add_type<Router>("Router")
    .constructor<const std::string&>()
    .constructor<const std::string &, const std::string &>()
    .method("getTravelTime", &Router::getTravelTime)
    .method("getNearestNode", &Router::getNearestNode)
    .method("save_ch", &Router::save_ch);
}
