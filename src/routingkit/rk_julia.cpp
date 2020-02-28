#include <jlcxx/jlcxx.hpp>
#include "routingkit.hpp"

JLCXX_MODULE define_julia_module(jlcxx::Module& mod)
{
  mod.add_type<Router>("Router")
    .constructor<const std::string&>()
    .method("getTravelTime", &Router::getTravelTime);
}
