# !/bin/bash
# 
# setup script for distributor, containing the following steps
declare -a INSTALL_STEP=(
  "install and configure virtual box"
  "download base image for virtualbox cluster (for development)"
  "create ssh-rsa key"
  "create virtual box cluster"
  "install prerequisites (g++, make, openmpi, openssh-server, tool for dot-graph, dig, jq, opencl-headers)"
  "test mpi"
  "test distributor"
  "setup AWS console"
  "configure AWS console"
  "find valid AWS g2.2xlarge and t2.micro images")
counter=0


echo "reading configuration..."
if [ -f ./config.cfg ]; then
  source ./config.cfg
elif [ -f ./config.template.cfg ]; then
  source ./config.template.cfg
else
  echo -e "${RED}ERROR: host-specific configuration file not found: ./config.template.cfg (or ./config.cfg)${NC}"
  exit -1
fi
echo -e "${GREEN}  Starting setup for DistrutedGPGPU-library. Every installation step asks for execution.\n  In case manual intervention is required, fix the problem, restart the script and skip the previous steps.${NC}"


confirm_installStep() {
  ((item=$1+1))
  read -p "Execute installation step ($item of ${#INSTALL_STEP[@]}) '${INSTALL_STEP[$1]}' (y/n)?  " -r
  while [[ !($REPLY =~ ^[y]$ || $REPLY =~ ^[n]$)]]
  do
    echo -e "${RED}ERROR: invalid input. Please enter 'y' or 'n'.${NC}"
    read -p "Execute installation step $item) (y/n)?  " -r
  done
}



#################################################################################################################
###  install and configure virtual box  #########################################################################
#################################################################################################################
if [[ $counter -le 0 ]]; then
  confirm_installStep $counter
  if [[ $REPLY =~ ^[y]$ ]]; then
    rc=0
    MISSING=""
    command -v vboxmanage  >/dev/null 2>&1 || { MISSING="${MISSING}virtualbox virtualbox-guest-additions-iso "; }
    if [[ ! -z "$MISSING" ]]; then
      echo -e "${YELLOW}The following tools are required and need to be installed: $MISSING${NC}"
      sudo apt install $MISSING --assume-yes
      ((rc |= $?))
    fi
    
    NEW_HOSTONLY_ADAPTER_VBOX=`vboxmanage list hostonlyifs | sed -rn "s/^Name:\s+(.*)/\1/p"`
    if [ -z $NEW_HOSTONLY_ADAPTER_VBOX ]; then
      echo -e "${YELLOW}  configuring a host-only network for Virtual Box...${NC}"
      vboxmanage hostonlyif create
      ((rc |= $?))
      NEW_HOSTONLY_ADAPTER_VBOX=`vboxmanage list hostonlyifs | sed -rn "s/^Name:\s+(.*)/\1/p"`
      if [ -z $NEW_HOSTONLY_ADAPTER_VBOX ]; then
        echo -e "${RED}ERROR: could not create the host-only network, rc=$rc${NC}"
        exit $rc
      fi
    fi
    NEW_HOSTONLY_ADAPTER_VBOX_IP=`vboxmanage list hostonlyifs | sed -rn "s/^IPAddress:\s+(.*)/\1/p"`
    
    echo "  vbox hostonly IF: $NEW_HOSTONLY_ADAPTER_VBOX, IP: $NEW_HOSTONLY_ADAPTER_VBOX_IP"
    echo "  writing changes to configuration file (if not already done)"
    if [ ! -f ./config.cfg ]; then
      cp ./config.template.cfg ./config.cfg
      ((rc |= $?))
    fi
    sed -i -e 's/HOSTONLY_ADAPTER_VBOX=""/HOSTONLY_ADAPTER_VBOX="'${NEW_HOSTONLY_ADAPTER_VBOX}'"/g' ./config.cfg
    ((rc |= $?))

    if [ $rc != 0 ]; then
      echo -e "${RED}ERROR: failed with rc=$rc${NC}"
      exit $rc
    fi
  fi
  REPLY=""
  ((counter++))
fi

