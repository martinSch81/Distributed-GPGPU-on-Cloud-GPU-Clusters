# author: Schuchardt Martin, csap9442
# introduction how to migrate single-server code to a distributed version

ALLEXECUTABLES     = 01_singleVersion 02_distributedVersion

ACC_DEVICE_OFFSET  ?= 0
W                  ?= 2
N                  ?= 10


ADDITIONAL_INC_PATHS= -I/usr/include/mpi -I/usr/include/openmpi-x86_64 -I/usr/local/cuda/include -I/opt/nvidia/cuda/include # -I/usr/include/CL
ADDITIONAL_LIB_PATHS= -L/usr/lib/openmpi/lib -L/usr/local/cuda/lib64 -L/opt/nvidia/cuda/lib64 -L../distributor # -L/opt/intel/opencl/lib64

# CC_LIB               = -lm
OCL_LIB              = -lOpenCL
MPI_LIB              = -lmpi
DistributedGPGPU_lib = -lDistributedGPGPU

CC                  = g++
MPI_CC              = mpic++

CC_FLAGS            = -O3 -Wall -Werror -std=c++11 -DDEBUG_IGNORE_SILENT_STDERR -DDEBUG_IGNORE_SILENT_STDOUT -DACC_DEVICE_OFFSET=$(ACC_DEVICE_OFFSET) $(ADDITIONAL_INC_PATHS) $(ADDITIONAL_LIB_PATHS)
MPI_CC_FLAGS        = 



all: $(ALLEXECUTABLES)

../distributor/libDistributedGPGPU.a:
	make -C ../distributor libDistributedGPGPU.a
	
01_singleVersion: 01_singleVersion.cpp
	$(CC) $(CC_FLAGS) $^ $(OCL_LIB) $(MPI_LIB) $(CC_LIB) -o $@

02_distributedVersion: 02_distributedVersion.cpp ../distributor/libDistributedGPGPU.a
	$(MPI_CC) $(MPI_CC_FLAGS) $(CC_FLAGS) $^ $(DistributedGPGPU_lib) $(OCL_LIB) $(MPI_LIB) $(CC_LIB) -o $@



.PHONY: all run runSilent debug valgrind clean cleanCluster develop usage

distribute: 02_distributedVersion mpi.hostfile #Makefile
	@echo "***************************** distribute **********************************"
	@(for file in ${ALLEXECUTABLES}; do\
	  (for ip in `grep -E "^\s*[^#].*$\" mpi.hostfile | sed -e 's/\s*\(\S*\).*/\1/g'`; do\
	    scp $$file $$ip:/tmp/ || exit 1;\
	  done);\
	done)
	@touch distribute
	@echo "***************************** done ****************************************"

runSingle: 01_singleVersion
	@echo "***************************** execute *************************************"
	@echo "***************************** testing 01_singleVersion ****************";\
	./01_singleVersion || exit 1;
	@echo "***************************** done ****************************************"

run: 02_distributedVersion
	@echo "***************************** execute *************************************"
	@echo "***************************** testing 02_distributedVersion ****************";\
	touch restarting;\
	(while [ -f "restarting" ]; do\
	  rm restarting;\
	  mpirun --n 1 ./02_distributedVersion N=$N W=$W || exit 1;\
	done)
	@echo "***************************** done ****************************************"

runSilent: 02_distributedVersion
	@echo "***************************** execute silently ****************************"
	@echo "***************************** testing 02_distributedVersion ****************";\
	rm -f /tmp/02_distributedVersion.out.* ./02_distributedVersion.out.master ./02_distributedVersion.err.master;\
	touch restarting;\
	(while [ -f "restarting" ]; do\
	  rm restarting;\
	  mpirun --n 1 ./02_distributedVersion N=$N W=$W silent || exit 1;\
	done)
	@echo "***************************** done ****************************************"
	
clean:
	rm -f $(ALLEXECUTABLES) *.o
	rm -f *.out.master *.err.master distribute checkpoint.sav.* graph*.png graph*.dot
	rm -rf ./Debug ./x64 ./.vs

cleanCluster:
	@echo "***************** clean on cluster instances and host *********************"
	@(for file in ${ALLEXECUTABLES}; do\
	  rm -f $$file.out.master $$file.err.master distribute checkpoint.sav.* graph*.dot graph*.png || exit 1;\
	  (for ip in `grep -E "^\s*[^#].*$\" mpi.hostfile | sed -e 's/\s*\(\S*\).*/\1/g'`; do\
	    ssh $$ip "rm -f /tmp/$$file /tmp/$$file.out* /tmp/$$file.err* /tmp/checkpoint.sav.* /tmp/graph*.dot /tmp/distributor.shutdownSimulation" || exit 1;\
	  done);\
	done)
	@echo "***************************** done ****************************************"

