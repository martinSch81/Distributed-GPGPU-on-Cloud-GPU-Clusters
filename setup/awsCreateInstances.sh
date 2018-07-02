# !/bin/bash

#######################################################################################################################
### ARGUMENT CHECKING #################################################################################################
#######################################################################################################################
USAGE="${RED}ERROR: call script with 3 (optional 5 or 6) arguments,\n\
 1) path to pem-file (script will set permission to 600)\n\
 2) name of keypair (usually identical with the name of the pem-file without file suffix)\n\
 3) number of instances\n\
 optional:\n\
 4*) generate EC2-master node: [ master | <ANYTHING_ELSE> ] (number of instances needs to be 1 for creation of master)\n\
 5*) password for samba share (\\<HOSTNAME>\git) (required for EC2-master node only)\n\
 6**) tag for all instances (cluster tag);\n\
           needs to be different than any other currently configured EC2-instances including corresponding EC2-master node\n\
 7***) comma separated list of pre-created instance public dns names, only configuration will be made
 \n\
 eg. ./awsCreateInstances.sh keys/usWestNVirginia.pem usWestNVirginia 5\n\
 eg. ./awsCreateInstances.sh keys/usWestNVirginia.pem usWestNVirginia 1 master PASSWORD OPTIONAL_UNIQUE_TAG\n\
 eg. ./awsCreateInstances.sh keys/usWestNVirginia.pem usWestNVirginia 3 ANYTHING ANYTHING GPU_CLUSTER\n\
 eg. ./awsCreateInstances.sh keys/usWestNVirginia.pem ANYTHING 815 ANYTHING ANYTHING ANYTHING ec2-user@ec2-192-168-0-1.ap-northeast-1.compute.amazonaws.com,...${NC}"


EC2_PEMFILE=$1
KEY_NAME=$2
if [[ -z $EC2_PEMFILE || -z $KEY_NAME || -z $3 ]]; then
  echo -e $USAGE
  exit -1
fi

if [ -r $EC2_PEMFILE ]; then
  chmod 600 $EC2_PEMFILE
else
  echo -e $USAGE
  echo -e "${RED}\n\nERROR: please specify path to pem-file${NC}"
  exit -1
fi


NBR_CLUSTER_NODES=2
re='^[0-9]+$'
if [[ $3 =~ $re ]]; then
  NBR_CLUSTER_NODES=$3
  if [ $NBR_CLUSTER_NODES -lt 1 ]; then
    echo -e $USAGE
    echo -e "${RED}\n\nERROR: at least one node needs to be instanciated${NC}"
    exit -1
  fi
else
  echo -e $USAGE
  echo -e "${RED}\n\nERROR: specify number of requested instances${NC}"
  exit -1
fi

IS_MASTER=$4
SAMBA_PW=$5
if [[ "$IS_MASTER" == "master" ]]; then
  if [ $NBR_CLUSTER_NODES != 1 ]; then
    echo -e $USAGE
    echo -e "${RED}\n\nERROR: creation of master-node requires number of instances to be set to 1${NC}"
	exit -1
  fi
  if [[ -z "$SAMBA_PW" ]]; then
    echo "Please enter the password that will be used for the samba-share:"
    read -s SAMBA_PW
  fi
fi

if [[ -z "$6" ]]; then
  CLUSTER_ID=cluster_`date +'%Y%m%d-%H%M%S'`
else
  CLUSTER_ID=$6
fi

if [[ ! -z "$7" ]]; then
  PREDEFINED_INSTANCES=$7
fi

echo "reading configuration..."
if [ -f ./config.cfg ]; then
  source ./config.cfg
else
  echo -e "${RED}ERROR: host-specific configuration file not found: ./config.cfg${NC}"
  exit -1
fi

if [[ $INSTANCE_TYPE == g2.* || $INSTANCE_TYPE == p2.* ]]; then
  PLACEMENT_GROUP="--placement GroupName=$EC2_PLACEMENT_GROUP"