#################################################################################################################
###  download base image for virtualbox cluster (for development)  ##############################################
#################################################################################################################
if [[ $counter -le 1 ]]; then
  confirm_installStep $counter
  if [[ $REPLY =~ ^[y]$ ]]; then
    rc=0

    MISSING=""
    command -v wget  >/dev/null 2>&1 || { MISSING="${MISSING}wget "; }
    command -v unzip >/dev/null 2>&1 || { MISSING="${MISSING}unzip "; }
    if [[ ! -z "$MISSING" ]]; then
      echo -e "${YELLOW}The following tools are required and need to be installed: $MISSING${NC}"
      sudo apt install $MISSING --assume-yes
      ((rc |= $?))
    fi

    DESTINATION_DIR="$BASE_FOLDER/Debian64_Cluster"
    DESTINATION_FILE="Debian_ClusterBase.vdi"
    DOWNLOAD_URL=$DEBIAN_BASE_IMAGE_URL
    TMP_FILE="/tmp/Debian_ClusterBase.zip"
    mkdir -pv "$DESTINATION_DIR"
    ((rc |= $?))
    if [ ! -f "$DESTINATION_DIR/$DESTINATION_FILE" ]; then
      wget $DOWNLOAD_URL -q --tries=1 --show-progress --timeout=10 -O $TMP_FILE
      ((rc |= $?))
      echo "  extracting zip to '$DESTINATION_DIR/' (this may take a while)..."
      unzip $TMP_FILE -d "$DESTINATION_DIR/"
      ((rc |= $?))
      vboxmanage modifymedium disk "$DESTINATION_DIR/$DESTINATION_FILE" --type=multiattach
      ((rc |= $?))
    fi
    DESTINATION_DIR="$BASE_FOLDER/Ubuntu64_Cluster"
    DESTINATION_FILE="Ubuntu_ClusterBase.vdi"
    DOWNLOAD_URL=$UBUNTU_BASE_IMAGE_URL
    TMP_FILE="/tmp/Ubuntu_ClusterBase.zip"
    mkdir -pv "$DESTINATION_DIR"
    ((rc |= $?))
    if [ ! -f "$DESTINATION_DIR/$DESTINATION_FILE" ]; then
      wget $DOWNLOAD_URL -q --tries=1 --show-progress --timeout=10 -O $TMP_FILE
      ((rc |= $?))
      echo "  extracting zip to '$DESTINATION_DIR/' (this may take a while)..."
      unzip $TMP_FILE -d "$DESTINATION_DIR/"
      ((rc |= $?))
      vboxmanage modifymedium disk "$DESTINATION_DIR/$DESTINATION_FILE" --type=multiattach
      ((rc |= $?))
    fi

    if [ $rc != 0 ]; then
      echo -e "${RED}ERROR: failed with rc=$rc${NC}"
      exit $rc
    fi
  fi
  REPLY=""
  ((counter++))
fi

#################################################################################################################
###  create ssh-rsa key  ########################################################################################
#################################################################################################################
if [[ $counter -le 2 ]]; then
  confirm_installStep $counter
  if [[ $REPLY =~ ^[y]$ ]]; then
    if [ ! -f "${HOME}/.ssh/id_rsa.pub" ]; then
      rc=0
      ssh-keygen -t rsa -f ${HOME}/.ssh/id_rsa -N ""
      ((rc |= $?))
      
      if [ $rc != 0 ]; then
        echo -e "${RED}ERROR: failed with rc=$rc${NC}"
        exit $rc
      fi
    else
      echo -e "${YELLOW}WARNING: found an RSA-key in '${HOME}/.ssh'. Setup will not overwrite this one.${NC}"
    fi
  fi
  REPLY=""
  ((counter++))
fi

#################################################################################################################
###  create virtual box cluster  ################################################################################
#################################################################################################################
if [[ $counter -le 3 ]]; then
  confirm_installStep $counter
  if [[ $REPLY =~ ^[y]$ ]]; then
    rc=0
    
    read -p "Number of instances to be created: " -r
    CLUSTER_NBR_WORKERS=$REPLY
    read -p "starting ip-address for instances (in the same subnet as your vbox hostonly IF (vboxmanage list hostonlyifs)), e.g. 192.168.56.20 " -r
    CLUSTER_FIRST_IP=$REPLY
    read -p "distribution: [ debian | ubuntu ]: " -r
    CLUSTER_DISTRI=$REPLY
    read -p "PASSWORD for root in instances (default: 'mpi'): " -r -s
    CLUSTER_PW=$REPLY
    echo ""
    
    echo "executing ./vboxCreateInstances.sh $CLUSTER_NBR_WORKERS $CLUSTER_FIRST_IP $CLUSTER_DISTRI"
    ./vboxCreateInstances.sh $CLUSTER_NBR_WORKERS $CLUSTER_FIRST_IP $CLUSTER_DISTRI $CLUSTER_PW
    
    echo "transfering the cluster's mpi.hostfile to $CLUSTER_FIRST_IP..."
    scp ./output/mpi.hostfile $CLUSTER_FIRST_IP:~/ > /dev/null
    ((rc |= $?))

    if [ $rc != 0 ]; then
      echo -e "${RED}ERROR: failed with rc=$rc${NC}"
      exit $rc
    fi
  fi
  REPLY=""
  ((counter++))
