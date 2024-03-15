demo: demo.cpp
	mpiicpx -fsycl -o demo demo.cpp

install: pc2/queue_extensions.hpp 0.2.lua bin/ls
	mkdir -p /opt/software/FPGA/patches/oneapi_queue_extensions/bin
	cp bin/ls /opt/software/FPGA/patches/oneapi_queue_extensions/bin/
	chmod +x /opt/software/FPGA/patches/oneapi_queue_extensions/bin/ls

	mkdir -p /opt/software/FPGA/extensions/pc2
	cp pc2/queue_extensions.hpp /opt/software/FPGA/extensions/pc2/

	mkdir -p /opt/software/FPGA/modulefiles/intel/oneapi_queue_extensions
	cp 0.2.lua /opt/software/FPGA/modulefiles/intel/oneapi_queue_extensions/