else # t2 cannot be instanced in placement groups
  PLACEMENT_GROUP=""
fi

USERNAME=ec2-user
#######################################################################################################################
### Functions #########################################################################################################
#######################################################################################################################
function sshCmd {
  ssh -oStrictHostKeyChecking=no -tt -i $EC2_PEMFILE $USERNAME@$1 $2
}

function scpCmd {
  scp -oStrictHostKeyChecking=no -i $EC2_PEMFILE $2 $USERNAME@$1:$3
}

function waitForNode {
  ssh -oStrictHostKeyChecking=no -i $EC2_PEMFILE -q $1@$2 exit
  while [ $? != 0 ]
  do
      echo "  $2 is still not available..."
      sleep 2
      ssh -oStrictHostKeyChecking=no -i $EC2_PEMFILE -q $1@$2 exit
  done
}
#######################################################################################################################
###      ##############################################################################################################
### MAIN ##############################################################################################################
###      ##############################################################################################################
#######################################################################################################################

if [[ -z "$PREDEFINED_INSTANCES" ]]; then
  echo "***  creating instances... **********************************************"
    nodes=$(aws ec2 run-instances $DRY_RUN --image-id $IMAGE_ID --count $NBR_CLUSTER_NODES --instance-type $INSTANCE_TYPE $PLACEMENT_GROUP --key-name $KEY_NAME --security-groups $EC2_SECURITY_GROUP --instance-initiated-shutdown-behavior $SHUTDOWN_BEHAVIOR --query 'Instances[*].{InstanceId:InstanceId,InstanceType:InstanceType,PublicIpAddress:PublicIpAddress,PrivateIpAddress:PrivateIpAddress}')
    instance_ids=( $(echo $nodes | jq -r '.[].InstanceId') )
  
    # for testing purposes: just query running instances
    # nodes=$(aws ec2 describe-instances --query 'Reservations[*].Instances[*].{InstanceId:InstanceId,InstanceType:InstanceType,PublicIpAddress:PublicIpAddress,PrivateIpAddress:PrivateIpAddress}' --filters "Name=tag-key, Values=clusterID, Name=tag-value, Values=$CLUSTER_ID")
    # instance_ids=( $(echo $nodes | jq -r '.[] | .[].InstanceId') )
    echo "***  adding tags ... ****************************************************"
      for i in "${!instance_ids[@]}"; do
        aws ec2 create-tags --resources ${instance_ids[$i]} --tags Key=clusterID,Value=$CLUSTER_ID
      done
    echo "***  done ***************************************************************"
  echo "***  done ***************************************************************"
  
  echo "***  querying public ips ************************************************"
    echo "  waiting until public IPs are assigned..."
    invalid_ip=999
    while [  $invalid_ip -gt 0 ]; do
      sleep 3
      NODES=$(aws ec2 describe-instances --query 'Reservations[*].Instances[*].{InstanceId:InstanceId,InstanceType:InstanceType,PublicIpAddress:PublicIpAddress,PublicDnsName:PublicDnsName,PrivateIpAddress:PrivateIpAddress}' --filters "Name=tag-key, Values=clusterID, Name=tag-value, Values=$CLUSTER_ID")
      ### example: NODES="[ [ { \"InstanceId\": \"i-0bf319625bdead7f6\", \"InstanceType\": \"t2.micro\", \"PrivateIpAddress\": \"172.31.60.116\", \"PublicIpAddress\": \"52.55.99.0\" } ], [ { \"InstanceId\": \"i-03fbee88bd769e63e\", \"InstanceType\": \"t2.micro\", \"PrivateIpAddress\": \"172.31.22.112\", \"PublicIpAddress\": \"52.90.28.61\" } ] ]"
      PUBLIC_IP_ADDRESSES=( $(echo $NODES | jq -r '.[] | .[].PublicIpAddress') )
      PUBLIC_DNS_NAMES=( $(echo $NODES | jq -r '.[] | .[].PublicDnsName') )
      invalid_ip=0
      nbr_nodes=0
      for ip in "${PUBLIC_IP_ADDRESSES[@]}"; do
        ((nbr_nodes++))
        if [ $ip == null ]; then
          ((invalid_ip++))
        fi
      done
      if [[ "$IS_MASTER" == "master" ]]; then
        if [ $nbr_nodes != 1 ]; then
          echo -e "${RED}ERROR: master needs a unique ID, but $nbr_nodes instances with tag $CLUSTER_ID have been found.${NC}"
          exit -1
        fi
      fi
    done
  
    if [ $nbr_nodes == 0 ]; then
      echo -e "${RED}ERROR: could not find any EC2 instances. Has the configuration file 'config.cfg' been modified appropriatly?${NC}"
      exit -1
    fi
  echo "***  done ***************************************************************"
