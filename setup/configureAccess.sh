# !/bin/bash

#######################################################################################################################
### ARGUMENT CHECKING #################################################################################################
#######################################################################################################################
USAGE="${RED}ERROR: call script with 2 (optional 3) arguments,\n\
       1) type of instance: [ debian | ubuntu | ANY_STRING ] where ANY_STRING defaults to EC2\n\
       2) comma separated list of cluster nodes' public ips\n\
     optional:\n\
       3*) path to pem-file    *...for EC2 only\n\
     \n\
       eg. ./configureAccess.sh debian 192.168.56.20,192.168.56.21\n\
       eg. ./configureAccess.sh EC2 178.168.56.20,178.168.56.21 keys/usWestNVirginia.pem${NC}"

TYPE_OF_INSTANCES=$1
HOSTNAME_LIST=$2
if [[ -z $TYPE_OF_INSTANCES || -z $HOSTNAME_LIST ]]; then
  echo -e $USAGE
  exit -1
fi

echo "reading configuration..."
if [ -f ./config.cfg ]; then
  source ./config.cfg
  echo "...done!"
else
  echo -e "${RED}ERROR: host-specific configuration file not found: ./config.cfg${NC}"
  exit -1
fi

if [[ ! ($TYPE_OF_INSTANCES == "debian" || $TYPE_OF_INSTANCES == "ubuntu") ]]; then
  USERNAME="ec2-user"
  if [[ -z $3 ]]; then
  echo -e $USAGE
    exit -1
  fi
  EC2_PEMFILE=$(readlink -e $3)
fi

#######################################################################################################################
### Functions #########################################################################################################
#######################################################################################################################
function awaitStartup {
  error="error"
  while [[ ! -z "$error" ]]; do
    echo "  waiting for $2..."
    if [[ $1 == "debian" || $1 == "ubuntu" ]]; then
      error=$(ssh -o StrictHostKeyChecking=no $USERNAME@$2 "hostname > /dev/null 2>&1" 2>&1)
    else
      error=$(ssh -o StrictHostKeyChecking=no -i $EC2_PEMFILE $USERNAME@$2 "hostname > /dev/null 2>&1" 2>&1)
    fi
  done
}

#######################################################################################################################
###      ##############################################################################################################
### MAIN ##############################################################################################################
###      ##############################################################################################################
#######################################################################################################################
DATE_TIME=$(date +'%Y-%m-%d %H:%M:%S')

echo "***  prepairing config-files for cluster... *****************************"
  echo "### keys for GPU-cluster: $TYPE_OF_INSTANCES ($DATE_TIME) ####################################" > ./output/authorized_keys.cluster
  echo "### ssh-config for GPU-cluster: $TYPE_OF_INSTANCES ($DATE_TIME) ##############################" > ./output/ssh-config.cluster
  echo "### ssh-config for GPU-cluster: $TYPE_OF_INSTANCES ($DATE_TIME) ##############################" > ./output/ssh-config.master
echo "***  done ***************************************************************"


echo "***  adding current hosts data to templates *****************************"
  if [[ $TYPE_OF_INSTANCES == "debian" || $TYPE_OF_INSTANCES == "ubuntu" ]]; then
    IP=`vboxmanage list hostonlyifs | sed -rn "s/^IPAddress:\s+(.*)/\1/p"`
    if [ -z $IP ]; then
      echo -e "${RED}ERROR: could not parse for local IP address. Please check if the virtual box host-only network adapter '${HOSTONLY_ADAPTER_VBOX}' is correct and it has a valid IP assigned.${NC}"
      exit -1
    fi
    aliases="$IP $HOSTNAME"
  elif [ -f /sys/hypervisor/uuid ] && [ `head -c 3 /sys/hypervisor/uuid` == ec2 ]; then # running on ec2
    IP=$(/sbin/ip address show eth0 | grep -Eo 'inet (addr:)?([0-9]*\.){3}[0-9]*' | grep -Eo '([0-9]*\.){3}[0-9]*' | grep -v '127.0.0.1')
    external_ip=$(dig +short myip.opendns.com @resolver1.opendns.com)
    public_dns_name="ec2-`echo $external_ip | sed -e 's/\./-/g'`.compute-1.amazonaws.com"
  
    aliases="$IP $HOSTNAME $external_ip $public_dns_name"
  else # configuring for ec2, but not running on ec2
    IP=$(dig +short myip.opendns.com @resolver1.opendns.com)
    aliases="$IP $HOSTNAME"
  fi

  port=$(grep -Po "^Port\s*(\d*)" /etc/ssh/sshd_config | grep -Po "\d*")
  if [[ -z $port ]]; then
    port=22
  fi

  echo -e "Host $aliases\n  StrictHostKeyChecking no\n  User $USER\n  HostName $IP\n  Port $port" > ./templates/ssh-config.${HOSTNAME}.template
  
echo "***  done ***************************************************************"

