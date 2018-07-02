EXECUTABLES = microBenchmarkCL_D$(ACC_DEVICE)
ALLEXECUTABLES = $(EXECUTABLES)

# arguments and default values
ACC_DEVICE         ?= 0
VERIFY             = 1

WORKGROUPSIZE      = 64
THREADS            = 100000
ITERS              = 750

OPTIMIZATION       = O3
ifeq ($(OPTIMIZATION),O0)
  CL_OPTIMIZATIONS=\"-cl-opt-disable\"
else
  CL_OPTIMIZATIONS=\"\"
endif

GCC                = gcc
GCCFLAGS           = -$(OPTIMIZATION) -std=c99 -Wall -Werror -lm -I/usr/local/cuda/include -I/opt/nvidia/cuda/include 
OCLLIB             = -lOpenCL -L/usr/local/cuda/lib64 -L/opt/nvidia/cuda/lib64 

ifeq ($(LOC),uni) # rr 15
  AMDOCLSDK=/scratch/c703/c703432/amd_ocl/AMD-APP-SDK-v2.6-RC3-lnx64
  OCLLIB=-I$(AMDOCLSDK)/include -L$(AMDOCLSDK)/lib/x86_64 -lOpenCL
endif

ifeq ($(LOC),o3)
  OCLLIB=-I../include -lOpenCL
endif

all: $(ALLEXECUTABLES) Makefile

%.o: %.c %.h
	$(GCC) $(GCCFLAGS) $< -c -DWORKGROUPSIZE=$(WORKGROUPSIZE) -DVERIFY=$(VERIFY)
  
cl_utils.o:  ../../utils/cl_utils.c ../../utils/cl_utils.h Makefile
	$(GCC) $(GCCFLAGS) $< -c

microBenchmarkCL_D$(ACC_DEVICE): microBenchmarkCL.c microBenchmark.o cl_utils.o Makefile
	$(GCC) $(GCCFLAGS) $(filter-out %.h Makefile, $^) $(OCLLIB) -o $@ -lm -DACC_DEVICE=$(ACC_DEVICE) -DWORKGROUPSIZE=$(WORKGROUPSIZE) -DVERIFY=$(VERIFY) -DCL_OPTIMIZATIONS=$(CL_OPTIMIZATIONS)

	

.PHONY: all run clean

run: $(EXECUTABLES) Makefile
	@echo "******************************  execute ***********************************"
	(for l in ${EXECUTABLES}; do echo "**************** testing $$l ****************"; ./$$l $(THREADS) $(ITERS); done)
	@echo "******************************** done *************************************"
	
clean:
	rm -f $(ALLEXECUTABLES) *.o
