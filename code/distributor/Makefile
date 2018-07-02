# author: Schuchardt Martin, csap9442

ALLEXECUTABLES     ?= sampleMMul-simple #sampleMMul #sampleCommunication

ACC_DEVICE_OFFSET  ?= 0
W                  ?= 2

# algorithm specific definitions
MAX_ROWS_PER_WORKER = 128
N                  ?= 727
# optional parameters
ifneq ($(M),)
  MM="M=$M"
endif
ifneq ($(O),)
  OO="O=$O"
endif
ifneq ($(P),)
  PP="P=$P"
endif
ifneq ($(Q),)
  QQ="Q=$Q"
endif
ifneq ($(R),)
  RR="R=$R"
endif
ifneq ($(S),)
  SS="S=$S"
endif
ifneq ($(T),)
  TT="T=$T"
endif
# algorithm specific definitions

DEBUG_FLAGS         = -DDEBUG_FORCE_ALWAYS_USING_ACC_DEVICE_0 #-DDEBUG_REDUCE_RESTART_CLUSTER_TIME_UNIT #-DDEBUG_SIMULATE_FAST_RESTART #-DDEBUG_IGNORE_SILENT_STDERR #-DDEBUG_IGNORE_SILENT_STDOUT

DEBUGGING           = $(DEBUG_FLAGS) # -g
OPTIMIZATIONS       = -O3
PREPROCESSOR_DEFS   = -DACC_DEVICE_OFFSET=$(ACC_DEVICE_OFFSET)

ADDITIONAL_INC_PATHS= -I/usr/include/mpi -I/usr/include/openmpi-x86_64 -I/usr/local/cuda/include -I/opt/nvidia/cuda/include # -I/usr/include/CL
ADDITIONAL_LIB_PATHS= -L. -L/usr/lib/openmpi/lib -L/usr/local/cuda/lib64 -L/opt/nvidia/cuda/lib64 # -L/opt/intel/opencl/lib64

# CC_LIB               = -lm
OCL_LIB              = -lOpenCL
# CUDA_LIB             = -lcudart -lcublas
MPI_LIB              = -lmpi
DistributedGPGPU_lib = -lDistributedGPGPU

CC                  = g++
# CUDA_CC             = nvcc
MPI_CC              = mpic++

CC_FLAGS            = $(OPTIMIZATIONS) -Wall -Werror -std=c++11 -fPIC $(DEBUGGING) $(PREPROCESSOR_DEFS) $(ADDITIONAL_INC_PATHS) $(ADDITIONAL_LIB_PATHS)
# CUDA_CC_FLAGS       = $(OPTIMIZATIONS) -std=c++11 $(DEBUGGING) -Wno-deprecated-gpu-targets --compiler-options -Wall --compiler-options -Werror $(PREPROCESSOR_DEFS)
MPI_CC_FLAGS        = 



all: $(ALLEXECUTABLES) #Makefile

cl_utils.o: ../utils/cl_utils.c ../utils/cl_utils.h #Makefile
	$(CC) $(CC_FLAGS) $< -c

Utils.o: ../utils/Utils.cpp ../utils/Utils.h #Makefile
	$(CC) $(CC_FLAGS) $< -c
	
Data.o Node.o Checkpoint.o Distributor.o: %.o: ./%.cpp ./%.h ./Distributor.h #Makefile
	$(CC) $(CC_FLAGS) $< -c
	
libDistributedGPGPU.a: Data.o Node.o Utils.o cl_utils.o Checkpoint.o Distributor.o #Makefile
	ar rcs $@ $^
	
sampleMMul: sampleMMul.cpp mmul.h mmul.tpp mmul.cl ../utils/time_ms.h libDistributedGPGPU.a #Makefile
	$(MPI_CC) $(MPI_CC_FLAGS) $(CC_FLAGS) $(filter-out %.h %.tpp %.cl Makefile, $^) $(DistributedGPGPU_lib) $(OCL_LIB) $(CUDA_LIB) $(MPI_LIB) -o $@

sampleMMul-simple: sampleMMul-simple.cpp mmul.h mmul.tpp mmul.cl ../utils/time_ms.h libDistributedGPGPU.a #Makefile
	$(MPI_CC) $(MPI_CC_FLAGS) $(CC_FLAGS) $(filter-out %.h %.tpp %.cl Makefile, $^) $(DistributedGPGPU_lib) $(OCL_LIB) $(CUDA_LIB) $(MPI_LIB) -o $@

