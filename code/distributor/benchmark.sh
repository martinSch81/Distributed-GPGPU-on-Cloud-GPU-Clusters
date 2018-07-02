#!/bin/bash
# build and execute

W=$1 # adapt arguments for specific benchmark
ALGORITHM=sampleMMul


echo "reading configuration..."
CONFIG_CFG="../../setup/config.cfg"
if [ -f $CONFIG_CFG ]; then
  source $CONFIG_CFG
  echo "...done!"
else
  echo -e "${RED}ERROR: host-specific configuration file not found: $CONFIG_CFG${NC}"
  exit -1
fi

  for RUN in {1..4}; do
    echo ""; echo ""; echo "";
    echo -e "${YELLOW}starting run ${RUN} of 4 for $ALGORITHM...${NC}"
      for N in 500 1000 2000 3000 #4000 #5000
      do
        # make -e clean
		make -e runSilent N=$N W=$W # adapt arguments for specific benchmark
        cat $ALGORITHM.out.master >> $ALGORITHM.benchmark.out
        cat $ALGORITHM.err.master >> $ALGORITHM.benchmark.err
      done
	
	echo -e "${YELLOW}done.${NC}"
  done

