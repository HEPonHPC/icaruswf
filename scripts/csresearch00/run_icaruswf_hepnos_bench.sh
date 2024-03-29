#!/bin/bash
set -euo pipefail

export MPICH_MAX_THREAD_SAFETY=multiple

# activate spack environment
. /scratch/gartung/spack/share/spack/setup-env.sh
spack load gcc@9.3.0
spack env activate icaruscode-09_37_02_vecmt04-hepnos-0_7_0
export ICARUSWF_SRC=/nashome/s/sasyed/packages/icaruswf
. ${ICARUSWF_SRC}/envvariable.sh

export ICARUSWF_BUILD=/nashome/s/sasyed/packages/icaruswf/build
export CET_PLUGIN_PATH=${ICARUSWF_BUILD}/src/modules:${CET_PLUGIN_PATH}
export FHICL_FILE_PATH=${ICARUSWF_BUILD}/fcl:${FHICL_FILE_PATH}

export DATA_DIR=/scratch/cerati/icaruscode-v09_37_01_02p02/icaruscode-09_37_01_02p02-samples
# for geometry files!
export FW_SEARCH_PATH=${FW_SEARCH_PATH}:/scratch/gartung/spack/opt/spack/linux-scientific7-x86_64_v2/gcc-9.3.0/icarusalg-09.37.02.01-tyt7hh6cyhthdx5ut6ngugcrdybpgclw/gdml
# for pandora xml file
export FW_SEARCH_PATH=${FW_SEARCH_PATH}:/scratch/gartung/spack/var/spack/environments/icaruscode-09_37_02_vecmt04-hepnos-0_7_0/.spack-env/view/fw

# collect diagnostic profiles
export MARGO_ENABLE_DIAGNOSTICS=0
export MARGO_ENABLE_PROFILING=0

# number of events to upload and process
export NUM_EVENTS=25

export BASEDIR=$(pwd)

# Create all the directories!
for THREADS in 1 2 4 8 16 32
do

  cd $BASEDIR
  mkdir -p threads_$THREADS
  cd threads_$THREADS

  for RUN in 1
  do
    cd $BASEDIR/threads_$THREADS
    mkdir -p run_$RUN
    cd run_$RUN

    # Copy hepnos.json config file
    cp $BASEDIR/hepnos.json .
  done
done
cd $BASEDIR

# run the benchmark
for RUN in 1
do
  # run icaruswf-bench with varying number of client threads
  for THREADS in 1 2 4 8 16 32
  do
      cd $BASEDIR/threads_$THREADS/run_$RUN
      export NUM_CLIENT_THREADS_PER_RANK=$THREADS

      # start the hepnos server
      echo "%%% before starting HEPnOS server for run with with $THREADS threads, run number $RUN at $(date)"
      mpirun -np 1 bedrock ofi+tcp -c hepnos.json -v trace &> server-log &
      sleep 30
      hepnos-list-databases ofi+tcp -s hepnos.ssg > connection.json
      echo "%%% after starting HEPnOS server for run with with $THREADS threads, run number $RUN at $(date)"

      # Store data (raw::RawDigits) to hepnos for the first step, signal processing
      echo "%%% before loading data for run with with $THREADS threads, run number $RUN at $(date)"
      art -c storedata.fcl -s /scratch/cerati/icaruscode-v09_37_01_02p02/icaruscode-09_37_01_02p02-samples/prodcorsika_bnb_genie_protononly_overburden_icarus_20220118T213827-GenBNBbkgr_100evt_G4_DetSim.root -n ${NUM_EVENTS} &> loading_log
      echo "%%% after loading data for run with with $THREADS threads, run number $RUN at $(date)"

      echo "%%% before icaruswf-hepnos-sigproc-hitfind with $THREADS threads, run number $RUN at $(date)"
      art --nschedules 1 --nthreads ${NUM_CLIENT_THREADS_PER_RANK} -c icarus_SH.fcl -n ${NUM_EVENTS} &> sp_hf_out
      echo "%%% after icaruswf-hepnos-sigproc-hitfind with $THREADS threads, run number $RUN at $(date)"

      #echo "%%% before icaruswf-hepnos-sigproc with $THREADS threads, run number $RUN at $(date)"
      #art --nschedules 1 --nthreads ${NUM_CLIENT_THREADS_PER_RANK} -c sp_hepnos.fcl -n ${NUM_EVENTS} &> sp_out
      #echo "%%% after icaruswf-hepnos-sigproc with $THREADS threads, run number $RUN at $(date)"

      #echo "%%% before icaruswf-hepnos-hit-find with $THREADS threads, run number $RUN at $(date)"
      #art --nschedules 1 --nthreads ${NUM_CLIENT_THREADS_PER_RANK} -c hf_hepnos.fcl -n ${NUM_EVENTS} &> hf_out
      #echo "%%% after icaruswf-hepnos-hit-find with $THREADS threads, run number $RUN at $(date)"

      echo "%%% before shutting down HEPnOS server for run with $THREADS threads, run number $RUN at $(date)"
      hepnos-shutdown ofi+tcp connection.json
      echo "%%% after shutting down HEPnOS server for run with $THREADS threads, run number $RUN at $(date)"

  done
done