fi

#################################################################################################################
###  install prerequisites (g++, make, openmpi, openssh-server, tool for dot-graph, dig, jq, opencl-headers)  ###
#################################################################################################################
if [[ $counter -le 4 ]]; then
  confirm_installStep $counter
  if [[ $REPLY =~ ^[y]$ ]]; then
    rc=0

    MISSING=""
    command -v make            >/dev/null 2>&1 || { MISSING="${MISSING}make "; }
    command -v g++             >/dev/null 2>&1 || { MISSING="${MISSING}g++ "; }
    command -v dot             >/dev/null 2>&1 || { MISSING="${MISSING}graphviz "; }
    if [ ! -f /usr/include/CL/cl.h ];          then MISSING="${MISSING}opencl-headers "; fi
    command -v mpirun          >/dev/null 2>&1 || { MISSING="${MISSING}openmpi-bin libopenmpi2 libopenmpi-dev "; } # for ubuntu 16.40, use 'libopenmpi1.10' instead of 'libopenmpi2'
    command -v jq              >/dev/null 2>&1 || { MISSING="${MISSING}jq "; }
    command -v dig             >/dev/null 2>&1 || { MISSING="${MISSING}dnsutils "; }
    command -v ssh             >/dev/null 2>&1 || { MISSING="${MISSING}ssh "; }
    if [[ ! -z "$MISSING" ]]; then
      echo -e "${YELLOW}The following tools are required and need to be installed: $MISSING${NC}"
      sudo apt install $MISSING --assume-yes
      ((rc |= $?))
    fi

    if [ $rc != 0 ]; then
      echo -e "${RED}ERROR: failed with rc=$rc${NC}"
      exit $rc
    fi
  fi
  REPLY=""
  ((counter++))
fi

