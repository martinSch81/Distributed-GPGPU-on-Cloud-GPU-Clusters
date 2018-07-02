# Distributed-GPGPU-on-Cloud-GPU-Clusters
Master thesis. Fully automated creation of MPI clusters on Amazon EC2, and a library, that distributes GPGPU problems, described as a workflow, on the newly created cluster.

Using GPGPU for problems, which can be solved via massive parallel algorithms, may lead to performance gains on appropriate hardware.
The high number of instances rentable from cloud providers like Amazon EC2 offer an interesting basis for powerful distributed systems. Combining both technologies could provide an immense computation power if the problem scales well.
The goal of this Master thesis was writing a library that simplifies and automates the creation of a cluster as well as typical requirements for distributed systems like send/receive, broadcast, execution, ...
To prove the usability of the library, real applications that are distributable and already use GPGPU should be implemented and benchmarked.

Our work is open source (GNU GPLv3), we hope that it is helpful, your comments are welcome and we invite everyone to help develop it further.

See Schuchardt-GPGPU_on_Cloud_GPUs.pdf for documentation and Schuchardt-GPGPU_on_Cloud_GPUs-Quick_Tutorial.pdf for an installation assistance.
