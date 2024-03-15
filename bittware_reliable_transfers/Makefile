TARGET_LIBDIR ?= /opt/software/FPGA/patches/bittware_reliable_transfers
TARGET_MODDIR ?= /opt/software/FPGA/bsp_specific_modulefiles
MODNAME ?= bittware/520n_reliable_transfers

INCLUDE_FLAGS = -I/opt/software/FPGA/IntelFPGA/opencl_sdk/20.4.0/hld/board/bittware_pcie/s10_hpc_default/software/include
CPPFLAGS = -O2 -std=c++17 -Wall -Wextra -Wshadow -Wnon-virtual-dtor -Wconversion -Wno-unused-parameter $(INCLUDE_FLAGS)
LINK_FLAGS = -ldl

NALLA_BSPs = 18.0.0 18.0.1 18.1.1
BITTWARE_BSPs = 19.1.0 19.2.0 19.4.0 20.4.0

NALLA_TARGETS = $(addsuffix /libnalla_pcie_mmd.so, $(NALLA_BSPs))
BITTWARE_TARGETS = $(addsuffix /libbitt_s10_pcie_mmd.so, $(BITTWARE_BSPs))
MODULE_TARGETS = $(addsuffix .lua, $(addprefix modules/, $(NALLA_BSPs) $(BITTWARE_BSPs)))

INSTALL_NALLA_TARGETS = $(addprefix install_, $(NALLA_TARGETS))
INSTALL_BITTWARE_TARGETS = $(addprefix install_, $(BITTWARE_TARGETS))
INSTALL_MODULE_TARGETS = $(addprefix install_, $(MODULE_TARGETS))

all: $(NALLA_TARGETS) $(BITTWARE_TARGETS) $(MODULE_TARGETS)

install: $(INSTALL_NALLA_TARGETS) $(INSTALL_BITTWARE_TARGETS) $(INSTALL_MODULE_TARGETS)

$(NALLA_TARGETS): %/libnalla_pcie_mmd.so: wrapper.cpp
	mkdir -p $*
	$(eval INCLUDE_AOCL_MMD_REPROGRAM = $(shell echo $* | awk '$$0 ~ /^18.0.[01]$$/ {print "-DINCLUDE_AOCL_MMD_REPROGRAM"}'))
	$(CXX) $(CPPFLAGS) $(LINK_FLAGS) -fPIC -shared \
	-DBSP=/opt/software/FPGA/IntelFPGA/opencl_sdk/$*/hld/board/nalla_pcie/linux64/lib/libnalla_pcie_mmd.so \
	$(INCLUDE_AOCL_MMD_REPROGRAM) -o $@ $^

$(BITTWARE_TARGETS): %/libbitt_s10_pcie_mmd.so: wrapper.cpp
	mkdir -p $*
	$(CXX) $(CPPFLAGS) $(LINK_FLAGS) -fPIC -shared \
	-DBSP=/opt/software/FPGA/IntelFPGA/opencl_sdk/$*/hld/board/bittware_pcie/s10/linux64/lib/libbitt_s10_pcie_mmd.so \
	-o $@ $^

$(MODULE_TARGETS): modules/%.lua: reliable_transfers.lua.template
	mkdir -p modules
	cat $^ | sed "s/VERSION/$*/" > $@

$(INSTALL_NALLA_TARGETS): install_%/libnalla_pcie_mmd.so: %/libnalla_pcie_mmd.so
	install -Dm755 $*/libnalla_pcie_mmd.so -t $(TARGET_LIBDIR)/$*

$(INSTALL_BITTWARE_TARGETS): install_%/libbitt_s10_pcie_mmd.so: %/libbitt_s10_pcie_mmd.so
	install -Dm755 $*/libbitt_s10_pcie_mmd.so -t $(TARGET_LIBDIR)/$*

$(INSTALL_MODULE_TARGETS): install_modules/%.lua: modules/%.lua
	install -Dm755 modules/$*.lua -t $(TARGET_MODDIR)/$*/$(MODNAME)