#################################################################################################################
###  test mpi  ##################################################################################################
#################################################################################################################
if [[ $counter -le 5 ]]; then
  confirm_installStep $counter
  if [[ $REPLY =~ ^[y]$ ]]; then
    rc=0
    
    if [[ -z "$CLUSTER_FIRST_IP" ]]; then
      read -p "What is the IP of the first cluster instance? " -r
      CLUSTER_FIRST_IP=$REPLY
    fi
    IFS=. read ip1 ip2 ip3 CLUSTER_FIRST_IP_SEGMENT <<< "$CLUSTER_FIRST_IP"
    if [[ -z "$CLUSTER_NBR_WORKERS" ]]; then
      read -p "How many cluster instances have been configured? " -r
      CLUSTER_NBR_WORKERS=$REPLY
    fi
    
    echo "  testing ssh connection host->cluster instances..."
    ip4=$CLUSTER_FIRST_IP_SEGMENT
    for i in $(seq 1 $CLUSTER_NBR_WORKERS); do
      ssh_test=$(ssh cluster$ip4 hostname 2>/dev/null)
      ((rc |= $?))
      if [[ ! "$ssh_test" == "cluster$ip4" ]]; then
        echo -e "${RED}ERROR: could not connect from host to 'cluster$ip4' using 'ssh cluster$ip4 hostname'. Expected 'cluster$ip4' as result but got '$ssh_test'${NC}"
        exit $rc
      fi
      ((ip4++))
    done

    echo "  testing ssh connection cluster instances->host..."
    ip4=$CLUSTER_FIRST_IP_SEGMENT
    for i in $(seq 1 $CLUSTER_NBR_WORKERS); do
      ssh_test=$(ssh cluster$ip4 "ssh $HOSTNAME hostname 2>/dev/null")
      ((rc |= $?))
      if [[ ! "$ssh_test" == "$HOSTNAME" ]]; then
        echo -e "${RED}ERROR: could not connect from 'cluster$ip4' to host using 'ssh cluster$ip4 'ssh $HOSTNAME hostname''. Expected '$HOSTNAME' as result but got '$ssh_test'${NC}"
        exit $rc
      fi
      ((ip4++))
    done

    echo "  testing ssh connection cluster instances->cluster instances..."
    ip4_source=$CLUSTER_FIRST_IP_SEGMENT
    for i in $(seq 1 $CLUSTER_NBR_WORKERS); do
      ip4_target=$CLUSTER_FIRST_IP_SEGMENT
      for j in $(seq 1 $CLUSTER_NBR_WORKERS); do
        ssh_test=$(ssh cluster$ip4_source "ssh cluster$ip4_target hostname 2>/dev/null")
        ((rc |= $?))
        if [[ ! "$ssh_test" == "cluster$ip4_target" ]]; then
          echo -e "${RED}ERROR: could not connect from 'cluster$ip4_source' to 'cluster$ip4_target' using 'ssh cluster$ip4_source 'ssh cluster$ip4_target hostname''. Expected 'cluster$ip4_target' as result but got '$ssh_test'${NC}"
          exit $rc
        fi
        ((ip4_target++))
      done
      ((ip4_source++))
    done
    
    ip4=$CLUSTER_FIRST_IP_SEGMENT
    echo "transfering the cluster's mpi.hostfile to $CLUSTER_FIRST_IP..."
    scp ./output/mpi.hostfile cluster$CLUSTER_FIRST_IP_SEGMENT:~/ > /dev/null
    ((rc |= $?))
    mpi_test=$(ssh cluster$CLUSTER_FIRST_IP_SEGMENT "mpirun --hostfile ~/mpi.hostfile hostname")
    for i in $(seq 1 $CLUSTER_NBR_WORKERS); do
      echo $mpi_test | grep cluster$ip4 >/dev/null
      if [[ $? != 0 ]]; then
        ((rc |= $?))
        echo -e "${RED}ERROR: command 'mpirun --hostfile ~/mpi.hostfile hostname' on 'cluster$CLUSTER_FIRST_IP_SEGMENT' should have collected all instances' hostname, but 'cluster$ip4' has not been found in output '$mpi_test'${NC}"
        exit $rc
      fi
      ((ip4++))
    done
    
    if [ $rc != 0 ]; then
      echo -e "${RED}ERROR: failed with rc=$rc${NC}"
      exit $rc
    fi
  fi
  REPLY=""
  ((counter++))
fi

#################################################################################################################
###  test distributor  ##########################################################################################
#################################################################################################################
if [[ $counter -le 6 ]]; then
  confirm_installStep $counter
  if [[ $REPLY =~ ^[y]$ ]]; then
    rc=0

    if [[ -z "$CLUSTER_FIRST_IP" ]]; then
      read -p "What is the IP of the first cluster instance? " -r
      CLUSTER_FIRST_IP=$REPLY
    fi
    IFS=. read ip1 ip2 ip3 CLUSTER_FIRST_IP_SEGMENT <<< "$CLUSTER_FIRST_IP"
    
    make -C ../code/distributor
    ((rc |= $?))
    scp ../code/distributor/sampleMMul-simple ../code/distributor/Makefile ./output/mpi.hostfile cluster$CLUSTER_FIRST_IP_SEGMENT:
    ((rc |= $?))
    ssh cluster$CLUSTER_FIRST_IP_SEGMENT 'slots=$(sed -rn "s/^$HOSTNAME\s+slots=([0-9]*)$/\1/p" mpi.hostfile); sed -ie "s/\($HOSTNAME\s\s*slots=\)$slots$/\1$((slots+1))/g" mpi.hostfile;'
    ((rc |= $?))
    ssh cluster$CLUSTER_FIRST_IP_SEGMENT "mpirun --n 1 ./sampleMMul-simple N=727" | grep "results verified, results are correct"
    ((rc |= $?))

    if [ $rc != 0 ]; then
      echo -e "${RED}ERROR: failed with rc=$rc${NC}"
      exit $rc
    fi
  fi
  REPLY=""
  ((counter++))
fi