else
  echo "***  parsing public dns names and IPs... ********************************"
  IFS=',' read -ra INSTANCES <<< "$PREDEFINED_INSTANCES"
  for instance in "${INSTANCES[@]}"; do
    PUBLIC_DNS_NAMES+=("$instance")
    ip=$(echo "$instance" | grep -Eo '\-([0-9]{1,3}\-){3}[0-9]{1,3}' | grep -Eo '([0-9]{1,3}\-){3}[0-9]{1,3}' | sed 's/\-/\./g')
    PUBLIC_IP_ADDRESSES+=("$ip")
  done
  echo "***  done ***************************************************************"
fi


# add this machines public key to template, if not already in file
grep -qe "^`cat ~/.ssh/id_rsa.pub`" ./templates/authorized_keys.template 2>/dev/null || cat ~/.ssh/id_rsa.pub >> ./templates/authorized_keys.template


echo "***  installing and configuring nodes... ********************************"
  echo "  Waiting until nodes have been started and are accessible via ssh."
  echo "  This might take a while. Please be patient."
  for dns_name in "${PUBLIC_DNS_NAMES[@]}"; do
    waitForNode $USERNAME $dns_name
    sshCmd $dns_name "mkdir \$HOME/setup"
    scpCmd $dns_name "./templates/authorized_keys.template ./files-common/initSSH.sh" \$HOME/setup/
    cmd="echo 'set bell-style none' >> ~/.inputrc;"
    cmd+='echo "export PATH=\"\$PATH:/usr/lib64/openmpi/bin\"" >> ~/.bashrc;'
    cmd+='echo "export LD_LIBRARY_PATH=\"\$LD_LIBRARY_PATH:/usr/lib64/openmpi/lib\"" >> ~/.bashrc;'
    if [[ $INSTANCE_TYPE == "t2.micro" ]]; then
      cmd+="sudo yum update -y  >/dev/null 2>&1 && sudo yum install -y redhat-lsb-core kernel-devel-\`uname -r\` openmpi openmpi-devel >/dev/null 2>&1;"
      
      cmd+="wget -q $OPENCL_SILENT_INSTALL -O \$HOME/setup/opencl_runtime_silent.cfg;"
      cmd+="wget -q $OPENCL_INSTALL_REDHAT -O \$HOME/setup/opencl_runtime_x64_rh.tgz;"
      cmd+="tar -zxf \$HOME/setup/opencl_runtime_x64_rh.tgz -C \$HOME/setup/ >/dev/null;"
      cmd+="mv \$HOME/setup/opencl_runtime_*.*_x64_rh_*.* \$HOME/setup/opencl_runtime_x64_rh;"
      # INFORMATION: Warning about 'Missing optional prerequisites' and 'Unsupported OS' can be ignored
      cmd+="sudo \$HOME/setup/opencl_runtime_x64_rh/install.sh --silent \$HOME/setup/opencl_runtime_silent.cfg >/dev/null;"
      cmd+="sudo mkdir -p /usr/include/CL && cd /usr/include/CL && sudo wget -q https://raw.githubusercontent.com/KhronosGroup/OpenCL-Headers/opencl12/{opencl,cl_platform,cl,cl_ext,cl_gl,cl_gl_ext}.h;"
    else
      cmd+='echo "export PATH=\"\$PATH:/usr/local/cuda-7.5/bin\"" >> ~/.bashrc;'
      cmd+="sudo yum update -y && sudo yum install -y openmpi openmpi-devel;"
    fi
    cmd+="\$HOME/setup/initSSH.sh;"
    
    if [[ "$IS_MASTER" == "master" ]]; then
      cmd+="sudo yum install -y gcc gcc-c++ gdb git make graphviz samba jq >/dev/null 2>&1;"
      cmd+="(echo \"$SAMBA_PW\"; echo \"$SAMBA_PW\") | sudo smbpasswd -sa ec2-user;"
      cmd+="mkdir /home/$USERNAME/git;"
      cmd+="echo -e '[git]\npath = /home/$USERNAME/git\navailable = yes\nvalid users = $USERNAME\nread only = no\nbrowseable = yes\npublic = yes\nwritable = yes\n' | sudo tee -a /etc/samba/smb.conf >/dev/null;"
      cmd+="sudo service smb start;"
      cmd+="mkdir /home/$USERNAME/.aws;"
      cmd+="printf \"[default]\nregion = $REGION\n\" | tee -a /home/$USERNAME/.aws/config >/dev/null;"
      cmd+="printf \"[default]\naws_access_key_id = $AWS_ACCESS_KEY_ID\naws_secret_access_key = $AWS_SECRET_ACCESS_KEY\n\" | tee -a /home/$USERNAME/.aws/credentials >/dev/null;"
    fi
    
    sshCmd $dns_name "$cmd" &
  done
  wait
