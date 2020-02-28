# Load the module and generate the functions
module RoutingKit
  using CxxWrap
  @wrapmodule(joinpath(".","libjlroutingkit"))

  function __init__()
    @initcxx
  end
end