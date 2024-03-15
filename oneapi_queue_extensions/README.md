# oneAPI Queue Extensions

## Issue
oneAPI on our system does not allow using different accelerators from different MPI ranks. When a process creates a context or a queue (which explicitly creates a context), all devices are probed. If one of them has already been locked by a different process, this fails.

## Fix
* Create a wrapper for `ls` which is used to obtain the list of available devices from /sys/class/aclpci_bitt_s10_pcie. This wrapper will hide the device specified via the environment variable `HIDEACL`:
    ```bash
    $ /opt/software/FPGA/patches/oneapi_queue_extensions/bin/ls /sys/class/aclpci_bitt_s10_pcie
    aclbitt_s10_pcie0  aclbitt_s10_pcie1

    $ HIDEACL=0 /opt/software/FPGA/patches/oneapi_queue_extensions/bin/ls /sys/class/aclpci_bitt_s10_pcie
    aclbitt_s10_pcie1

    $ HIDEACL=1 /opt/software/FPGA/patches/oneapi_queue_extensions/bin/ls /sys/class/aclpci_bitt_s10_pcie
    aclbitt_s10_pcie0
    ```
* Devices need to be allocated in reverse order (i.e., first acl1, then acl0) because libbitt_s10_pcie_mmd only counts the number of available devices and then assumes they start at 0. Hence, one possible way to go is:
    * Hide nothing from rank 1
    * Create context and queue for acl1 on rank 1
    * Hide acl1 from rank 0
    * Create context and queue for acl0 on rank 0
* We provide a C++ header (queue_extensions.hpp) which provides the following functions to conveniently implement this workaround.
    * `sycl::queue sycl::ext::pc2::mpi_queue(DeviceSelector &device_selector)`  
    Returns a queue already set up with the correct device and context for the local rank.
    * `std::vector<sycl::queue> sycl::ext::pc2::mpi_queues(DeviceSelector &device_selector, int num_qs)`  
    Similar to `sycl::ext::pc2::mpi_queue` but returns a list of `num_qs` queues.
* The oneapi_queue_extensions module automatically puts the `ls` wrapper into the user's `$PATH` and makes the header file available as `pc2/queue_extensions.hpp` in the user's `$CPATH`.