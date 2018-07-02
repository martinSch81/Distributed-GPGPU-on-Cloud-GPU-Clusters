#!/bin/bash
# clean, build and execute
# ATTENTION: check for correct ACC_DEVICES using showDevicesCL

rm ./leibniz/leibniz*.csv
rm ./microBenchmark/microBenchmark*.csv
rm ./mmul/mmul*.csv

echo "starting: $(date +\"%T\")"

echo "openCL: $(date +\"%T\")" > openCL.log
./openCL.sh >> openCL.log
echo "openCL: $(date +\"%T\")" >> openCL.log

echo "done: $(date +"%T")"
