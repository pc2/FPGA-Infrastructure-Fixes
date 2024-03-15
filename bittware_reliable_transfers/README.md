# Bittware 520N Reliable Transfer Layer

## Symptoms
Data transfers from the FPGA to the host sometimes leave out a couple of pages, leading to corruption of the read data.

## Technical Details
The issue occurs seldomly enough to stay unnoticed for several years with a rough estimate of one error per 280 TiB of transferred data. The root cause is unknown but likely to be found in one of the following components:
* DMA logic in the kernel driver
* Bittware userspace library (libbitt_s10_pcie_mmd.so)
* FPGA shell, in particular the PCIe / DMA logic

All BSP versions seem to be affected.

## Fix

### Approach and Implementation
* Wrap the Bittware userspace library which implements Intel's MMD API. Since the library is loaded at runtime, this requires replacing the original library and wrapping large parts of the API.
* Introduce additional logic in `aocl_mmd_read` that puts a known 32-byte pattern into each page of the receive buffer if a global memory read is performed.
* Verify that the pattern has entirely vanished in:
    * `aocl_mmd_read` for blocking calls
    * a custom status handler for non-blocking calls (always the case for OpenCL/oneAPI)
* Information about the data transfer is written to a struct of type `wrapped_aocl_mmd_op_t` whose address is passed as `aocl_mmd_op_t op` from `aocl_mmd_read` into that custom handler.
* The custom handler recognizes these addresses based on a global set of known allocations `known_op_wrappings`.
* The originally registered handler is stored globally and invoked by the custom handler after  doing the transfer validation and unwrapping the original `op`.
* If validation of the data transfer fails, a message is printed to `stderr` and a blocking transfer with the same arguments is issued to transparently fix the data.

### Performance Overhead
The overhead for read transfers is expected to be around 5% and depends on the overall system load. However, there is a performance trap that can reduce the performance by 50% or more: The workaround writes to each page of the host target buffer. If this is freshly allocated memory that is not yet backed by physical memory pages, this introduces severe overhead. This overhead can be slightly reduced setting the environment variable `BITTFIX_MLOCK`, causing the entire buffer being locked in one go instead of causing syscalls for each page. However, for low-overhead use of this workaround, host buffers should always be reused in the host code instead of being allocated for each transfer.
