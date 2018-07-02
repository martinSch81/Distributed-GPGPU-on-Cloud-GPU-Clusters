cd ../code/distributor
rm -f mpi.hostfile
cp createInstances.vboxUbuntu.sh createInstances.sh
make runSilent W=5 N=2347 && cat sampleMMul.out.master sampleMMul.err.master