echo "***  done ***************************************************************"


echo "***  writing hostfile... ************************************************"
  date_time=$(date +'%Y%m%d_%H%M%S')
  if [[ -f ./output/mpi.hostfile ]]; then
    mv ./output/mpi.hostfile ./output/mpi.hostfile.$date_time
  fi
  
  ALL_DNS_NAMEs=""
  echo "# Optional: master can host additional workers:" > ./output/mpi.hostfile
  echo "# $HOSTNAME slots=$(( NUM_GPUS + 1 ))" >> ./output/mpi.hostfile
  for dns_name in "${PUBLIC_DNS_NAMES[@]}"; do
    remote_hostname=$(ssh -oStrictHostKeyChecking=no -i $EC2_PEMFILE $USERNAME@$dns_name 'hostname')
    echo "$remote_hostname slots=$NUM_GPUS" >> ./output/mpi.hostfile
    ALL_DNS_NAMEs+="$dns_name,"
  done
echo "***  done ***************************************************************"


if [[ "$IS_MASTER" == "master" ]]; then
  date_time=$(date +'%Y%m%d_%H%M%S')
  if [[ -f ./output/authorized_keys.ec2-master ]]; then
    mv ./output/authorized_keys.ec2-master ./output/authorized_keys.ec2-master.$date_time
  fi
  if [[ -f ./output/external_ip.ec2-master ]]; then
    mv ./output/external_ip.ec2-master ./output/external_ip.ec2-master.$date_time
  fi
  if [[ -f ./output/ssh-config.ec2-master ]]; then
    mv ./output/ssh-config.ec2-master ./output/ssh-config.ec2-master.$date_time
  fi
fi


echo "***  exchanging keys and writing ssh-config... **************************"
  ./configureAccess.sh ec2 $ALL_DNS_NAMEs $EC2_PEMFILE
echo "***  done ***************************************************************"


