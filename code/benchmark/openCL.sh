#!/bin/bash
# clean, build and execute
# for all

GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

# run showDevicesCL to detect which id belongs to which device
ACC_DEVICE=0
if [ ! "$LOC" = "" ]
then
  if [ "$LOC" = "home" ]
  then
    ACC_DEVICE=0
  elif [ "$LOC" = "aws" ]
  then
    ACC_DEVICE=0
  fi
  echo "location $LOC, using ACC_DEVICE $ACC_DEVICE"
else
  echo "using default ACC_DEVICE $ACC_DEVICE"
fi

cd ./leibniz

  echo ""; echo ""; echo "";
  echo -e "${GREEN}executing on device ${ACC_DEVICE}...${NC}"
  LEIBNIZ=leibnizCL_D${ACC_DEVICE}
  MICROBENCHMARK=microBenchmarkCL_D${ACC_DEVICE}
  MMUL=mmulCL_D${ACC_DEVICE}

  VERIFY=1
  for RUN in {1..3}
  do
    echo ""; echo ""; echo "";
    echo -e "${YELLOW}starting run ${RUN} of 3...${NC}"
    cd ../leibniz
    for REAL in float double
    do
      for STEPS in 16777216 67108864 268435456 1073741824 2147483648 # 4294967296
      do
        # for WORKSIZE in 128 512 2048 8192 32768
        for WORKSIZE in 1024
        do
          make -e clean REAL=$REAL ACC_DEVICE=${ACC_DEVICE}
          make -e ${LEIBNIZ}_$REAL REAL=$REAL WORKSIZE=$WORKSIZE ACC_DEVICE=${ACC_DEVICE} VERIFY=$VERIFY
          ./${LEIBNIZ}_$REAL $STEPS
        done
      done
    done
    
    cd ../microBenchmark
    for THREADS in 31250 125000 500000 # 2000000
    do
      # for ITERS in 125 500 2000
      for ITERS in 125 2000
      do
        # for WORKGROUPSIZE in 128 512 2048 8192 32768
        for WORKGROUPSIZE in 1024
        do
          make -e clean ACC_DEVICE=${ACC_DEVICE}
          make -e ${MICROBENCHMARK} WORKGROUPSIZE=$WORKGROUPSIZE ACC_DEVICE=${ACC_DEVICE} VERIFY=$VERIFY
          ./${MICROBENCHMARK} $THREADS $ITERS
    	done
      done
    done
    
    cd ../mmul
    for REAL in int float double
    do
      for N in 256 512 1024 2048
      do
        # for WORKGROUPSIZE in 128 512 2048
        for WORKGROUPSIZE in 128
        do
          # for TILESIZE in 4 8 16
          for TILESIZE in 16 # 32 # best/ allowed tilesize; CPU: 16; home: 16; o3: 16; o9: 16; o11: freak values
          do
            make -e clean REAL=$REAL ACC_DEVICE=${ACC_DEVICE}
            make -e ${MMUL}_$REAL REAL=$REAL WORKGROUPSIZE=$WORKGROUPSIZE TILESIZE=$TILESIZE ACC_DEVICE=${ACC_DEVICE} VERIFY=$VERIFY
            ./${MMUL}_$REAL $N
    	  done
        done
      done
    done
	
	VERIFY=0
  done

