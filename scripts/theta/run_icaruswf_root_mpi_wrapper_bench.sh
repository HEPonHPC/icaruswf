#!/bin/bash

#COBALT -A HEP_on_HPC
#COBALT -n 3
#COBALT -t 01:00:00
#COBALT --mode script

export MPICH_GNI_NDREG_ENTRIES=1024
export MPICH_MAX_THREAD_SAFETY=multiple

echo "%%% start $(date)"
module unload darshan
module swap PrgEnv-intel PrgEnv-gnu
module load gcc/9.3.0 
module swap cray-mpich/7.7.14 cray-mpich-abi/7.7.14
# activate spack environment
. /projects/HEP_on_HPC/icaruscode/spack/share/spack/setup-env.sh
spack env activate icaruscode-09_37_02_vecmt04-hepnos-0_7_2
echo "%%% after activating spack envrionment $(date)"

# set relevant env var
export ICARUSWF_BUILD=/projects/HEP_on_HPC/sajid/icarus_hepnos/icaruswf/build
export CET_PLUGIN_PATH=${ICARUSWF_BUILD}/src/modules:${CET_PLUGIN_PATH}
export FHICL_FILE_PATH=${ICARUSWF_BUILD}/fcl:${FHICL_FILE_PATH}

# for geometry files!
export FW_SEARCH_PATH=${FW_SEARCH_PATH}:/lus/theta-fs0/projects/HEP_on_HPC/icaruscode/spack/var/spack/environments/icaruscode-09_37_02_vecmt04-hepnos-0_7_2/.spack-env/view/gdml

export DATA_DIR=/projects/HEP_on_HPC/icarus_data/icaruscode-v09_37_01_03p02/icaruscode-09_37_01_03p02-samples
export CONFIG_DIR=/projects/HEP_on_HPC/icarus_data/icaruscode-v09_37_01_03p02/icaruscode-09_37_01_03p02-configs

export NUM_CLIENT_NODES=3
export NUM_CLIENT_TOTAL_RANKS=$(($NUM_CLIENT_NODES*32))
export NUM_CLIENT_RANKS_PER_NODE=$(($NUM_CLIENT_TOTAL_RANKS/$NUM_CLIENT_NODES))
export NUM_CLIENT_HYPERTHREADS=1
export NUM_CLIENT_HARDWARE_THREADS_PER_RANK=2

mkdir ht_${NUM_CLIENT_HYPERTHREADS}_depth_${NUM_CLIENT_HARDWARE_THREADS_PER_RANK}
cd ht_${NUM_CLIENT_HYPERTHREADS}_depth_${NUM_CLIENT_HARDWARE_THREADS_PER_RANK}

echo "%%% before icaruswf-root with $NUM_CLIENT_HARDWARE_THREADS_PER_RANK threads, at $(date)"
aprun -n $NUM_CLIENT_TOTAL_RANKS \
	-N $NUM_CLIENT_RANKS_PER_NODE \
	-d $NUM_CLIENT_HARDWARE_THREADS_PER_RANK \
	-j $NUM_CLIENT_HYPERTHREADS \
	-cc depth \
	${ICARUSWF_BUILD}/src/modules/mpi_wrapper -R -t ${NUM_CLIENT_HARDWARE_THREADS_PER_RANK} --num_evts_per_rank 1 --root_file_path /projects/HEP_on_HPC/icarus_data/icaruscode-v09_37_01_02p02/icaruscode-09_37_01_02p02-samples/prodcorsika_bnb_genie_protononly_overburden_icarus_20220118T213827-GenBNBbkgr_100evt_G4_DetSim.root &> root_analysis_out
echo "%%% after icaruswf-root with $NUM_CLIENT_HARDWARE_THREADS_PER_RANK threads, at $(date)"

