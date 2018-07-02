# !/bin/bash
# get sudo:
# su -
# apt-get install sudo
# adduser <USER> sudo
# mkdir /home/mpi/setup
# 
# if required, change timezone (currently: vienna)
#   sudo ln -sf /usr/share/zoneinfo/Asia/Tokyo /etc/localtime
# 
# also, mount guest additions
# transfer necessary files & execute:
#   ssh mpi@$IP "mkdir -p ~/setup"
#   scp OpenCL/opencl_runtime_* CUDA/cuda_*.run files-common/config.cfg files-debian/setupDebianTemplate.sh mpi@$IP:~/setup/
#   /mnt/c/Program\ Files/Oracle/VirtualBox/VBoxManage.exe storageattach "Debian_ClusterTemplate" --storagectl "IDE" --port 1 --device 0 --type dvddrive --medium additions
#   ssh -t mpi@$IP "sudo setup/setupDebianTemplate.sh"
#   ssh -t mpi@$IP "eject; sudo shutdown -h now"
#


echo "reading configuration..."
if [ -f /home/${SUDO_USER}/setup/config.cfg ]; then
  . /home/${SUDO_USER}/setup/config.cfg
fi

QUIT="false"
CUDA_INSTALLER="cuda_9.1.85_387.26_linux.run"
OPENCL_INSTALLER_NAME="opencl_runtime_16.1.2_x64_rh_6.4.0.37"

mount /media/cdrom
if [ ! -f /media/cdrom/VBoxLinuxAdditions.run ]; then
  echo "VirtualBox Guest Additions not found, terminating now."
  QUIT="true"
fi
if [ ! -f /home/${SUDO_USER}/setup/opencl_runtime_silent.cfg ]; then
  echo "OpenCL silent installation file not found, terminating now."
  QUIT="true"
fi
if [ ! -f /home/${SUDO_USER}/setup/${OPENCL_INSTALLER_NAME}.tgz ]; then
  echo "OpenCL runtime installer not found, terminating now."
  QUIT="true"
fi
if [ ! -f /home/${SUDO_USER}/setup/${CUDA_INSTALLER} ]; then
  echo "CUDA toolkit installer not found, terminating now."
  QUIT="true"
fi

if [ $QUIT = "true" ]; then
  exit 99
fi


echo "%sudo   ALL=(ALL) NOPASSWD: /sbin/poweroff, /sbin/reboot, /sbin/shutdown" | sudo tee -a /etc/sudoers
echo 'set bell-style none' >> /home/${SUDO_USER}/.inputrc
chown -R ${SUDO_USER}:${SUDO_USER} /home/${SUDO_USER}/.inputrc

# 2nd IF, that instances use, often hungs if not set to 'allow-hotplug'
echo "\nallow-hotplug enp0s8\niface enp0s8 inet dhcp\n" | tee -a /etc/network/interfaces
sed -i -e 's/auto enp0s3/allow-hotplug enp0s3/g' /etc/network/interfaces


# upgrade distribution
echo "\ndeb http://ftp.at.debian.org/debian/ stretch contrib non-free" | tee -a /etc/apt/sources.list
echo "deb-src http://ftp.at.debian.org/debian/ stretch contrib non-free\n" | tee -a /etc/apt/sources.list
apt-get -qq update --assume-yes
apt-get -qq upgrade --assume-yes
# apt-get -qq dist-upgrade --assume-yes

# install vbox guest additions
apt-get -qq install dkms build-essential linux-headers-$(uname -r) --assume-yes
# installing from repository makes vm inaccessible for vboxmanage guestcontrol, unfortunately
# apt-get -qq install virtualbox-guest-utils --assume-yes
mount /dev/sr0 /media/cdrom
cd /media/cdrom
sh ./VBoxLinuxAdditions.run
cd /
umount /media/cdrom

# tiny tweaks 
echo "alias ls='ls --color=auto'\nalias ll='ls -l'\nalias la='ls -A'\nalias lt='ls -ltr'\nalias l='ls -CF'\n" | tee -a /home/${SUDO_USER}/.bash_aliases
echo "alias grep='grep --color=auto'\nalias fgrep='fgrep --color=auto'\nalias egrep='egrep --color=auto'\n" | tee -a /home/${SUDO_USER}/.bash_aliases
chown -R ${SUDO_USER}:${SUDO_USER} /home/${SUDO_USER}/.bash_aliases

# fake DNS to access vbox host
echo $CUSTOM_HOST_ENTRIES | tee -a /etc/hosts

# for OpenCL, OpenMPI and the library features
apt-get -qq install gcc g++ gdb git make opencl-headers openmpi-bin libopenmpi2 libopenmpi-dev lsb-core jq graphviz --assume-yes

target=/home/${SUDO_USER}/setup/${OPENCL_INSTALLER_NAME}.tgz
tar -zxf $target -C /home/${SUDO_USER}/setup/
echo "  INFORMATION: Warning about 'Missing optional prerequisites' and 'Unsupported OS' may be ignored"
/home/${SUDO_USER}/setup/${OPENCL_INSTALLER_NAME}/install.sh --cli-mode --silent /home/${SUDO_USER}/setup/opencl_runtime_silent.cfg
echo "/opt/intel/opencl/lib64" | tee /etc/ld.so.conf.d/intel.conf >/dev/null
ldconfig

# CUDA
target=/home/${SUDO_USER}/setup/${CUDA_INSTALLER}
chmod u+x $target
$target --silent --toolkit --override --no-opengl-libs
grep "Toolkit: " /tmp/cuda_install_*.log
echo "/usr/local/cuda/lib64" | tee /etc/ld.so.conf.d/nvidia.conf >/dev/null
ldconfig

# reduce image size
rm -rf /home/${SUDO_USER}/setup/*
apt-get purge $(dpkg -l linux-{image,headers}-"[0-9]*" | awk '/ii/{print $2}' | grep -ve "$(uname -r | sed -r 's/-[a-z]+//')")
apt-get autoclean
apt-get autoremove
dd if=/dev/zero of=zerofillfile bs=1M
rm zerofillfile
