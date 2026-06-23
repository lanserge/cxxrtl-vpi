# SPDX-License-Identifier: ISC
#
# Prototype of the cocotb-side glue: build a CXXRTL model + the cxxrtl-vpi
# harness into an executable that loads cocotb's VPI consumer, then run the
# Python testbench against it.
#
# STATUS: SCAFFOLD. The model-generation and compile steps are real; the link
# against cocotb's VPI library + the run target depend on the provider
# implementation (see ../../docs/vpi-coverage.md).

TOP        := counter
SRC        := counter.v
TESTMODULE := test_counter

CXX        ?= c++
CXXSTD     ?= -std=c++14
OPT        ?= -O2

REPO       := ../..
CXXRTL_INC := $(shell yosys-config --datdir)/include/backends/cxxrtl/runtime

# cocotb-provided pieces (paths come from cocotb-config / the Python helper)
COCOTB_LIBDIR  := $(shell cocotb-config --lib-dir 2>/dev/null)
COCOTB_INCDIR  := $(shell cocotb-config --share-dir 2>/dev/null)/include

.PHONY: all model build run clean

all: run

# 1. RTL -> CXXRTL C++ model (emits extern "C" cxxrtl_design_create()).
$(TOP)_cxxrtl.cc: $(SRC)
	yosys -q -p "read_verilog $(SRC); hierarchy -top $(TOP); write_cxxrtl $@"

model: $(TOP)_cxxrtl.cc

# 2. Build the provider library (design-independent).
$(REPO)/build/libcxxrtl_vpi.a:
	cmake -S $(REPO) -B $(REPO)/build
	cmake --build $(REPO)/build

# 3. Link model + harness + provider + cocotb VPI consumer -> sim executable.
#    TODO: add cocotb VPI link flags once the startup symbol is confirmed:
#       -L$(COCOTB_LIBDIR) -lcocotbvpi_cxxrtl -Wl,-rpath,$(COCOTB_LIBDIR)
sim_$(TOP): $(TOP)_cxxrtl.cc $(REPO)/build/libcxxrtl_vpi.a
	$(CXX) $(CXXSTD) $(OPT) \
	  -I$(REPO)/include -I$(CXXRTL_INC) -I$(COCOTB_INCDIR) \
	  $(TOP)_cxxrtl.cc $(REPO)/src/harness.cc \
	  -L$(REPO)/build -lcxxrtl_vpi \
	  -o $@

build: sim_$(TOP)

# 4. Run cocotb against it.
#    cocotb env mirrors the other simulators (see cocotb docs / SC integration).
run: build
	@echo "TODO: COCOTB_TEST_MODULES=$(TESTMODULE) COCOTB_TOPLEVEL=$(TOP) ./sim_$(TOP)"
	@echo "Provider not yet implemented; see docs/vpi-coverage.md."

clean:
	rm -f $(TOP)_cxxrtl.cc sim_$(TOP) results.xml *.vcd
	rm -rf __pycache__
