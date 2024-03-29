cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
# add our cmake module directory to the path
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
     "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
project(icaruswf LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(USE_GNI_TRANSPORT "use Cray's uGNI transport backend for libfabric" OFF)
if(USE_GNI_TRANSPORT)
  set(MERCURY_TRANSPORT_PROTOCOL "ofi+gni")
  message(STATUS "Using ofi+gni as the mercury protocol")
else()
  set(MERCURY_TRANSPORT_PROTOCOL "ofi+tcp")
  message(STATUS "Using ofi+tcp as the mercury protocol")
endif()

find_package(MPI REQUIRED COMPONENTS C)
find_package(Boost REQUIRED COMPONENTS system serialization)
find_package(hepnos REQUIRED)
find_package(ROOT REQUIRED)
find_package(TBB REQUIRED)
find_package(art REQUIRED)
find_package(canvas REQUIRED)
find_package(hep_concurrency REQUIRED)
find_package(larcoreobj REQUIRED)
find_package(lardataobj REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)

add_subdirectory(src)
add_subdirectory(fcl)

enable_testing()
add_subdirectory(test)
