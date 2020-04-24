cmake_minimum_required(VERSION 3.10)

set(PROTOZERO_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/submodules/protozero/include")
set(OSMIUM_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/submodules/libosmium/include")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/submodules/libosmium/cmake/")

set(CMAKE_CXX_FLAGS_RelWithDebInfoAndAssert "-Wall -Wabi -Wextra -O2 -g" CACHE STRING
 "Flags used by the C++ compiler during RelWithDebInfoAndAssert builds."
  FORCE)
set(CMAKE_C_FLAGS_RelWithDebInfoAndAssert "-Wall -Wabi -Wextra -pedantic -O2 -g" CACHE STRING
  "Flags used by the C compiler during RelWithDebInfoAndAssert builds."
  FORCE)
set(CMAKE_EXE_LINKER_FLAGS_RelWithDebInfoAndAssert
  "-Wl,--warn-unresolved-symbols,--warn-once" CACHE STRING
  "Flags used for linking binaries during RelWithDebInfoAndAssert builds."
  FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_RelWithDebInfoAndAssert
  "-Wl,--warn-unresolved-symbols,--warn-once" CACHE STRING
  "Flags used by the shared libraries linker during RelWithDebInfoAndAssert builds."
  FORCE)

mark_as_advanced(
  CMAKE_CXX_FLAGS_RelWithDebInfoAndAssert
  CMAKE_C_FLAGS_RelWithDebInfoAndAssert
  CMAKE_EXE_LINKER_FLAGS_RelWithDebInfoAndAssert
  CMAKE_SHARED_LINKER_FLAGS_RelWithDebInfoAndAssert)
# Update the documentation string of CMAKE_BUILD_TYPE for GUIs
set(CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}" CACHE STRING
  "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel RelWithDebInfoAndAssert."
  FORCE)




if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -pedantic)
endif()

add_subdirectory(submodules)
add_subdirectory(src)
