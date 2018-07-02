# author: Schuchardt Martin, csap9442

ALLEXECUTABLES     ?= mmulMPI

# algorithm specific definitions
# algorithm specific definitions

DEBUGGING           = -g
OPTIMIZATIONS       = -O3
PREPROCESSOR_DEFS   = -DACC_DEVICE_OFFSET=$(ACC_DEVICE_OFFSET) -DCPU_OR_GPU=$(CPU_OR_GPU)

ADDITIONAL_INC_PATHS= -I/usr/include/mpi # -I/usr/local/cuda/include # -I/usr/include/CL
ADDITIONAL_LIB_PATHS= -L/usr/lib/openmpi/lib -L/usr/local/cuda/lib64 # -L/opt/intel/opencl/lib64

# CC_LIB              = -lm
OCL_LIB             = -lOpenCL
# CUDA_LIB            = -lcudart -lcublas
# MPI_LIB             = -lmpi

CC                  = g++
# CUDA_CC             = nvcc
MPI_CC              = mpic++
 
CC_FLAGS            = $(OPTIMIZATIONS) -Wall -Werror -std=c++11 $(DEBUGGING) $(PREPROCESSOR_DEFS) $(ADDITIONAL_INC_PATHS) $(ADDITIONAL_LIB_PATHS)
# CUDA_CC_FLAGS       = $(OPTIMIZATIONS) -std=c++11 $(DEBUGGING) -Wno-deprecated-gpu-targets --compiler-options -Wall --compiler-options -Werror $(PREPROCESSOR_DEFS)
MPI_CC_FLAGS        = 



all: $(ALLEXECUTABLES)

mmulMPI: mmulMPI.cpp mmul.o #Makefile
	$(MPI_CC) $(MPI_CC_FLAGS) $(CC_FLAGS) $(filter-out %.h %.tpp %.cl Makefile, $^)  $(OCL_LIB) $(CUDA_LIB) $(MPI_LIB) -o $@

%.o: %.cpp %.h #Makefile
	$(CC) $(CC_FLAGS) $< -c

# %.o: %.c #Makefile
# 	$(CC) $(CC_FLAGS) $< -c
	

.PHONY: all run clean valgrind

run: $(ALLEXECUTABLES) Makefile
	@echo "***************************** execute *************************************"
	@(for file in ${ALLEXECUTABLES}; do\
	  echo "***************************** testing $$file ****************";\
	  mpirun -n 8 ./$$file;\
	done)
	@echo "***************************** done ****************************************"

valgrind: $(ALLEXECUTABLES)
	@echo "***************************** valgrind ************************************"
	@(for file in ${ALLEXECUTABLES}; do\
	  echo "***************************** testing $$file ****************";\
	  valgrind --tool=memcheck --leak-check=yes ./$$file;\
	done)
	@echo "***************************** done ****************************************"

clean:
	rm -f $(ALLEXECUTABLES) *.o
	rm -rf ./Debug ./x64 ./.vs
