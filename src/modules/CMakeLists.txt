configure_file(icaruswf_config.h.in icaruswf_config.h)

add_library(MergeDataProds_module SHARED MergeDataProds_module.cc)
target_link_libraries(MergeDataProds_module PRIVATE Boost::system TBB::tbb)
target_include_directories(
  MergeDataProds_module
  PRIVATE $ENV{ROOT_INC}
          $ENV{ART_INC}
          $ENV{CANVAS_INC}
          $ENV{CETLIB_EXCEPT_INC}
          $ENV{CETLIB_INC}
          $ENV{FHICLCPP_INC}
          $ENV{HEP_CONCURRENCY_INC}
          $ENV{LARCOREOBJ_INC}
          $ENV{LARDATAOBJ_INC})

add_library(HepnosInput_source SHARED HepnosInput_source.cc)
target_link_libraries(
  HepnosInput_source
  PRIVATE Boost::system hepnos Boost::serialization TBB::tbb
          art::Framework_IO_Sources
          $ENV{LARDATAOBJ_LIB}/liblardataobj_RecoBase.so)
target_include_directories(
  HepnosInput_source
  PRIVATE $ENV{ROOT_INC}
          $ENV{ART_INC}
          $ENV{HEPNOS_INC}
          $ENV{LARDATAOBJ_INC}
          $ENV{LARCOREOBJ_INC}
          $ENV{THALLIUM_INC}
          $ENV{CEREAL_INC}
          ${CMAKE_CURRENT_BINARY_DIR})

add_library(HepnosSPOutput_module SHARED HepnosSPOutput_module.cc)
target_link_libraries(
  HepnosSPOutput_module PRIVATE ROOT::Core Boost::system hepnos
                                Boost::serialization TBB::tbb spdlog::spdlog)
target_include_directories(
  HepnosSPOutput_module
  PRIVATE $ENV{ROOT_INC}
          $ENV{ART_INC}
          $ENV{HEPNOS_INC}
          $ENV{CANVAS_INC}
          $ENV{CETLIB_INC}
          $ENV{CETLIB_EXCEPT_INC}
          $ENV{FHICLCPP_INC}
          $ENV{HEP_CONCURRENCY_INC}
          $ENV{LARDATAOBJ_INC}
          $ENV{THALLIUM_INC}
          $ENV{CEREAL_INC}
          $ENV{LARCOREOBJ_INC}
          ${CMAKE_CURRENT_BINARY_DIR})

add_library(HepnosDataStore_service SHARED HepnosDataStore_service.cc)
target_link_libraries(
  HepnosDataStore_service PRIVATE Boost::system hepnos Boost::serialization
                                  TBB::tbb art::Framework_Services_Registry)
target_include_directories(
  HepnosDataStore_service
  PRIVATE $ENV{ROOT_INC}
          $ENV{ART_INC}
          $ENV{HEPNOS_INC}
          $ENV{THALLIUM_INC}
          $ENV{LARDATAOBJ_INC}
          $ENV{CEREAL_INC}
          ${CMAKE_CURRENT_BINARY_DIR})

# TCLAP
find_package(TCLAP REQUIRED)
add_executable(mpi_wrapper mpi_wrapper.cc)
target_link_libraries(mpi_wrapper PRIVATE MPI::MPI_C)
target_include_directories(mpi_wrapper PRIVATE ${TCLAP_INCLUDE_PATH})

add_executable(cheesyQueue_maker cheesyQueue_maker.cc)
target_include_directories(cheesyQueue_maker PRIVATE ${CMAKE_SOURCE_DIR}
                                                     $ENV{HEPNOS_INC})
target_link_libraries(cheesyQueue_maker PRIVATE hepnos)

add_library(LoadbalancingOutput_module SHARED LoadbalancingOutput_module.cc)
target_link_libraries(
  LoadbalancingOutput_module
  PRIVATE ROOT::Core Boost::system hepnos Boost::serialization TBB::tbb
          spdlog::spdlog)
target_include_directories(
  LoadbalancingOutput_module
  PRIVATE $ENV{ROOT_INC}
          $ENV{ART_INC}
          $ENV{HEPNOS_INC}
          $ENV{CANVAS_INC}
          $ENV{CETLIB_INC}
          $ENV{CETLIB_EXCEPT_INC}
          $ENV{FHICLCPP_INC}
          $ENV{HEP_CONCURRENCY_INC}
          $ENV{LARDATAOBJ_INC}
          $ENV{THALLIUM_INC}
          $ENV{CEREAL_INC}
          $ENV{LARCOREOBJ_INC}
          ${CMAKE_CURRENT_BINARY_DIR}
          ${CMAKE_SOURCE_DIR})

add_library(LoadbalancingInput_source SHARED LoadbalancingInput_source.cc)
target_link_libraries(
  LoadbalancingInput_source
  PRIVATE Boost::system hepnos Boost::serialization TBB::tbb
          art::Framework_IO_Sources
          $ENV{LARDATAOBJ_LIB}/liblardataobj_RecoBase.so)
target_include_directories(
  LoadbalancingInput_source
  PRIVATE $ENV{ROOT_INC}
          $ENV{ART_INC}
          $ENV{HEPNOS_INC}
          $ENV{LARDATAOBJ_INC}
          $ENV{THALLIUM_INC}
          $ENV{CEREAL_INC}
          $ENV{LARCOREOBJ_INC}
          ${CMAKE_CURRENT_BINARY_DIR}
          ${CMAKE_SOURCE_DIR})

add_library(Timestamp_service SHARED Timestamp_service.cc)
target_link_libraries(
  Timestamp_service PRIVATE TBB::tbb art::Framework_Services_Registry
                            $ENV{NUMA_LIB}/libnuma.so)
target_include_directories(Timestamp_service
                           PRIVATE $ENV{ART_INC} ${CMAKE_CURRENT_BINARY_DIR})
