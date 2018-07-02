cd ../code/distributor
rm -f mpi.hostfile
cp createInstances.fake.sh createInstances.sh # can't call script createInstances.vboxDebian.sh directly. distributer crashes on MPI_Comm_Spawn if I do. Works perfectly with Ubuntu (16.4), but not with debian 8 or 9.
(cd ../../setup && ./vboxCreateInstances.sh 3 192.168.56.60 debian mpi) &&
make runSilent W=3 N=1459 && cat sampleMMul.out.master sampleMMul.err.master