#################################################################################################################
###  setup AWS console  #########################################################################################
#################################################################################################################
if [[ $counter -le 7 ]]; then
  confirm_installStep $counter
  if [[ $REPLY =~ ^[y]$ ]]; then
    rc=0
    
    MISSING=""
    command -v pip  >/dev/null 2>&1 || { MISSING="${MISSING}python-pip "; }
    command -v jq   >/dev/null 2>&1 || { MISSING="${MISSING}jq "; }
    command -v dig             >/dev/null 2>&1 || { MISSING="${MISSING}dnsutils "; }
    if [[ ! -z "$MISSING" ]]; then
      echo -e "${YELLOW}The following tools are required and need to be installed: $MISSING${NC}"
      sudo apt install $MISSING --assume-yes
      ((rc |= $?))
    fi

    sudo -H pip install --upgrade pip
    ((rc |= $?))
    sudo -H pip install awscli
    ((rc |= $?))

    # adding a little more comfort...
    grep -e "^complete -C '/usr/local/bin/aws_completer' aws$" ~/.bashrc >/dev/null || echo -e "\ncomplete -C '/usr/local/bin/aws_completer' aws" | tee -a ~/.bashrc >/dev/null
    ((rc |= $?))
    grep -e "^export PATH=/usr/local/bin/aws:\$PATH$" ~/.bashrc >/dev/null || echo -e "export PATH=/usr/local/bin/aws:\$PATH\n" | tee -a ~/.bashrc >/dev/null
    ((rc |= $?))

    if [ $rc != 0 ]; then
      echo -e "${RED}ERROR: failed with rc=$rc${NC}"
      exit $rc
    fi
  fi
  REPLY=""
  ((counter++))
fi

