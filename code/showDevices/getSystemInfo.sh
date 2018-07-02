# !/bin/bash

lsb_release > lsb_release.txt
cat /etc/system-release > system-release.txt
lscpu > lscpu.txt
cat /proc/cpuinfo > proc-cpuinfo.txt
cat /proc/meminfo > proc-meminfo.txt
./showDevicesCL > showDevicesCL.txt

mpic++ --version > mpicpp.txt
gcc --version > gcc.txt
g++ --version > gpp.txt
ompi_info > ompi_info.txt
mpirun --version > mpirun.txt