echo "***  collecting public keys and host-ids... *****************************"
IFS=',' read -ra IPs <<< "$HOSTNAME_LIST"
  for ip in "${IPs[@]}"; do
  # param $ip is public dns name for ec2, and internal ip for vbox
    ssh-keygen -R $ip  > /dev/null 2>&1
    awaitStartup $TYPE_OF_INSTANCES $ip
    internal_ip=""
    if [[ $TYPE_OF_INSTANCES == "debian" || $TYPE_OF_INSTANCES == "ubuntu" ]]; then
      ssh -o StrictHostKeyChecking=no $USERNAME@$ip "cat .ssh/id_rsa.pub" >> ./output/authorized_keys.cluster
      internal_ip=$ip
      remote_hostname=$(ssh -o StrictHostKeyChecking=no $USERNAME@$ip 'hostname')
    aliases="$ip $remote_hostname"
    else
      ssh -o StrictHostKeyChecking=no -i $EC2_PEMFILE $USERNAME@$ip "cat .ssh/id_rsa.pub" >> ./output/authorized_keys.cluster
      internal_ip=$(ssh -o StrictHostKeyChecking=no -i $EC2_PEMFILE $USERNAME@$ip "/sbin/ip address show eth0 | grep -Eo 'inet (addr:)?([0-9]*\.){3}[0-9]*' | grep -Eo '([0-9]*\.){3}[0-9]*' | grep -v '127.0.0.1'")
    external_ip=$(ssh -o StrictHostKeyChecking=no -i $EC2_PEMFILE $USERNAME@$ip "dig +short myip.opendns.com @resolver1.opendns.com")
      ssh-keygen -R $internal_ip  > /dev/null 2>&1
      remote_hostname=$(ssh -i $EC2_PEMFILE $USERNAME@$ip 'hostname')
    aliases="$ip $external_ip $internal_ip $remote_hostname"
    fi
    ssh-keygen -R $remote_hostname  > /dev/null 2>&1
  
    echo -e "Host $aliases\n  StrictHostKeyChecking no\n  User $USERNAME\n  HostName $internal_ip\n" >> ./output/ssh-config.cluster
  if [ -f /sys/hypervisor/uuid ] && [ `head -c 3 /sys/hypervisor/uuid` == ec2 ]; then # running on ec2
    echo -e "Host $aliases\n  StrictHostKeyChecking no\n  User $USERNAME\n  HostName $internal_ip\n  ForwardAgent yes\n" >> ./output/ssh-config.master
  else
    echo -e "Host $aliases\n  StrictHostKeyChecking no\n  User $USERNAME\n  HostName $ip\n  ForwardAgent yes\n" >> ./output/ssh-config.master
  fi
  done
echo "***  done ***************************************************************"


echo "***  merging new config with templates from root node... *****************"
  echo "### keys for GPU-cluster: $TYPE_OF_INSTANCES ($DATE_TIME) ####################################" >> ./output/authorized_keys.cluster
  cat ./output/authorized_keys.cluster >> ~/.ssh/authorized_keys
  cat ./templates/authorized_keys.template >> ./output/authorized_keys.cluster
  
  if [[ ! ($TYPE_OF_INSTANCES == "debian" || $TYPE_OF_INSTANCES == "ubuntu") ]]; then
    if [[ -f ./output/authorized_keys.ec2-master && -f ./output/external_ip.ec2-master && -f ./output/ssh-config.ec2-master ]]; then
    MASTER_EXTERNAL_IP=$(cat ./output/external_ip.ec2-master)
    error=$(ssh -o StrictHostKeyChecking=no $MASTER_EXTERNAL_IP "hostname > /dev/null 2>&1" 2>&1)
    if [[ -z "$error" ]]; then
      cat ./output/authorized_keys.ec2-master >> ./output/authorized_keys.cluster
      cat ./output/ssh-config.ec2-master >> ./output/ssh-config.cluster
      IFS=',' read -ra IPs <<< "$HOSTNAME_LIST$MASTER_EXTERNAL_IP"
      else
      date_time=$(date +'%Y%m%d_%H%M%S')
      mv ./output/authorized_keys.ec2-master ./output/authorized_keys.ec2-master.$date_time
      mv ./output/external_ip.ec2-master ./output/external_ip.ec2-master.$date_time
      mv ./output/ssh-config.ec2-master ./output/ssh-config.ec2-master.$date_time
    MASTER_EXTERNAL_IP=""
      fi
  fi
  fi
  
  echo "### keys for GPU-cluster: $TYPE_OF_INSTANCES ($DATE_TIME) ####################################" >> ./output/authorized_keys.cluster
  echo "### ssh-config for GPU-cluster: $TYPE_OF_INSTANCES ($DATE_TIME) ##############################" >> ./output/ssh-config.cluster
  echo "### ssh-config for GPU-cluster: $TYPE_OF_INSTANCES ($DATE_TIME) ##############################" >> ./output/ssh-config.master
  cat ./output/ssh-config.master        >> ~/.ssh/config
  cat ./templates/ssh-config.*.template >> ./output/ssh-config.cluster
  echo "### ssh-config for GPU-cluster: $TYPE_OF_INSTANCES ($DATE_TIME) ##############################" >> ./output/ssh-config.cluster
echo "***  done ***************************************************************"


echo "***  distributing config to each cluster node... ************************"
  for ip in "${IPs[@]}"; do
    scp ./output/authorized_keys.cluster ./output/ssh-config.cluster $USERNAME@$ip:~/setup/
    ssh -o StrictHostKeyChecking=no $USERNAME@$ip "cat ~/setup/authorized_keys.cluster >> ~/.ssh/authorized_keys; cat ~/setup/ssh-config.cluster >> ~/.ssh/config; "
  done
echo "***  done ***************************************************************"

echo "***  listing public IPs *************************************************"
  echo -e "${YELLOW}  your host: ${IP}${NC}"
  if [[ ! -z "$MASTER_EXTERNAL_IP" ]]; then
    echo -e "${YELLOW}  ec2-master: ${ip}${NC}"
  fi
  IFS=',' read -ra IPs <<< "$HOSTNAME_LIST"
  for ip in "${IPs[@]}"; do
    echo -e "${YELLOW}  configured instance: ${ip}${NC}"
  done
echo "***  done ***************************************************************"

