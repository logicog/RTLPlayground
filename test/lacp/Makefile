# Host-side sandbox for the firmware LACP state machines.
#
# Compiles the UNMODIFIED ../../rtl837x_lacp.c against shim headers: SDCC
# storage-class keywords are blanked via -D, and the module copy lives in
# build/ so quote-includes resolve to shim/ (its own directory has no real
# platform headers) instead of the firmware tree.
CC ?= cc
CFLAGS = -std=c11 -Wall -Wextra -Wno-unknown-pragmas \
         -D__xdata= -D__code= -D__banked= \
         -Ishim -I../..

all: build/lacp_sim
	./build/lacp_sim

verbose: build/lacp_sim
	./build/lacp_sim -v

build/lacp_sim: lacp_sim.c ../../rtl837x_lacp.c ../../rtl837x_lacp.h shim/*.h
	mkdir -p build
	cp ../../rtl837x_lacp.c build/lacp_under_test.c
	$(CC) $(CFLAGS) -o $@ lacp_sim.c build/lacp_under_test.c

clean:
	rm -rf build

.PHONY: all verbose clean
