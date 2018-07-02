# !/bin/bash
# param 1: IP
# param 2: hostname
# param 3: username

IFS=. read ip1 ip2 ip3 ip4 <<< $1
sed -i -e 's/iface enp0s3 inet dhcp/iface enp0s3 inet static\n  address '$1'\n  netmask 255.255.255.0\n  network '$ip1'.'$ip2'.'$ip3'.0\n  broadcast '$ip1'.'$ip2'.'$ip3'.255/g' /etc/network/interfaces

hostnamectl set-hostname $2
sed -i "s/clusterTemplate/$2/" /etc/hosts

cat /home/$3/setup/hosts.cluster | tee -a /etc/hosts 1>/dev/null
