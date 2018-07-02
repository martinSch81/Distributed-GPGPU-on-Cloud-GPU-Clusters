# !/bin/bash

#######################################################################################################################
### ARGUMENT CHECKING #################################################################################################
#######################################################################################################################
USAGE="${RED}ERROR: call script with 4 arguments,\n\
       1) number of instances\n\
       2) starting ip-address for instances (in the same subnet as your vbox hostonly IF (vboxmanage list hostonlyifs))\n\
       3) distribution: [ debian | ubuntu ]\n\
       4) PASSWORD for root in instances\n\
	   \n\
       eg. ./vboxCreateInstances.sh 5 192.168.56.20 debian PASSWORD${NC}"

NBM_OF_INSTANCES=$1
STARTING_IP=$2
DISTRIBUTION=$3
PASSWORD=$4
if [[ -z $NBM_OF_INSTANCES || -z $STARTING_IP || -z $DISTRIBUTION ]]; then
    echo -e $USAGE
    exit -1
fi
if [[ -z $PASSWORD ]]; then
  echo "Please enter the root password of the template:"
  read -s PASSWORD
fi

echo "reading configuration..."
if [ -f ./config.cfg ]; then
  source ./config.cfg
else
  echo -e "${RED}ERROR: host-specific configuration file not found: ./config.cfg${NC}"
  exit -1
fi

if [[ "${3,,}" = "debian" ]]; then
  PREFIX="Debian"
  TEMPLATE_VDI=$TEMPLATE_VDI_DEBIAN
elif [[ "${3,,}" = "ubuntu" ]]; then
  PREFIX="Ubuntu"
  TEMPLATE_VDI=$TEMPLATE_VDI_UBUNTU
else
  echo -e $USAGE
  exit -1
fi


#######################################################################################################################
### COMMON SETTINGS ###################################################################################################
#######################################################################################################################
BASE_FOLDER="$BASE_FOLDER/${PREFIX}64_Cluster"
mkdir -p "$BASE_FOLDER"
BASE_VDI="${BASE_FOLDER}/${PREFIX}_ClusterBase.vdi"
TEMPLATE_VDI="${BASE_FOLDER}/${TEMPLATE_VDI}"
OS_TYPE="${PREFIX}_64"

TEMPLATE_PATH=$(readlink -e ./templates)
OUTPUT_PATH=$(readlink -e ./output)
FILES_COMMON_PATH=$(readlink -e ./files-common)
FILES_DISTRIBUTION_PATH=$(readlink -e ./files-${3,,})


BASE_VDI_ORG=$BASE_VDI
TEMPLATE_VDI_ORG=$TEMPLATE_VDI
# small changes between linux/windows: /mnt/c/ -> c:/; "\ " -> " "
if [[ $( cat /proc/version | grep -iP "Linux\sversion\s(\d+[\.-])*\s*Microsoft" ) ]]; then
  BASE_VDI=$(echo $BASE_VDI | sed -e 's/^\(\/mnt\)\/\([a-zA-Z]\)\//\2:\//g' | sed -e 's/\\ / /g')
  BASE_FOLDER=$(echo $BASE_FOLDER | sed -e 's/^\(\/mnt\)\/\([a-zA-Z]\)\//\2:\//g' | sed -e 's/\\ / /g')
  TEMPLATE_PATH=$(echo $TEMPLATE_PATH | sed -e 's/^\(\/mnt\)\/\([a-zA-Z]\)\//\2:\//g' | sed -e 's/\\ / /g')
  OUTPUT_PATH=$(echo $OUTPUT_PATH | sed -e 's/^\(\/mnt\)\/\([a-zA-Z]\)\//\2:\//g' | sed -e 's/\\ / /g')
  FILES_COMMON_PATH=$(echo $FILES_COMMON_PATH | sed -e 's/^\(\/mnt\)\/\([a-zA-Z]\)\//\2:\//g' | sed -e 's/\\ / /g')
  FILES_DISTRIBUTION_PATH=$(echo $FILES_DISTRIBUTION_PATH | sed -e 's/^\(\/mnt\)\/\([a-zA-Z]\)\//\2:\//g' | sed -e 's/\\ / /g')
  TEMPLATE_VDI=$(echo $TEMPLATE_VDI | sed -e 's/^\(\/mnt\)\/\([a-zA-Z]\)\//\2:\//g' | sed -e 's/\\ / /g')
fi

