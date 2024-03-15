conflict("oneapi_queue_extensions")

prepend_path{"PATH", "/opt/software/FPGA/patches/oneapi_queue_extensions/bin", priority=100}
append_path("CPATH", "/opt/software/FPGA/extensions")
