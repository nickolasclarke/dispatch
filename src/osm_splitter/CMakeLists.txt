cmake_minimum_required(VERSION 3.10)

if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

add_executable(osm_splitter bboxSplit.cpp)
install(TARGETS routing_preprocess RUNTIME DESTINATION DESTINATION bin)