#################################################################################################################
###  configure AWS console  #####################################################################################
#################################################################################################################
if [[ $counter -le 8 ]]; then
  confirm_installStep $counter
  if [[ $REPLY =~ ^[y]$ ]]; then
    rc=0
    
    mkdir -pv ${HOME}/.aws
    ((rc |= $?))
    REPLY="no_configuration_found"
    if [[ -f ${HOME}/.aws/config || -f ${HOME}/.aws/credentials ]]; then
      echo -e "${YELLOW}WARNING: aws-cli credentials found in ~/.aws. Do you want to overwrite it?${NC}"
      read -p "  (y/n)?  " -r
      while [[ !($REPLY =~ ^[y]$ || $REPLY =~ ^[n]$)]]
      do
        echo -e "${RED}ERROR: invalid input. Please enter 'y' or 'n'.${NC}"
        read -p "  (y/n)?  " -r
      done
      if [[ $REPLY =~ ^[y]$ ]]; then
        ls ${HOME}/.aws/config      >/dev/null 2>&1 && mv -f ${HOME}/.aws/config      ${HOME}/.aws/config.bak
        ((rc |= $?))
        ls ${HOME}/.aws/credentials >/dev/null 2>&1 && mv -f ${HOME}/.aws/credentials ${HOME}/.aws/credentials.bak
        ((rc |= $?))
      fi
    fi

    if [[ $REPLY =~ ^[y]$ || "$REPLY" == "no_configuration_found" ]]; then
      echo -e "${YELLOW}\
      Your AWS master-credentials for awscli need to be stored in ~/.aws\n\
      Please enter your\n\
        AWS Access Key ID;\n\
        AWS Secret Access Key;\n\
        Default region name: us-east-1 (or whatever is proper for you);\n\
      You should have received this keys during your AWS registration\n\
      (http://docs.aws.amazon.com/cli/latest/userguide/cli-chap-getting-set-up.html#cli-signup)\
      ${NC}"
      echo ""
      read -p "AWS Access Key ID: " -r
      NEW_AWS_ACCESS_KEY_ID=$REPLY
      read -p "AWS Secret Access Key: " -r
      NEW_AWS_SECRET_ACCESS_KEY=$REPLY
      read -p "Default region name (press enter for 'us-east-1'): " -r
      NEW_REGION=$REPLY
      if [[ -z "$NEW_REGION" ]]; then
        NEW_REGION="us-east-1"
      fi
      REGION=$NEW_REGION
    
      echo -e "[default]\nregion = $NEW_REGION" | tee ${HOME}/.aws/config >/dev/null
      ((rc |= $?))
      chmod 600 ${HOME}/.aws/config
      echo -e "[default]\naws_access_key_id = $NEW_AWS_ACCESS_KEY_ID\naws_secret_access_key = $NEW_AWS_SECRET_ACCESS_KEY" | tee ${HOME}/.aws/credentials >/dev/null
      ((rc |= $?))
      chmod 600 ${HOME}/.aws/credentials
    else
      REGION=$(sed -ne 's/region\s*=\s*//p' ${HOME}/.aws/config)
    fi
    
    echo "configuring aws ec2..."
    # create security groups for cluster
    echo -e "${YELLOW}WARNING: if you try to configure EC2 a second time, errors will be reported since the configuration for your account already exists. Do you want to cleanup any previous configuration?${NC}"
    read -p "  (y/n)?  " -r
    while [[ !($REPLY =~ ^[y]$ || $REPLY =~ ^[n]$)]]
    do
      echo -e "${RED}ERROR: invalid input. Please enter 'y' or 'n'.${NC}"
      read -p "  (y/n)?  " -r
    done
    if [[ $REPLY =~ ^[y]$ ]]; then
      if [[ ! -z $(aws ec2 describe-security-groups --group-names $EC2_SECURITY_GROUP 2>/dev/null) ]]; then echo "  removing security group '$EC2_SECURITY_GROUP'..."; aws ec2 delete-security-group --group-name $EC2_SECURITY_GROUP; fi
      if [[ ! -z $(aws ec2 describe-placement-groups --group-names $EC2_PLACEMENT_GROUP 2>/dev/null) ]]; then echo "  removing placement group '$EC2_PLACEMENT_GROUP'..."; aws ec2 delete-placement-group --group-name $EC2_PLACEMENT_GROUP; fi
      if [[ ! -z $(aws iam get-policy --group-names $EC2_SECURITY_GROUP 2>/dev/null) ]]; then echo "  removing security group..."; aws ec2 delete-security-group --group-name $EC2_SECURITY_GROUP; fi
      
      
      access_key=$(aws iam list-access-keys --user-name $EC2_CLUSTER_USER 2>/dev/null)
      access_key_id=$(echo $access_key | jq -r '.[]' | jq -r '.[].AccessKeyId')
      if [[ ! -z "$access_key_id" ]]; then echo "  removing iam-users access key..."; aws iam delete-access-key --user-name $EC2_CLUSTER_USER --access-key-id $access_key_id; fi
      aws iam remove-user-from-group --group-name $EC2_CLUSTER_GROUP --user-name $EC2_CLUSTER_USER 2>/dev/null
      if [[ ! -z $(aws iam get-user --user-name $EC2_CLUSTER_USER 2>/dev/null) ]]; then echo "  removing iam-user '$EC2_CLUSTER_USER'..."; aws iam delete-user --user-name $EC2_CLUSTER_USER; fi
      
      group_policy=$(aws iam list-attached-group-policies --group-name $EC2_CLUSTER_GROUP 2>/dev/null)
      policy_arn=$(echo $group_policy | jq -r '.[]' | jq -r '.[].PolicyArn')
      if [[ ! -z "$policy_arn" ]]; then echo "  detaching iam-policy from group $EC2_CLUSTER_GROUP..."; aws iam detach-group-policy --group-name $EC2_CLUSTER_GROUP --policy-arn $policy_arn; fi
      
      if [[ ! -z $(aws iam get-group --group-name $EC2_CLUSTER_GROUP 2>/dev/null) ]]; then echo "  removing iam-group '$EC2_CLUSTER_GROUP'..."; aws iam delete-group --group-name $EC2_CLUSTER_GROUP; fi
      ### if [[ ! -z $(aws iam delete-group-policy --group-name $EC2_CLUSTER_GROUP --policy-name $EC2_POLICY_NAME 2>/dev/null) ]]; then echo "  removing iam-group-policy '$EC2_POLICY_NAME'..."; aws iam delete-group-policy --group-name $EC2_CLUSTER_GROUP --policy-name $EC2_POLICY_NAME; fi
      if [[ ! -z "$policy_arn" ]]; then echo "  removing iam-policy '$EC2_CLUSTER_GROUP'..."; aws iam delete-policy --policy-arn $policy_arn; fi
      
      if [[ ! -z $(aws ec2 describe-key-pairs --key-name ${EC2_CLUSTER_USER}-${REGION} 2>/dev/null) ]]; then echo "  removing key pair '${EC2_CLUSTER_USER}-${REGION}'..."; aws ec2 delete-key-pair --key-name ${EC2_CLUSTER_USER}-${REGION}; fi
      echo "AWS cleanup completed."
    fi

    IP=$(dig +short myip.opendns.com @resolver1.opendns.com)
    echo -e "${YELLOW}We will restrict incoming SSH/SMB/NetBT ports on EC2. Your external IP has been detected as $IP. Do you want to restrict the firewall on a larger subnet?${NC}"
    read -p "Your default external IP/subnet (press enter to use '$IP/32'): " -r
    EXTERNAL_HOME_SUBNET=$REPLY
    if [[ -z "$EXTERNAL_HOME_SUBNET" ]]; then
      EXTERNAL_HOME_SUBNET="$IP/32"
    fi

    security_group=$(aws ec2 create-security-group $DRY_RUN --group-name $EC2_SECURITY_GROUP --description "enable MPI within internal subnet")
    security_group_id=$(echo $security_group | jq -r '.GroupId')

    if [ ! -z "$security_group_id" ]; then
      # ssh
      aws ec2 authorize-security-group-ingress $DRY_RUN --group-name $EC2_SECURITY_GROUP --protocol tcp --port 22 --cidr $EXTERNAL_HOME_SUBNET 1>/dev/null
      aws ec2 authorize-security-group-ingress $DRY_RUN --group-name $EC2_SECURITY_GROUP --protocol tcp --port 22 --source-group $security_group_id 1>/dev/null
      # mpi
      aws ec2 authorize-security-group-ingress $DRY_RUN --group-name $EC2_SECURITY_GROUP --protocol tcp --port 1024-64511 --source-group $security_group_id 1>/dev/null
      # samba
      aws ec2 authorize-security-group-ingress $DRY_RUN --group-name $EC2_SECURITY_GROUP --protocol tcp --port 139 --cidr $EXTERNAL_HOME_SUBNET 1>/dev/null
      aws ec2 authorize-security-group-ingress $DRY_RUN --group-name $EC2_SECURITY_GROUP --protocol tcp --port 445 --cidr $EXTERNAL_HOME_SUBNET 1>/dev/null
      aws ec2 authorize-security-group-ingress $DRY_RUN --group-name $EC2_SECURITY_GROUP --protocol udp --port 137-138 --cidr $EXTERNAL_HOME_SUBNET 1>/dev/null
    fi

    # create placement group to launch cluster nodes into
    aws ec2 create-placement-group $DRY_RUN --group-name $EC2_PLACEMENT_GROUP --strategy cluster 1>/dev/null
 
    policy=$(aws iam create-policy --policy-name $EC2_POLICY_NAME --policy-document file://templates/sample-policy.json --query 'Policy.{Arn:Arn}')
    policy_arn=$(echo $policy | jq -r '.Arn')
    aws iam create-group --group-name $EC2_CLUSTER_GROUP 1>/dev/null
    if [ ! -z "$policy_arn" ]; then
      aws iam attach-group-policy --group-name $EC2_CLUSTER_GROUP --policy-arn $policy_arn
    fi

    aws iam create-user --user-name $EC2_CLUSTER_USER 1>/dev/null
    aws iam add-user-to-group --group-name $EC2_CLUSTER_GROUP --user-name $EC2_CLUSTER_USER 1>/dev/null
    access_key=$(aws iam create-access-key --user-name $EC2_CLUSTER_USER --query 'AccessKey.{SecretAccessKey:SecretAccessKey,AccessKeyId:AccessKeyId}')
    if [ ! -z "$access_key" ]; then
      NEW_AWS_ACCESS_KEY_ID=$(echo $access_key | jq -r '.AccessKeyId')
      NEW_AWS_SECRET_ACCESS_KEY=$(echo $access_key | jq -r '.SecretAccessKey')
      NEW_AWS_SECRET_ACCESS_KEY=$(echo $NEW_AWS_SECRET_ACCESS_KEY | sed -e 's/\\/\\\\/g')
      NEW_AWS_SECRET_ACCESS_KEY=$(echo $NEW_AWS_SECRET_ACCESS_KEY | sed -e 's/\//\\\//g')
    
      if [ ! -f ./config.cfg ]; then
        cp ./config.template.cfg ./config.cfg
        ((rc |= $?))
      fi
      sed -i -e 's/REGION=""/REGION="'${REGION}'"/g' ./config.cfg
      ((rc |= $?))
      sed -i -e 's/AWS_ACCESS_KEY_ID=""/AWS_ACCESS_KEY_ID="'${NEW_AWS_ACCESS_KEY_ID}'"/g' ./config.cfg
      ((rc |= $?))
      sed -i -e 's/AWS_SECRET_ACCESS_KEY=""/AWS_SECRET_ACCESS_KEY="'${NEW_AWS_SECRET_ACCESS_KEY}'"/g' ./config.cfg
      ((rc |= $?))
    fi
    
    key=$(aws ec2 create-key-pair --key-name "$EC2_CLUSTER_USER-$REGION")
    if [ ! -z "$key" ]; then
      private_key=$(echo $key | jq -r '.KeyMaterial')
      date_time=$(date +'%Y%m%d_%H%M%S')
      mkdir -pv ./keys
      if [[ -f ./keys/$EC2_CLUSTER_USER-$REGION.pem ]]; then
        mv ./keys/$EC2_CLUSTER_USER-$REGION.pem ./keys/$EC2_CLUSTER_USER-$REGION.pem.$date_time
      fi
      echo "$private_key" > ./keys/$EC2_CLUSTER_USER-$REGION.pem
      
    fi
    echo -e "${YELLOW}The credentials for user: '$EC2_CLUSTER_USER' will be stored in your config.cfg and a private key for user $EC2_CLUSTER_USER has been created: ./keys/$EC2_CLUSTER_USER-$REGION.pem.\nPlease keep these files secure.${NC}"

    if [ $rc != 0 ]; then
      echo -e "${RED}ERROR: failed with rc=$rc${NC}"
      exit $rc
    fi
  fi
  REPLY=""
  ((counter++))
fi

#################################################################################################################
###  find valid AWS g2.2xlarge and t2.micro images  #############################################################
#################################################################################################################
if [[ $counter -le 9 ]]; then
  confirm_installStep $counter
  if [[ $REPLY =~ ^[y]$ ]]; then
    rc=0
    
    
    T2_MICRO_IMAGE=$(aws ec2 describe-images --filters Name=description,Values="Amazon Linux AMI 20* x86_64 Graphics HVM EBS" --query 'Images[*].[ImageId,CreationDate]' --output text | sort -k2 -r | head -n1)
    ((rc |= $?))
    T2_MICRO_IMAGE=$(echo $T2_MICRO_IMAGE | sed -rn 's/(ami-[^\s]+)\s.*/\1/p')
    ((rc |= $?))
    
    if [[ ! -z "$T2_MICRO_IMAGE" ]]; then
      if [ ! -f ./config.cfg ]; then
        cp ./config.template.cfg ./config.cfg
        ((rc |= $?))
      fi
      
      sed -i -e 's/^# t2.micro IMAGE_ID=".*"$/# t2.micro IMAGE_ID="'${T2_MICRO_IMAGE}'"/g' ./config.cfg
      ((rc |= $?))
      echo -e "${YELLOW}Found a t2.micro image: ${T2_MICRO_IMAGE}${NC}"
    fi

    G2_2XLARGE_IMAGE=$(aws ec2 describe-images --filters Name=description,Values="Amazon Linux AMI 20* x86_64 HVM GP2" --query 'Images[*].[ImageId,CreationDate]' --output text | sort -k2 -r | head -n1)
    ((rc |= $?))
    G2_2XLARGE_IMAGE=$(echo $G2_2XLARGE_IMAGE | sed -rn 's/(ami-[^\s]+)\s.*/\1/p')
    ((rc |= $?))

    if [[ ! -z "$G2_2XLARGE_IMAGE" ]]; then
      if [ ! -f ./config.cfg ]; then
        cp ./config.template.cfg ./config.cfg
        ((rc |= $?))
      fi
      
      sed -i -e 's/^IMAGE_ID=".*"$/IMAGE_ID="'${G2_2XLARGE_IMAGE}'"/g' ./config.cfg
      ((rc |= $?))
      echo -e "${YELLOW}Found a g2.2xlarge image: ${G2_2XLARGE_IMAGE}${NC}"
    fi
    
    if [ $rc != 0 ]; then
      echo -e "${RED}ERROR: failed with rc=$rc${NC}"
      exit $rc
    fi
  fi
  REPLY=""
  ((counter++))
fi

echo -e "${GREEN}  All done.${NC}"
