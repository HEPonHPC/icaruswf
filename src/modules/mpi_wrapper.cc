#include "mpicpp.hpp"
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <tclap/Arg.h>
#include <tclap/CmdLine.h>
#include <unistd.h>

struct Config {
  bool
  valid() const
  {
    // check that only one mode is selected!
    if (use_root == use_hepnos) {
      std::cout << "Please select only one of HEPnOS/ROOT execution modes!"
                << "\n";
      return false;
    }
    if (use_hepnos) {
      // This logic needs to be more robust, the configuration is valid
      // if either load path or process flag is specified not both.
      if (process != loadPath.empty()) {
        std::cout << "Please select only one of load/process execution modes "
                     "with HEPnOS!"
                  << "\n";
        return false;
      }
    }
    return true;
  };
  int64_t nevents{-1};
  int64_t nthreads{-1};
  std::string loadPath;
  bool process{false};
  bool use_root{false};
  bool use_hepnos{false};
};

Config
make_config(int argc, char* argv[])
{
  Config config;
  try {
    TCLAP::CmdLine cmd(
      "ICARUSWF with HEPnOS/ROOT mpi-wrapper for launching art over MPI!");

    TCLAP::ValueArg<int64_t> nevtsArg(
      "n",
      "num_evts_per_rank",
      "total number of events to process, by each MPI rank",
      true,
      0,
      "int64");
    cmd.add(nevtsArg);

    TCLAP::ValueArg<std::string> rootfilepathArg(
      "l",
      "root_file_path",
      "location of the ROOT file to process",
      false,
      "",
      "string");
    cmd.add(rootfilepathArg);

    TCLAP::ValueArg<int64_t> threadsArg(
      "t",
      "threads",
      "number of threads to use for SH processing",
      false,
      1,
      "int64");
    cmd.add(threadsArg);

    TCLAP::SwitchArg hepnosSwitch(
      "H", "hepnos", "Whether to process events using HEPnOS", cmd, false);

    TCLAP::SwitchArg rootSwitch(
      "R", "root", "Whether to process events using ROOT", cmd, false);

    TCLAP::SwitchArg processSwitch(
      "p",
      "process_events",
      "Whether to process events in the HEPnOS datastore",
      cmd,
      false);

    // Parse the argv array.
    cmd.parse(argc, argv);
    config.nevents = nevtsArg.getValue();
    config.loadPath = rootfilepathArg.getValue();
    config.nthreads = threadsArg.getValue();
    config.process = processSwitch.getValue();
    config.use_root = rootSwitch.getValue();
    config.use_hepnos = hepnosSwitch.getValue();
  }
  catch (TCLAP::ArgException& e) // catch exceptions
  {
    std::cerr << "error: " << e.error() << " for arg " << e.argId()
              << std::endl;
  }
  return config;
}

// the fcl file for loading shouldn't be hard coded,
// for now its okay but should be fixed at some point

int
loadData(int my_rank,
         int nevents,
         std::string path,
         std::optional<std::string> env_flags)
{
  std::string nskip = " --nskip ";
  nskip += std::to_string(my_rank * nevents);
  std::string nevts = " -n ";
  nevts += std::to_string(nevents);
  std::string cmd = "";
  if (env_flags.has_value()) {
    cmd.append(env_flags.value());
  }
  cmd.append("art -c storedata_queue.fcl -s ");
  cmd.append(path);
  cmd.append(nevts);
  cmd.append(nskip);
  std::string outfile = " &> ";
  outfile.append(std::to_string(my_rank));
  outfile.append("_load.txt");
  cmd.append(outfile);
  return std::system(cmd.c_str());
}

// this is to run the signal processing and hitfinding steps
// as specified in the fcl file. the name is hard coded here
// as well, may be we want to change it as well
int
runSH(int my_rank,
      int nevents,
      int nthreads,
      std::optional<std::string> env_flags)
{
  std::string nevts = " -n ";
  nevts += std::to_string(nevents);
  std::string memdb =
    " --memcheck-db memory_sp_" + std::to_string(my_rank) + ".db";

  std::string cmd = "";
  if (env_flags.has_value()) {
    cmd.append(env_flags.value());
  }
  cmd.append("art --nschedules 1 --nthreads " + std::to_string(nthreads) +
             " -c icarus_SH_queue.fcl "
             "--timing-db timing_sp_" +
             std::to_string(my_rank) + ".db" + memdb + nevts);
  std::string outfile = " &> ";
  outfile.append(std::to_string(my_rank));
  outfile.append("_process.txt");
  cmd.append(outfile);
  return std::system(cmd.c_str());
}

// the fcl file for loading shouldn't be hard coded,
// for now its okay but should be fixed at some point

