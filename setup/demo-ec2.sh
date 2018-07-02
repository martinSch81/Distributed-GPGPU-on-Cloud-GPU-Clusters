./awsCreateInstances.sh keys/mpi-us-east-1.pem mpi-us-east-1 1 master mpi MASTER_INSTANCE

master_external_ip=$(cat ./output/external_ip.ec2-master)
ssh $master_external_ip "printf \"Host github.com\n  StrictHostKeyChecking no\n\" >> ~/.ssh/config && cd ~/git && git clone git@github.com:martinSch81/DistributedGPGPU.git"

# create workers on master:
  scp config.cfg $master_external_ip:~/git/DistributedGPGPU/setup/config.cfg
  scp keys/mpi-us-east-1.pem $master_external_ip:~/git/DistributedGPGPU/setup/keys/
  # ssh $master_external_ip "cd ~/git/DistributedGPGPU/setup/ && ./awsCreateInstances.sh keys/mpi-us-east-1.pem mpi-us-east-1 3"

ssh $master_external_ip "cd ~/git/DistributedGPGPU/code/distributor && cp createInstances.ec2.sh createInstances.sh"
ssh $master_external_ip "cd ~/git/DistributedGPGPU/code/distributor && make runSilent W=3 N=5432 && cat sampleMMul.out.master sampleMMul.err.master"
