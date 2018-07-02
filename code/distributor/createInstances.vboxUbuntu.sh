#!/bin/bash
# 
# execute script with the number of instances to launch as argument.
if [[ -f mpi.hostfile ]]; then
  echo "found a 'mpi.hostfile', this indicates that a cluster already exists. Quitting..."
  exit 0
fi

(cd ../../setup && ./vboxCreateInstances.sh $1 192.168.56.60 ubuntu mpi) &&
cp ../../setup/output/mpi.hostfile ./mpi.hostfile
