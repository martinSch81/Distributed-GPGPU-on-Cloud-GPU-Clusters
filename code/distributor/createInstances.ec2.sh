#!/bin/bash
# 
# execute script with the number of instances to launch as argument.
if [[ -f mpi.hostfile ]]; then
  echo "found a 'mpi.hostfile', this indicates that a cluster already exists. Quitting..."
  exit 0
fi

(cd ../../setup && ./awsCreateInstances.sh keys/mpi-us-east-1.pem mpi-us-east-1 $1) &&
cp ../../setup/output/mpi.hostfile ./mpi.hostfile
