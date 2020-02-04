# Load the module and generate the functions
module RoutingKit
  using CxxWrap
  @wrapmodule(joinpath("/home/rick/projects/RISE_ev_bus/dispatch/build/bin","libjlroutingkit"))

  function __init__()
    @initcxx
  end
end