# Using spack environment with icarus code and hepnos to build new code on Theta and run icarus workflow with hepnos using interactive session

1. clone the repository, : `git clone git@github.com:HEPonHPC/icaruswf.git`

2. Get an interactive node: `qsub -I -n 4 -t 01:00 --attrs enable_ssh=1 -q debug-cache-quad -A HEP_on_HPC`

3. Change the cray programming environment to gnu : `module swap PrgEnv-intel PrgEnv-gnu`

4. `export ICARUSWF_SRC=<path to cloned repository>` 

5. make a build directory for out of source build and cd into it. 

6. Load the system provided modules `gcc@9.3.0` and `cray-mpich` via `module load gcc/9.3.0 && module load cray-mpich/7.7.14`
 
7. Setup spack by `source /projects/HEP_on_HPC/icaruscode/spack/share/spack/setup-env.sh` 
 
8. activate spack environment: `spack env activate icaruscode-09_37_02_vecmt04-hepnos-0_7_2`
 
9. set up the environment variables: `source ${ICARUSWF_SRC}/envvariable.sh` 
 
10. enable dynamic linking via `export CRAYPE_LINK_TYPE=dynamic`

11. make a build directory via `mkdir -p build`, run cmake in the build directory: `cd build && cmake -DCMAKE_CXX_COMPILER=CC -DUSE_GNI_TRANSPORT=ON -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_CXX_FLAGS="-O3" ${ICARUSWF_SRC}`; where `CC` is the cray compiler wrapper for the underlying c++ compiler.

12. build the code: `make -j10` 

13. Update the `CET_PLUGIN_PATH`: `export CET_PLUGIN_PATH=${ICARUSWF_SRC}/build/src/modules:${CET_PLUGIN_PATH}`

14. Update the `FHICL_FILE_PATH`: `export FHICL_FILE_PATH=${ICARUSWF_SRC}/build/fcl:${FHICL_FILE_PATH}`

15. define following before starting hepnos server: `export MPICH_GNI_NDREG_ENTRIES=1024 && export MPICH_MAX_THREAD_SAFETY=multiple`

16. Define you protection domain, which can only be used by you: `export PDOMAIN=mypdomainname && apstat -P | grep ${PDOMAIN} || apmgr pdomain -c -u ${PDOMAIN}`

17. Move to the test sub-directory `cd ${ICARUSWF_SRC}/build/test`

18. Start hepnos server: `aprun -n 2 -p ${PDOMAIN} bedrock ofi+gni -c hepnos.json &`, 

19. followed by `aprun -n 1 -p ${PDOMAIN} hepnos-list-databases ofi+gni -s hepnos.ssg > connection.json`, there may be some extra lines at the end of connection.json, they should be removed. 

20. Store data to hepnos: `aprun -n 1 -N 1 -p ${PDOMAIN} art -c storedata.fcl -s /projects/HEP_on_HPC/icarus_data/icaruscode-v09_37_01_02p02/icaruscode-09_37_01_02p02-samples/prodcorsika_bnb_genie_protononly_overburden_icarus_20220118T213827-GenBNBbkgr_100evt_G4_DetSim.root -n 1`

21. Run signal processing, using hepnos as input and output: `aprun -n 1 -N 1 -p ${PDOMAIN} art -c sp_hepnos.fcl`

22. Run hit finding, using hepnos as input and output: `aprun -n 1 -N 1 -p ${PDOMAIN} art -c hf_hepnos.fcl`

23. Run pandora, using hepnos as input and output: `aprun -n 1 -N 1 -p ${PDOMAIN} art -c p_hepnos.fcl`

