# !/bin/bash
# param 1: optional cluster tag (defaults to 'cluster') to differentiate with your other instances
#
# eg ./awsTerminateInstances.sh GPGPU_cluster

echo "reading configuration..."
if [ -f ./config.cfg ]; then
  source ./config.cfg
else
  echo -e "${RED}ERROR: host-specific configuration file not found: ./config.cfg${NC}"
  exit -1
fi

#######################################################################################################################
### ARGUMENT CHECKING #################################################################################################
#######################################################################################################################
if [[ -z "$1" ]]; then
  CLUSTER_ID=cluster
else
  CLUSTER_ID=$1
fi

#######################################################################################################################
###      ##############################################################################################################
### MAIN ##############################################################################################################
###      ##############################################################################################################
#######################################################################################################################
echo "***  fetching instance-data from aws... *********************************"
  NODES=`aws ec2 describe-instances --query 'Reservations[*].Instances[*].{InstanceId:InstanceId,InstanceType:InstanceType,PrivateIpAddress:PrivateIpAddress,PublicDnsName:PublicDnsName}' --filters "Name=tag-key, Values=clusterID, Name=tag-value, Values=$CLUSTER_ID"`
  INSTANCE_IDS=( $(echo $NODES | jq -r '.[] | .[].InstanceId') )
echo "***  done ***************************************************************"

echo "***  terminating all instances... ***************************************"
  ALL_IDS=
  for INSTANCE in "${INSTANCE_IDS[@]}"; do
    ALL_IDS+="$INSTANCE "
  done
  aws ec2 terminate-instances $DRY_RUN --instance-ids $ALL_IDS
echo "***  done ***************************************************************"

echo "***  Summary ************************************************************"
  echo -e "${YELLOW}  ${#INSTANCE_IDS[@]} instances:${NC}"
  for INSTANCE in "${INSTANCE_IDS[@]}"; do
    echo -e "${YELLOW}    $INSTANCE${NC}"
  done
  echo -e "${YELLOW}  have been terminated.${NC}"
echo "***  done ***************************************************************"
