EXECUTABLES = mmulCL_D$(ACC_DEVICE)_$(REAL)
ALLEXECUTABLES = $(EXECUTABLES)

# arguments and default values
ACC_DEVICE         ?= 0

# NUMBER_TYPE: 0..int; 1..float; 2..double
REAL               = int
NUMBER_TYPE        = 0
ifeq ($(REAL),float)
  NUMBER_TYPE      = 1
else ifeq ($(REAL),double)
  NUMBER_TYPE      = 2
else
  REAL             = int
  NUMBER_TYPE      = 0
endif

N                  = 1024
VERIFY             = 1

WORKGROUPSIZE      = 64
TILESIZE           = 16 # may not be larger on AMD

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

cl_utils.o:  ../../utils/cl_utils.c ../../utils/cl_utils.h Makefile
	$(GCC) $(GCCFLAGS) $< -c

mmulCL_D$(ACC_DEVICE)_$(REAL): mmul.c mmulCL.c cl_utils.o Makefile
	$(GCC) $(GCCFLAGS) $(filter-out %.h Makefile, $^) $(OCLLIB) -o $@ -DNUMBER_TYPE=$(NUMBER_TYPE) -DWORKGROUPSIZE=$(WORKGROUPSIZE) -DTILESIZE=$(TILESIZE) -DACC_DEVICE=$(ACC_DEVICE) -DVERIFY=$(VERIFY) -DCL_OPTIMIZATIONS=$(CL_OPTIMIZATIONS)

	

.PHONY: all run clean

run: $(EXECUTABLES) Makefile
	@echo "******************************  execute ***********************************"
	@(for l in ${EXECUTABLES}; do echo "**************** testing $$l ****************"; ./$$l $(N); done)
	@echo "******************************** done *************************************"
	
clean:
	rm -f $(ALLEXECUTABLES) *.o
