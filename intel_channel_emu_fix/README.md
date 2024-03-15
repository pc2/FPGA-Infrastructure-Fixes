# Intel Channel Emulation Fix

## Symptoms
Kernels that use the *cl_intel_channels* OpenCL extension and communicate via `write_channel_intel` and `read_channel_intel` might deadlock on emulation, depending on the order in which kernels attempt to read and write from a channel.

## Technical Details
The issue popped up with the update to RHEL 8 and is caused by I/O buffering on the files that back the emulated channels. The receiving side repeatedly calls `fread` on the file but never gets any new data written by the sending side due to buffering.

## Fix
* Intercept all `fopen` calls performed by application.
* Check if the call originates from `libOclCpuBackEnd_emu.so`.
* If so, disable any buffering on the file using `setvbuf`.