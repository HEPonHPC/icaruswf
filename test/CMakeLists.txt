# cmake_minimum_required(VERSION 3.9)
project(icaruswf CXX)
enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(Boost_NO_BOOST_CMAKE OFF) set(Boost_USE_STATIC_LIBS        ON)

set(BOOST_ROOT $ENV{BOOST_DIR})
set(BOOST_INCLUDEDIR $ENV{BOOST_INC})
set(BOOST_LIBRARYDIR $ENV{BOOST_LIB})
set(Boost_USE_MULTITHREADED ON)
set(Boost_NO_SYSTEM_PATHS ON)
option(BUILD_SHARED_LIBS "Build shared libraries for this project." ON)

find_package(Boost REQUIRED COMPONENTS serialization system filesystem)
find_package(hepnos REQUIRED)
include_directories($ENV{ARTDAQ_CORE_INC})
include_directories($ENV{BOOST_INC})
include_directories($ENV{CETLIB_EXCEPT_INC})
include_directories($ENV{CETLIB_INC})
include_directories($ENV{TRACE_INC})
include_directories($ENV{LARDATAOBJ_INC})
include_directories($ENV{LARCOREOBJ_INC})
include_directories($ENV{ROOT_INC})

link_directories(
  $ENV{ROOTSYS}/lib
  $ENV{BOOST_LIB}
  $ENV{ARTDAQ_CORE_LIB}
  $ENV{CETLIB_LIB}
  $ENV{CETLIB_EXCEPT_LIB}
  $ENV{LARDATAOBJ_LIB}
  $ENV{LARCOREOBJ_LIB})
# $ENV{ROOT_LIB})

add_executable(test_hit test_hit.cc main.cc)
target_link_libraries(test_hit Core cetlib_except cetlib lardataobj_RecoBase
                      Boost::serialization)
add_test(test_hit test_hit)

add_executable(test_artdaq_fragment test_artdaq_fragment.cc main.cc)
target_link_libraries(test_artdaq_fragment cetlib_except cetlib
                      Boost::serialization artdaq-core_Core artdaq-core_Data)
add_test(test_artdaq_fragment test_artdaq_fragment)

add_executable(test_rawdigit test_rawdigit.cc main.cc)
target_link_libraries(test_rawdigit Core cetlib_except cetlib
                      Boost::serialization lardataobj_RawData)
add_test(test_rawdigit test_rawdigit)

add_executable(test_sparse_vector test_sparse_vector.cc main.cc)
target_link_libraries(test_sparse_vector Core cetlib_except cetlib
                      lardataobj_RecoBase Boost::serialization)
add_test(test_sparse_vector test_sparse_vector)

add_executable(test_wire test_wire.cc main.cc)
target_link_libraries(test_wire Core cetlib_except cetlib lardataobj_RecoBase
                      Boost::serialization)
add_test(test_wire test_wire)

add_executable(test_spacepoint test_spacepoint.cc main.cc)
target_link_libraries(test_spacepoint Core cetlib_except cetlib
                      lardataobj_RecoBase Boost::serialization)
add_test(test_spacepoint test_spacepoint)

add_executable(test_edge test_edge.cc main.cc)
target_link_libraries(test_edge Core cetlib_except cetlib lardataobj_RecoBase
                      Boost::serialization)
add_test(test_edge test_edge)

add_executable(test_ophit test_ophit.cc main.cc)
target_link_libraries(test_ophit Core cetlib_except cetlib lardataobj_RecoBase
                      Boost::serialization)
add_test(test_ophit test_ophit)

add_executable(test_opflash test_opflash.cc main.cc)
target_link_libraries(test_opflash Core cetlib_except cetlib
                      lardataobj_RecoBase Boost::serialization)
add_test(test_opflash test_opflash)

add_executable(test_opdetwaveform test_opdetwaveform.cc main.cc)
target_link_libraries(test_opdetwaveform Core cetlib_except cetlib
                      lardataobj_RawData Boost::serialization)
add_test(test_opdetwaveform test_opdetwaveform)

add_executable(test_pfparticle test_pfparticle.cc main.cc)
target_link_libraries(test_pfparticle Core cetlib_except cetlib
                      lardataobj_RecoBase Boost::serialization)
add_test(test_pfparticle test_pfparticle)

add_executable(test_seed test_seed.cc main.cc)
target_link_libraries(test_seed Core cetlib_except cetlib Boost::serialization
                      lardataobj_RecoBase)
add_test(test_seed test_seed)

add_executable(test_track test_track.cc main.cc)
target_link_libraries(test_track Core cetlib_except cetlib lardataobj_RecoBase
                      Boost::serialization)
add_test(test_track test_track)

add_executable(test_trackhitmeta test_trackhitmeta.cc main.cc)
target_link_libraries(test_trackhitmeta Core cetlib_except cetlib
                      lardataobj_RecoBase Boost::serialization)
add_test(test_trackhitmeta test_trackhitmeta)

add_executable(test_shower test_shower.cc main.cc)
target_link_libraries(
  test_shower
  Core
  cetlib_except
  cetlib
  lardataobj_RecoBase
  Boost::serialization
  ROOT::Physics)
add_test(test_shower test_shower)

add_executable(test_pcaxis test_pcaxis.cc main.cc)
target_link_libraries(test_pcaxis Core cetlib_except cetlib
                      Boost::serialization lardataobj_RecoBase)
add_test(test_pcaxis test_pcaxis)

add_executable(test_slice test_slice.cc main.cc)
target_link_libraries(test_slice Core cetlib_except cetlib Boost::serialization
                      lardataobj_RecoBase)
add_test(test_slice test_slice)

add_executable(test_vertex test_vertex.cc main.cc)
target_link_libraries(test_vertex Core cetlib_except cetlib
                      Boost::serialization lardataobj_RecoBase)
add_test(test_vertex test_vertex)

add_executable(test_cluster test_cluster.cc main.cc)
target_link_libraries(test_cluster Core cetlib_except cetlib
                      Boost::serialization lardataobj_RecoBase)
add_test(test_cluster test_cluster)

add_executable(test_hepnos test_hepnos.cc)
target_link_libraries(
  test_hepnos
  PRIVATE Boost::system
          hepnos
          Core
          cetlib_except
          cetlib
          Boost::serialization
          lardataobj_RecoBase
          TBB::tbb)
# target_include_directories(test_hepnos PRIVATE  $ENV{ROOT_INC} $ENV{ART_INC}
# $ENV{HEPNOS_INC} $ENV{CANVAS_INC} $ENV{CETLIB_INC} $ENV{CETLIB_EXCEPT_INC}
# $ENV{FHICLCPP_INC} $ENV{HEP_CONCURRENCY_INC} $ENV{LARDATAOBJ_INC}
# $ENV{THALLIUM_INC} $ENV{CEREAL_INC} $ENV{LARCOREOBJ_INC})

# Copy over hepnos.json file
configure_file(hepnos.json.in hepnos.json)
