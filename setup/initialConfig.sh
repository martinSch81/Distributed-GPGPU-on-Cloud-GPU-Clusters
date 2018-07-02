# !/bin/bash
#
# has three parts:
#   install and configure aws on local machine (is interactive)
#   install and configure virtual box on local machine
#   configure EC2 (only necessary and successful once per EC2-user)
#
# call from setup-directory:
# ./awsInstallConfig.sh [ --aws-cli ] [ --vbox ] [ --ec2 ]

if [ "$1" == "--aws-cli" ]; then
  aws_cli=true
elif [ "$1" == "--vbox" ]; then
  vbox=true
elif [ "$1" == "--ec2" ]; then
  ec2=true
else
  echo -e "${RED}call from setup-directory:${NC}"
  echo -e "${RED}./awsInstallConfig.sh [ --aws-cli ] [ --vbox ] [ --ec2 ]${NC}"
  exit
fi


echo "reading configuration..."
if [ -f ./config.cfg ]; then
  source ./config.cfg
else
  echo -e "${RED}ERROR: host-specific configuration file not found: ./config.cfg${NC}"
  exit -1
fi

#######################################################################################################################
###      ##############################################################################################################
### MAIN ##############################################################################################################
###      ##############################################################################################################
#######################################################################################################################
if [ "$aws_cli" == true ]; then
  echo "***  installing and configuring aws client... ***************************"
    sudo apt-get update && sudo apt-get install python-pip jq --assume-yes
    sudo pip install --upgrade pip
    sudo pip install awscli
  
    # gui - non-interactive login
    echo -e "complete -C '/usr/local/bin/aws_completer' aws\nexport PATH=/usr/local/bin/aws:\$PATH\n" | tee -a ~/.bashrc
    # ssh - interactive login (required on ubuntu server only, not debian)
    # echo -e "\n[ -n "$BASH" ] && [ -f ~/.bashrc ] && . ~/.bashrc\n" | tee -a ~/.profile
  
    echo -e "${YELLOW}\
    storing your AWS master-credentials for awscli; interactive configuration\n\
    use\n\
      AWS Access Key ID;\n\
      AWS Secret Access Key;\n\
      Default region name: us-east-1 (or whatever is proper for you);\n\
      Default output format: none (defaults to JSON))\n\
    enter both keys you got while registering\n\
    (http://docs.aws.amazon.com/cli/latest/userguide/cli-chap-getting-set-up.html#cli-signup)\
    ${NC}"
    echo ""
    aws configure
  echo "***  done ***************************************************************"
  exit
fi

if [ "$vbox" == true ]; then
  echo "***  installing and configuring virtual box... **************************"
  if [ ! -f ./OpenCL/opencl_runtime_silent.cfg ]; then
    wget $OPENCL_SILENT_INSTALL -O ./OpenCL/opencl_runtime_silent.cfg
  fi
  if [ ! -f ./OpenCL/opencl_runtime_16.1.1_x64_rh_6.4.0.25.tgz ]; then
    wget $OPENCL_INSTALL_REDHAT -O ./OpenCL/opencl_runtime_16.1.1_x64_rh_6.4.0.25.tgz
  fi
  if [ ! -f ./OpenCL/opencl_runtime_16.1.1_x64_ubuntu_6.4.0.25.tgz ]; then
    wget $OPENCL_INSTALL_UBUNTU -O ./OpenCL/opencl_runtime_16.1.1_x64_ubuntu_6.4.0.25.tgz
  fi
  if [ ! -f ./CUDA/InstallUtils.pm ]; then
    wget $CUDA_INSTALL_PERL -O ./CUDA/InstallUtils.pm
  fi
  if [ ! -f ./CUDA/cuda_8.0.61_375.26_linux.run ]; then
    wget $CUDA_INSTALL_LINUX -O ./CUDA/cuda_8.0.61_375.26_linux.run
  fi
  
  echo "***  done ***************************************************************"
  exit
fi

if [ "$ec2" == true ]; then
  echo "***  configuring aws ec2 environment... ********************************"
    # create security groups for cluster
    echo -e "${YELLOW}\
    if you try to configure EC2 a second time, errors will be reported\n\
    since the configuration for your account already exists\n\
    ${NC}"
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
    aws ec2 create-placement-group $DRY_RUN --group-name $PLACEMENT_GROUP --strategy cluster 1>/dev/null
    
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
      access_key_id=$(echo $access_key | jq -r '.AccessKeyId')
      secret_access_key=$(echo $access_key | jq -r '.SecretAccessKey')

	  echo -e "${YELLOW}\
      Please store the credentials for user: $EC2_CLUSTER_USER in your config.cfg and keep config.cfg secure:\n\
        AWS_ACCESS_KEY_ID=$access_key_id\n\
        AWS_SECRET_ACCESS_KEY=$secret_access_key\n\
      ${NC}"
	fi
	    
    key=$(aws ec2 create-key-pair --key-name "$EC2_CLUSTER_USER-$REGION")
	if [ ! -z "$key" ]; then
      private_key=$(echo $key | jq -r '.KeyMaterial')
      date_time=$(date +'%Y%m%d_%H%M%S')
      if [[ -f ./keys/$EC2_CLUSTER_USER-$REGION.pem ]]; then
        mv ./keys/$EC2_CLUSTER_USER-$REGION.pem ./keys/$EC2_CLUSTER_USER-$REGION.pem.$date_time
      fi
      echo "$private_key" > ./keys/$EC2_CLUSTER_USER-$REGION.pem
	  
      echo -e "${YELLOW}\
      A private key for user $EC2_CLUSTER_USER has been created: ./keys/$EC2_CLUSTER_USER-$REGION.pem\n
      ${NC}"
	fi
  echo "***  done ***************************************************************"

  exit
fi

