#!/bin/bash

#COBALT -A HEP_on_HPC
#COBALT -n 4
#COBALT -t 01:00:00
#COBALT --mode script

export NUM_DAEMON_NODES=1
export NUM_DAEMON_TOTAL_RANKS=$(($NUM_DAEMON_NODES*16))
export NUM_DAEMON_HYPERTHREADS=1
export NUM_DAEMON_RANKS_PER_NODE=$(($NUM_DAEMON_TOTAL_RANKS/$NUM_DAEMON_NODES))
export NUM_DAEMON_THREADS_PER_RANK=$(($NUM_DAEMON_HYPERTHREADS*64/$NUM_DAEMON_RANKS_PER_NODE))

PDOMAIN=hepnos-sajid
SSGFILE=hepnos.ssg
CONNECTIONFILE=$(pwd)/connection.json
ICARUSWF_BUILD=/projects/HEP_on_HPC/sajid/icarus_hepnos/icaruswf/build

rm ${SSGFILE}
rm ${CONNECTIONFILE}

export MPICH_GNI_NDREG_ENTRIES=1024
export MPICH_MAX_THREAD_SAFETY=multiple
export CET_PLUGIN_PATH=${ICARUSWF_BUILD}/src/modules:${CET_PLUGIN_PATH}
export FHICL_FILE_PATH=${ICARUSWF_BUILD}/fcl:${FHICL_FILE_PATH}
# for geometry files!
export FW_SEARCH_PATH=${FW_SEARCH_PATH}:/lus/theta-fs0/projects/HEP_on_HPC/icaruscode/spack/var/spack/environments/icaruscode-09_37_02_vecmt04-hepnos-0_7_2/.spack-env/view/gdml

echo "%%% start $(date)"
module unload darshan
module swap PrgEnv-intel PrgEnv-gnu
module load gcc/9.3.0 
module swap cray-mpich/7.7.14 cray-mpich-abi/7.7.14
echo "%%% after_module_loads $(date)"

echo "Setting up protection domain"
apstat -P | grep ${PDOMAIN} || apmgr pdomain -c -u ${PDOMAIN}

# activate spack environment
. /projects/HEP_on_HPC/icaruscode/spack/share/spack/setup-env.sh
spack env activate icaruscode-09_37_02_vecmt04-hepnos-0_7_2

# options to collect diagnostic profiles
export MARGO_ENABLE_DIAGNOSTICS=0
export MARGO_ENABLE_PROFILING=0

echo "%%% before_start_daemon $(date)"
aprun -n $NUM_DAEMON_TOTAL_RANKS \
	-N $NUM_DAEMON_RANKS_PER_NODE \
	-d $NUM_DAEMON_THREADS_PER_RANK \
	-j $NUM_DAEMON_HYPERTHREADS \
	-cc none \
	-p ${PDOMAIN} \
	bedrock ofi+gni -c hepnos.json -v info &> server-log &
sleep 30
echo "%%% after_start_daemon $(date)"
while [ ! -f ${SSGFILE} ]; do sleep 10 && echo "waiting for ssgfile"; done

echo "%%% before_start_list_dbs $(date)"
aprun -n 1 -N 1 \
	-p ${PDOMAIN} \
	hepnos-list-databases ofi+gni -s ${SSGFILE} > ${CONNECTIONFILE}
sleep 2
echo "%%% after_start_list_dbs $(date)"
while [ ! -f ${CONNECTIONFILE} ]; do sleep 10; done
sed -i '$ d' ${CONNECTIONFILE} # we have to because aprun adds a line

# Create Queues
aprun -n 1 -N 1 -p ${PDOMAIN} ${ICARUSWF_BUILD}/src/modules/cheesyQueue_maker ofi+gni connection.json DetSim HitFinding &> queue-creation-log

export DATA_DIR=/projects/HEP_on_HPC/icarus_data/icaruscode-v09_37_01_03p02/icaruscode-09_37_01_03p02-samples
export CONFIG_DIR=/projects/HEP_on_HPC/icarus_data/icaruscode-v09_37_01_03p02/icaruscode-09_37_01_03p02-configs

export NUM_CLIENT_NODES=3
export NUM_CLIENT_TOTAL_RANKS=$(($NUM_CLIENT_NODES*32))
export NUM_CLIENT_RANKS_PER_NODE=$(($NUM_CLIENT_TOTAL_RANKS/$NUM_CLIENT_NODES))
export NUM_CLIENT_HYPERTHREADS=1
export NUM_CLIENT_HARDWARE_THREADS_PER_RANK=2

mkdir ht_${NUM_CLIENT_HYPERTHREADS}_depth_${NUM_CLIENT_HARDWARE_THREADS_PER_RANK}
cd ht_${NUM_CLIENT_HYPERTHREADS}_depth_${NUM_CLIENT_HARDWARE_THREADS_PER_RANK}
cp $CONNECTIONFILE .

echo "%%% before icaruswf-load with $NUM_CLIENT_HARDWARE_THREADS_PER_RANK threads, at $(date)"
aprun -n $NUM_CLIENT_TOTAL_RANKS \
	-N $NUM_CLIENT_RANKS_PER_NODE \
	-d $NUM_CLIENT_HARDWARE_THREADS_PER_RANK \
	-j $NUM_CLIENT_HYPERTHREADS \
	-cc none \
	-p ${PDOMAIN} \
	${ICARUSWF_BUILD}/src/modules/mpi_wrapper -H --num_evts_per_rank 1 --root_file_path /projects/HEP_on_HPC/icarus_data/icaruscode-v09_37_01_02p02/icaruscode-09_37_01_02p02-samples/prodcorsika_bnb_genie_protononly_overburden_icarus_20220118T213827-GenBNBbkgr_100evt_G4_DetSim.root &> load_out
echo "%%% after icaruswf-load with $NUM_CLIENT_HARDWARE_THREADS_PER_RANK threads, at $(date)"

echo "%%% before icaruswf-process with $NUM_CLIENT_HARDWARE_THREADS_PER_RANK threads, at $(date)"
aprun -n $NUM_CLIENT_TOTAL_RANKS \
	-N $NUM_CLIENT_RANKS_PER_NODE \
	-d $NUM_CLIENT_HARDWARE_THREADS_PER_RANK \
	-j $NUM_CLIENT_HYPERTHREADS \
	-cc none \
	-p ${PDOMAIN} \
	${ICARUSWF_BUILD}/src/modules/mpi_wrapper -H -p -t ${NUM_CLIENT_HARDWARE_THREADS_PER_RANK} --num_evts_per_rank 1 &> process_out
echo "%%% after icaruswf-process with $NUM_CLIENT_HARDWARE_THREADS_PER_RANK threads, at $(date)"

cd ${BASEDIR}

echo "%%% before_start_shutdown $(date)"
aprun -n 1 \
      -N 1 \
      -p ${PDOMAIN} \
      hepnos-shutdown ofi+gni ${CONNECTIONFILE}
echo "%%% after_end_shutdown $(date)"

echo "Destroying protection domain"
apmgr pdomain -r -u ${PDOMAIN}