sampleCommunication: sampleCommunication.cpp libDistributedGPGPU.a #Makefile
	$(MPI_CC) $(MPI_CC_FLAGS) $(CC_FLAGS) $(filter-out %.h %.tpp %.cl Makefile, $^) $(DistributedGPGPU_lib) $(OCL_LIB) $(CUDA_LIB) $(MPI_LIB) -o $@

# %.o: %.cu %.h #Makefile
# 	$(CUDA_CC) $(CUDA_CC_FLAGS) $< -c

# %.o: %.cpp %.h #Makefile
# 	$(CC) $(CC_FLAGS) $< -c

# %.o: %.c %.h #Makefile
# 	$(CC) $(CC_FLAGS) $< -c



.PHONY: all run runSilent debug valgrind clean cleanCluster develop

distribute: mpi.hostfile #Makefile
	@echo "***************************** distribute **********************************"
	@(for file in ${ALLEXECUTABLES}; do\
	  (for ip in `grep -E "^\s*[^#].*$\" mpi.hostfile | sed -e 's/\s*\(\S*\).*/\1/g'`; do\
	    scp $$file $$ip:/tmp/ || exit 1;\
	  done);\
	done)
	@touch distribute
	@echo "***************************** done ****************************************"

run: $(ALLEXECUTABLES)
	@echo "***************************** execute *************************************"
	@# optional parameter for mpirun: --display-map
	@(for file in ${ALLEXECUTABLES}; do\
	  echo "***************************** testing $$file ****************";\
	  touch restarting;\
	  (while [ -f "restarting" ]; do\
	    rm restarting;\
	    mpirun --n 1 ./$$file N=$N $(MM) $(OO) $(PP) $(QQ) $(RR) $(SS) $(TT) W=$W || exit 1;\
	  done) && \
	  (dot -Tpng graphDependencies.dot -o graphDependencies.png &&\
	  dot -Tpng graphComputation.dot -o graphComputation.png;)\
	done)
	@echo "***************************** done ****************************************"

runSilent: $(ALLEXECUTABLES)
	@echo "***************************** execute silently ****************************"
	@# optional parameter for mpirun: --display-map
	@(for file in ${ALLEXECUTABLES}; do\
	  echo "***************************** testing $$file ****************";\
	  rm -f /tmp/$$file.out.* ./$$file.out.master ./$$file.err.master;\
	  touch restarting;\
	  (while [ -f "restarting" ]; do\
	    rm restarting;\
	    mpirun --n 1 ./$$file N=$N $(MM) $(OO) $(PP) $(QQ) $(RR) $(SS) $(TT) W=$W silent || exit 1;\
	  done) && \
	  (awk '!a[$$0]++' graphDependencies.dot > tmp.dot; mv tmp.dot graphDependencies.dot; dot -Tpng graphDependencies.dot -o graphDependencies.png &&\
	  dot -Tpng graphComputation.dot -o graphComputation.png;)\
	done)
	@echo "***************************** done ****************************************"
	
debug: $(ALLEXECUTABLES)
	@echo "***************************** debug ***************************************"
	@(for file in ${ALLEXECUTABLES}; do\
	  echo "***************************** testing $$file ****************";\
	  gdb --args mpirun --n 1 ./$$file N=$N $(MM) $(OO) $(PP) $(QQ) $(RR) $(SS) $(TT) W=$W;\
	done)
	@echo "***************************** done ****************************************"

valgrind: $(ALLEXECUTABLES)
	@echo "***************************** valgrind ************************************"
	@(for file in ${ALLEXECUTABLES}; do\
	  echo "***************************** testing $$file ****************";\
	  valgrind --tool=memcheck --leak-check=yes --suppressions=/usr/share/openmpi/openmpi-valgrind.supp mpirun --n 1 ./$$file N=$N $(MM) $(OO) $(PP) $(QQ) $(RR) $(SS) $(TT) W=$W;\
	done)
	@echo "***************************** done ****************************************"

clean:
	rm -f $(ALLEXECUTABLES) *.o libDistributedGPGPU.a
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

develop:
	# cp ../../setup/output/mpi.hostfile .
	# make cleanCluster
	# rm mpi.hostfile
	# cp createInstances.fake.sh createInstances.sh
	make runSilent W=5 N=432 && cat sampleMMul.out.master sampleMMul.err.master