if [[ "$IS_MASTER" == "master" ]]; then
  echo "***  adding current hosts data to templates... **************************"
  external_ip=${PUBLIC_IP_ADDRESSES[0]}
  external_dns_name=${PUBLIC_DNS_NAMES[0]}
  
  internal_ip=$(ssh -oStrictHostKeyChecking=no -i $EC2_PEMFILE $USERNAME@$external_dns_name "/sbin/ip address show eth0 | grep -Eo 'inet (addr:)?([0-9]*\.){3}[0-9]*' | grep -Eo '([0-9]*\.){3}[0-9]*' | grep -v '127.0.0.1'")
  remote_hostname=$(ssh -oStrictHostKeyChecking=no -i $EC2_PEMFILE $USERNAME@$external_dns_name 'hostname')
  port=$(ssh -oStrictHostKeyChecking=no -i $EC2_PEMFILE $USERNAME@$external_dns_name "grep -Po \"^Port\\s*(\\d*)\" /etc/ssh/sshd_config | grep -Po \"\\d*\"")
  if [[ -z $port ]]; then
    port=22
  fi
  echo -e "Host $external_dns_name $external_ip $internal_ip $remote_hostname\n  StrictHostKeyChecking no\n  User $USERNAME\n  HostName $internal_ip\n  Port $port" > ./output/ssh-config.ec2-master
  
  echo "***  adding public key and master's external ip to templates... *********"
  date_time=$(date +'%Y-%m-%d %H:%M:%S')
  echo "### key for GPU-cluster - EC2-master: ($date_time) ###############################" > ./output/authorized_keys.ec2-master
  cat ~/.ssh/id_rsa.pub >> ./output/authorized_keys.ec2-master
  echo "### key for GPU-cluster - EC2-master: ($date_time) ###############################" >> ./output/authorized_keys.ec2-master
  
  echo "$external_ip" > ./output/external_ip.ec2-master
  
  echo -e "${YELLOW}  map git-folder on master using net use \\\\\\$external_ip\\git /USER:$USERNAME PASSWORD${NC}"
  echo -e "${YELLOW}  connect with ssh using 'ssh $external_ip'${NC}"
  echo -e "${YELLOW}                      or 'ssh $external_dns_name'${NC}"
  echo -e "${YELLOW}                      or 'ssh $internal_ip'${NC}"
  echo -e "${YELLOW}                      or 'ssh $remote_hostname'${NC}"
  
  date_time=$(date +'%Y%m%d_%H%M%S')
  sed -e 's/<<EC2_MASTER_IP_ADDRESS>>/'$external_ip'/g' templates/policy.ec2-master > output/policy.ec2-master.$date_time
  
  list_policies=$(aws iam list-policies --scope Local)
  previous_policy_arn=$(echo $list_policies | jq -r '.Policies | .[] | select(.PolicyName == "mpi_ec2_master") | .Arn')
  if [ ! -z "$previous_policy_arn" ]; then
    aws iam detach-group-policy --group-name mpi --policy-arn $previous_policy_arn
    aws iam delete-policy --policy-arn $previous_policy_arn
  fi
  
  policy=$(aws iam create-policy --policy-name mpi_ec2_master --policy-document file://output/policy.ec2-master.$date_time --query 'Policy.{Arn:Arn}')
  policy_arn=$(echo $policy | jq -r '.Arn')
  aws iam attach-group-policy --group-name $EC2_CLUSTER_GROUP --policy-arn $policy_arn
fi


if [ -f ./output/external_ip.ec2-master ]; then
  master_external_ip=$(cat ./output/external_ip.ec2-master)
  error=$(ssh $master_external_ip "hostname > /dev/null 2>&1" 2>&1)
  if [[ -z "$error" ]]; then
    echo "***  sending mpi.hostfile to ec2-master... ******************************"
    # will not work from one ec2-master to another because fw blocks all traffic except home-network and $EC2_SECURITY_GROUP
    scpCmd $master_external_ip ./output/mpi.hostfile \$HOME/
    echo "***  done ***************************************************************"
  fi
fi

exit 0

