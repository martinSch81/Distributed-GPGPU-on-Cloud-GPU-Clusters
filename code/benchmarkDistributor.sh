#!/bin/bash
# clean, build and execute
# handling sampleMMul, alg01 (for additional algorithms, add to 'prepare, cleanup', main loop and 'collect results')

echo "reading configuration..."
CONFIG_CFG="../setup/config.cfg"
if [ -f $CONFIG_CFG ]; then
  source $CONFIG_CFG
  echo "...done!"
else
  echo -e "${RED}ERROR: host-specific configuration file not found: $CONFIG_CFG${NC}"
  exit -1
fi

# MAX_W=5
MAX_W=10

LOGFILE=./output/benchmarkDistributor.out
ERR_LOGFILE=./output/benchmarkDistributor.err

# prepare, cleanup
rm -f $LOGFILE $ERR_LOGFILE ./output/*.csv
# TODO reactivate make -C distributor clean
rm -f ./distributor/sampleMMul.*.csv # delete csv-files from previous benchmarks first
rm -f ./alg01_gmres/alg01.*.csv
rm -f ./benchmark/leibniz/*.csv ./benchmark/microBenchmark/*.csv ./benchmark/mmul/*.csv
for ip in $(grep -E "^\s*[^#].*$" mpi.$MAX_W.hostfile | sed -e 's/\s*\(\S*\).*/\1/g')
do
	ssh $ip "rm -f /tmp/*.csv"
done


echo "starting: $(date +%T)" >> $LOGFILE
# for W in 1 2 3 4 5;
for W in 1 2 4 6 8 10;
do
  if [ ! -f "./mpi.$W.hostfile" ]; then
    echo -e "${RED}ERROR: could not find ./mpi.$W.hostfile. Benchmark needs mpi.[$MAX_W, 1].hostfile.${NC}"
	exit
  fi
done

# execute GPU benchmarks
cd benchmark
make clean
./runAll.sh
cd ..
cp ./benchmark/leibniz/*.csv ./output/
cp ./benchmark/microBenchmark/*.csv ./output/
cp ./benchmark/mmul/*.csv ./output/
cp ./benchmark/openCL.log ./output/

# execute Distributor-implemenations
# for W in 1 2 3 4 5;
for W in 1 2 4 6 8 10;
do
  echo -e "${YELLOW}starting benchmark with $W workers...${NC}"
  
  DIRECTORY=distributor
  ALGORITHM=sampleMMul
    cp ./mpi.$W.hostfile ./$DIRECTORY/mpi.hostfile
	rm -f ./$DIRECTORY/$ALGORITHM.benchmark.err ./$DIRECTORY/$ALGORITHM.benchmark.out
    echo "benchmarking $ALGORITHM ($W workers): $(date +%T)" >> $ERR_LOGFILE
    echo "benchmarking $ALGORITHM ($W workers): $(date +%T)" >> $LOGFILE
    (cd ./$DIRECTORY && ./benchmark.sh $W) # adapt arguments for specific benchmark
    cat ./$DIRECTORY/$ALGORITHM.benchmark.err >> $ERR_LOGFILE
    cat ./$DIRECTORY/$ALGORITHM.benchmark.out >> $LOGFILE
    rm -f ./$DIRECTORY/$ALGORITHM.benchmark.err ./$DIRECTORY/$ALGORITHM.benchmark.out
    echo "benchmarking $ALGORITHM ($W workers): $(date +%T)" >> $ERR_LOGFILE
    echo "benchmarking $ALGORITHM ($W workers): $(date +%T)" >> $LOGFILE


  DIRECTORY=alg01_gmres
  ALGORITHM=alg01
    cp ./mpi.$W.hostfile ./$DIRECTORY/mpi.hostfile
	rm -f ./$DIRECTORY/$ALGORITHM.benchmark.err ./$DIRECTORY/$ALGORITHM.benchmark.out
    echo "benchmarking $ALGORITHM ($W workers): $(date +%T)" >> $ERR_LOGFILE
    echo "benchmarking $ALGORITHM ($W workers): $(date +%T)" >> $LOGFILE
    (cd ./$DIRECTORY && ./benchmark.sh $W) # adapt arguments for specific benchmark
    cat ./$DIRECTORY/$ALGORITHM.benchmark.err >> $ERR_LOGFILE
    cat ./$DIRECTORY/$ALGORITHM.benchmark.out >> $LOGFILE
    rm -f ./$DIRECTORY/$ALGORITHM.benchmark.err ./$DIRECTORY/$ALGORITHM.benchmark.out
    echo "benchmarking $ALGORITHM ($W workers): $(date +%T)" >> $ERR_LOGFILE
    echo "benchmarking $ALGORITHM ($W workers): $(date +%T)" >> $LOGFILE

  DIRECTORY=alg01_gmres
  ALGORITHM=alg01
	rm -f ./$DIRECTORY/${ALGORITHM}_legacy.benchmark.out
    echo "benchmarking ${ALGORITHM}_legacy (no distribution): $(date +%T)" >> $ERR_LOGFILE
    echo "benchmarking ${ALGORITHM}_legacy (no distribution): $(date +%T)" >> $LOGFILE
    (cd ./$DIRECTORY && ./benchmark_legacy.sh) # adapt arguments for specific benchmark
    cat ./$DIRECTORY/${ALGORITHM}_legacy.benchmark.out >> $LOGFILE
    rm -f ./$DIRECTORY/${ALGORITHM}_legacy.benchmark.out
    echo "benchmarking ${ALGORITHM}_legacy (no distribution): $(date +%T)" >> $ERR_LOGFILE
    echo "benchmarking ${ALGORITHM}_legacy (no distribution): $(date +%T)" >> $LOGFILE

  echo -e "${YELLOW}done.${NC}"
done

# collect results
cp ./distributor/sampleMMul.0.*.csv ./output/sampleMMul.master.csv # collect csv-files from all algorithms
cp ./alg01_gmres/alg01.0.*.csv ./output/alg01.master.csv
cp ./alg01_gmres/alg01.999.*.csv ./output/alg01.legacy.csv
for ip in $(grep -E "^\s*[^#].*$" mpi.$MAX_W.hostfile | sed -e 's/\s*\(\S*\).*/\1/g')
do
	scp $ip:/tmp/*.csv ./output/
done

echo "all done: $(date +%T)" >> $LOGFILE