#######################################################################################################################
### Functions #########################################################################################################
#######################################################################################################################
function awaitStartup {
  IFS=. read ip1 ip2 ip3 ip4 <<< "$2"
  for i in $(seq 1 $1); do
    instance_name="Cluster$ip4"
    error="error"
    while [[ ! -z "$error" ]]; do
	  echo "  waiting for ${instance_name}..."
      error=$(vboxmanage guestcontrol $instance_name --username root --password $PASSWORD run --exe /bin/ls --wait-stdout --wait-stderr -- /bin/ls 2>&1)
      error=$(echo "$error" | grep -v warning | grep error:; )
      sleep 2
    done
	((ip4++))
  done
}

function awaitShutdown {
  IFS=. read ip1 ip2 ip3 ip4 <<< "$2"
  for i in $(seq 1 $1); do
    instance_name="Cluster$ip4"
    error=""
    while [[ -z "$error" ]]; do
	  echo "  waiting for ${instance_name}..."
      error=$(vboxmanage showvminfo $instance_name 2>&1)
      error=$(echo "$error" | grep -P "State:\s*(aborted|powered of)"; )
      sleep 2
    done
	((ip4++))
  done
}
#######################################################################################################################
###      ##############################################################################################################
### MAIN ##############################################################################################################
###      ##############################################################################################################
#######################################################################################################################
DATE_TIME=$(date +'%Y-%m-%d %H:%M:%S')

echo "***  cloning base image... **********************************************"
  eval test_path='$BASE_VDI_ORG'
  if [[ ! -f $test_path  ]]; then
    eval test_path='$TEMPLATE_VDI_ORG'
    if [[ ! -f $test_path ]]; then
	  echo -e "${RED}ERROR: could not find template vdi file: '$TEMPLATE_VDI_ORG'. Terminating now.${NC}"
	  exit -99
	fi
  vboxmanage clonehd "$TEMPLATE_VDI" "$BASE_VDI"
  vboxmanage modifyhd --compact "$BASE_VDI"
    echo -e "\n\n"
  fi
echo "***  done ***************************************************************"

NBR_NODES=0
echo "***  creating instances... **********************************************"
  echo "### hosts for GPU-cluster: $DISTRIBUTION ($DATE_TIME) ###################################" > ./output/hosts.cluster
  IFS=. read ip1 ip2 ip3 ip4 <<< "$STARTING_IP"
  for i in $(seq 1 $NBM_OF_INSTANCES); do
    instance_name="Cluster$ip4"
    error=$(vboxmanage list vms | grep "$instance_name")
    if [[ $error != "" ]]; then
      echo -e "${RED}ERROR: instance '${instance_name}' already exits. Please check or cleanup your instances first. Terminating now.${NC}"
      exit -99
    fi
    vboxmanage createvm --name $instance_name --ostype $OS_TYPE --register --basefolder "$BASE_FOLDER"
    # optional: second, bridged network adapter: vboxmanage modifyvm        $instance_name --groups "/${PREFIX}Cluster" --memory 512 --vram 16 --pae off --cpus 1 --nic1 hostonly --hostonlyadapter1 "$HOSTONLY_ADAPTER_VBOX" --nic2 bridged --bridgeadapter2 "$BRIDGED_ADAPTER"
    vboxmanage modifyvm        $instance_name --groups "/${PREFIX}Cluster" --memory 512 --vram 16 --pae off --cpus 1 --nic1 hostonly --hostonlyadapter1 "$HOSTONLY_ADAPTER_VBOX"
    vboxmanage storagectl      $instance_name --name SATA --add sata
    vboxmanage storageattach   $instance_name --storagectl SATA --port 0 --device 0 --type hdd --medium "$BASE_VDI" --mtype multiattach
    vboxmanage startvm         $instance_name --type headless
    echo ""
 	echo -e "$ip1.$ip2.$ip3.$ip4    cluster$ip4" >> ./output/hosts.cluster
    ((NBR_NODES++))
    ((ip4++))
  done
echo "***  done ***************************************************************"

echo "***  preparing local host entry for workers... **************************"
  IP=`vboxmanage list hostonlyifs | sed -rn "s/^IPAddress:\s+(.*)/\1/p"` ### TODO test this, also in configureAccess.sh
  if [ -z $IP ]; then
    echo -e "${RED}ERROR: could not parse for local IP address. Please check if the virtual box host-only network adapter '${HOSTONLY_ADAPTER_VBOX}' is correct and it has a valid IP assigned.${NC}"
    exit -1
  fi
  echo "### hosts for GPU-cluster: $DISTRIBUTION ($DATE_TIME) ###################################" >> ./output/hosts.cluster
  echo -e "$IP    $HOSTNAME" >> ./output/hosts.cluster
  echo "### hosts for GPU-cluster: $DISTRIBUTION ($DATE_TIME) ###################################" >> ./output/hosts.cluster
