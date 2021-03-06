#cmake_minimum_required(VERSION 3.9)
project(icaruswf CXX)
enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(Boost_NO_BOOST_CMAKE OFF)
#set(Boost_USE_STATIC_LIBS        ON)

set(BOOST_ROOT $ENV{BOOST_DIR} )
set(BOOST_INCLUDEDIR $ENV{BOOST_INC} )
set(BOOST_LIBRARYDIR $ENV{BOOST_LIB} )
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

link_directories($ENV{ROOTSYS}/lib $ENV{BOOST_LIB} $ENV{ARTDAQ_CORE_LIB} $ENV{CETLIB_LIB} $ENV{CETLIB_EXCEPT_LIB} $ENV{LARDATAOBJ_LIB}
$ENV{LARCOREOBJ_LIB})
#$ENV{ROOT_LIB}) 

add_executable(test_hit test_hit.cc main.cc)
target_link_libraries(test_hit Core cetlib_except cetlib larcoreobj_SummaryData lardataobj_RecoBase hepnos Boost::serialization)
add_test(test_hit test_hit)

add_executable(test_artdaq_fragment test_artdaq_fragment.cc main.cc)
target_link_libraries(test_artdaq_fragment cetlib_except cetlib Boost::serialization artdaq-core_Core
artdaq-core_Data)
add_test(test_artdaq_fragment test_artdaq_fragment) 

add_executable(test_rawdigit test_rawdigit.cc main.cc)
target_link_libraries(test_rawdigit Core cetlib_except cetlib Boost::serialization lardataobj_RawData
larcoreobj_SummaryData)
add_test(test_rawdigit test_rawdigit) 

add_executable(test_wire test_wire.cc main.cc)
target_link_libraries(test_wire Core cetlib_except cetlib larcoreobj_SummaryData lardataobj_RecoBase hepnos Boost::serialization)
add_test(test_wire test_wire)

add_executable(test_spacepoint test_spacepoint.cc main.cc)
target_link_libraries(test_spacepoint Core cetlib_except cetlib larcoreobj_SummaryData lardataobj_RecoBase hepnos Boost::serialization)
add_test(test_spacepoint test_spacepoint)