int
runSP(int my_rank,
      int nevents,
      int nthreads,
      std::string path,
      std::optional<std::string> env_flags)
{
  std::string nskip = " --nskip ";
  nskip += std::to_string(my_rank * nevents);
  std::string nevts = " -n ";
  nevts += std::to_string(nevents);
  std::string memdb =
    " --memcheck-db memory_sp_" + std::to_string(my_rank) + ".db";
  std::string timedb =
    " --timing-db timing_sp_" + std::to_string(my_rank) + ".db";

  std::string cmd = "";
  if (env_flags.has_value()) {
    cmd.append(env_flags.value());
  }
  cmd.append("art --nschedules 1 --nthreads " + std::to_string(nthreads) +
             nevts + nskip + " -c sh_root.fcl " + timedb + memdb + " -s ");
  cmd.append(path);
  std::string outfile = " &> ";
  outfile.append("sp_log.txt");
  cmd.append(outfile);
  return std::system(cmd.c_str());
}

int
runHF(int my_rank,
      int nevents,
      int nthreads,
      std::optional<std::string> env_flags)
{
  std::string nevts = " -n ";
  nevts += std::to_string(nevents);
  std::string memdb =
    " --memcheck-db memory_hf_" + std::to_string(my_rank) + ".db";
  std::string timedb =
    " --timing-db timing_hf_" + std::to_string(my_rank) + ".db";

  std::string cmd = "";
  if (env_flags.has_value()) {
    cmd.append(env_flags.value());
  }
  cmd.append("art --nschedules 1 --nthreads " + std::to_string(nthreads) +
             nevts + " -c hf_root.fcl " + timedb + memdb +
             " -s sp_output.root");
  std::string outfile = " &> ";
  outfile.append("hf_log.txt");
  cmd.append(outfile);
  return std::system(cmd.c_str());
}

// return hostname along with printout of rank and total ranks
// and hardware concurrency for confidence check
// the printout is only relevant for debugging
std::string
get_hostname(int my_rank, int nranks)
{
  constexpr size_t LEN = 1024;
  std::string hostname;
  hostname.resize(LEN);
  if (gethostname(hostname.data(), LEN) == 0) {
    std::cout << "MPI rank " << my_rank << " of total " << nranks
              << " ranks is running on " << hostname << '\n';
  } else {
    std::cout << "could not determine execution resources being used!\n";
  }
  return hostname;
}

int
main(int argc, char* argv[])
{
  Config config = make_config(argc, argv);
  if (!config.valid()) {
    std::cerr << "invalid configuration\n";
    return 1;
  }

  int64_t events = config.nevents;
  std::string root_file_path = config.loadPath;
  bool load_events_to_hepnos = false;
  bool process_events_in_hepnos = false;
  if (config.use_hepnos) {
    load_events_to_hepnos = !root_file_path.empty();
    process_events_in_hepnos = config.process;
  }
  if (!process_events_in_hepnos && root_file_path.empty()) {
    std::cerr
      << "please provide the path to the root file to load events from! \n";
    return 1;
  }
  int64_t nthreads = config.nthreads;

  // initialize MPI after the configuration is all done and checked
  Mpi world(argc, argv);
  auto my_rank = world.rank();
  auto nranks = world.np();

  // Confidence check regarding execution resources being used
  // nranks and my_rank are only used to print out the rank information
  // they are not needed for getting the hostname
  auto hostname = get_hostname(my_rank, nranks);

  // Set special flags for running on Theta
  std::optional<std::string> env_flags;
  // hostname on theta is just nid0000, so check that we are
  // not running on csresearch for now.
  if (hostname.find("csresearch") == std::string::npos) {
    env_flags.emplace("PMI_NO_PREINITIALIZE=1 PMI_NO_FORK=1 ");
  }

  // create a directory for this MPI rank and cd into it!
  std::filesystem::create_directory(std::to_string(my_rank));
  if (config.use_hepnos) {
    // copy connection file to working directory!
    const auto copy_options = std::filesystem::copy_options::skip_existing;
    std::filesystem::copy_file(
      "connection.json",
      std::to_string(my_rank).append("/connection.json"),
      copy_options);
  }
  std::filesystem::current_path(std::to_string(my_rank));

  if (config.use_hepnos) {
    // finally either load events
    if (load_events_to_hepnos) {
      return loadData(my_rank, events, root_file_path, env_flags);
    }
    // OR process events
    if (process_events_in_hepnos) {
      return runSH(my_rank, events, nthreads, env_flags);
    }
  }

  if (config.use_root) {
    // with ROOT run both the processing steps
    if (runSP(my_rank, events, nthreads, root_file_path, env_flags) != 0) {
      std::cerr << "error when running signal processing with ROOT!";
    }
    //    if (runHF(my_rank, events, nthreads, env_flags) != 0) {
    //      std::cerr << "error when running hit finding with ROOT!";
    //    }
  }

  return 0;
}