echo "***  done ***************************************************************"

echo "Waiting for startup. This will take some time. Please be patient..."
  awaitStartup $NBM_OF_INSTANCES $STARTING_IP
echo "...done!"

# add this machines public key to template, if not already in file
grep -qe "^`cat ~/.ssh/id_rsa.pub`" ./templates/authorized_keys.template 2>/dev/null || cat ~/.ssh/id_rsa.pub >> ./templates/authorized_keys.template

echo "***  configuring instance specific settings... **************************"
  IFS=. read ip1 ip2 ip3 ip4 <<< "$STARTING_IP"
  for i in $(seq 1 $NBM_OF_INSTANCES); do
    instance_name="Cluster$ip4"
    hostname="cluster$ip4"
    ip="$ip1.$ip2.$ip3.$ip4"
	# ATTENTION: virtualbox has problems with stdout/stderr piping when using WSL. Commands need to be kept quiet.
	# ATTENTION: virtualbox 5.2.x has broken copyto cmd. Use any 5.1.x on Windows/WSL instead or copy files in advance in base image.
    vboxmanage guestcontrol    $instance_name --username $USERNAME --password $PASSWORD copyto --target-directory /home/$USERNAME/setup/ "$TEMPLATE_PATH/authorized_keys.template" "$OUTPUT_PATH/hosts.cluster" "$FILES_COMMON_PATH/initSSH.sh" "$FILES_DISTRIBUTION_PATH/setupInstance.sh" 2>/dev/null
    vboxmanage guestcontrol    $instance_name --username root      --password $PASSWORD run --exe /bin/bash  --wait-stdout --no-wait-stderr -- /bin/bash /home/$USERNAME/setup/setupInstance.sh $ip $hostname $USERNAME 2>/dev/null
    vboxmanage guestcontrol    $instance_name --username root      --password $PASSWORD run --exe /bin/chown --wait-stdout --no-wait-stderr -- chown -R $USERNAME /home/$USERNAME/setup/ 2>/dev/null
    vboxmanage guestcontrol    $instance_name --username $USERNAME --password $PASSWORD run --exe /bin/bash  --wait-stdout --no-wait-stderr -- /bin/bash /home/$USERNAME/setup/initSSH.sh 2>/dev/null
    vboxmanage controlvm       $instance_name acpipowerbutton
    ((ip4++))
  done
echo "***  done ***************************************************************"

echo "Waiting for shutdown. This will take some time. Please be patient..."
  awaitShutdown $NBM_OF_INSTANCES $STARTING_IP
echo "...done!"

echo "***  starting nodes again... ********************************************"
  IFS=. read ip1 ip2 ip3 ip4 <<< "$STARTING_IP"
  for i in $(seq 1 $NBM_OF_INSTANCES); do
    instance_name="Cluster$ip4"
    vboxmanage startvm         $instance_name --type headless
	((ip4++))
  done
echo "***  done ***************************************************************"

echo "Waiting for startup. This will take some time. Please be patient..."
  awaitStartup $NBM_OF_INSTANCES $STARTING_IP
echo "...done!"

echo "***  writing hostfile... ************************************************"
  ALL_IPs=""
  date_time=$(date +'%Y%m%d_%H%M%S')
  if [[ -f ./output/mpi.hostfile ]]; then
    mv ./output/mpi.hostfile ./output/mpi.hostfile.$date_time
  fi
  
  IFS=. read ip1 ip2 ip3 ip4 <<< "$STARTING_IP"
  echo "# Optional: master can host additional workers:" > ./output/mpi.hostfile
  echo "# $HOSTNAME slots=$(( NBR_OF_VBOX_CPU_CORES + 1 ))" >> ./output/mpi.hostfile
  for i in $(seq 1 $NBM_OF_INSTANCES); do
    ip="$ip1.$ip2.$ip3.$ip4"
    echo "cluster$ip4 slots=$NBR_OF_VBOX_CPU_CORES" >> ./output/mpi.hostfile
    ALL_IPs+="$ip,"
    ((ip4++))
  done
echo "***  done ***************************************************************"

echo "***  exchanging keys and writing ssh-config... **************************"
  ./configureAccess.sh "${3,,}" $ALL_IPs dummy
echo "***  done ***************************************************************"

exit 0

