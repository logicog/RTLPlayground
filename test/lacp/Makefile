# Host-side sandbox for the firmware LACP state machines.
#
# Compiles the UNMODIFIED ../../rtl837x_lacp.c against shim headers: SDCC
# storage-class keywords are blanked via -D, and the module copy lives in
# build/ so quote-includes resolve to shim/ (its own directory has no real
# platform headers) instead of the firmware tree. rtl837x_trap.h is copied
# alongside it for the same reason: TRAP_RX_BODY() is what delivery by trap
# rests on, so the test must use the real macro, not a shim restating it.
CC ?= cc
CFLAGS = -std=c11 -Wall -Wextra -Wno-unknown-pragmas \
         -D__xdata= -D__code= -D__banked= -D__reentrant= \
         -Ishim -I../..

all: test

test: build/lacp_sim
	./build/lacp_sim

verbose: build/lacp_sim
	./build/lacp_sim -v

build/lacp_sim: lacp_sim.c ../../rtl837x_lacp.c ../../rtl837x_lacp.h ../../rtl837x_trap.h shim/*.h
	mkdir -p build
	cp ../../rtl837x_lacp.c build/lacp_under_test.c
	cp ../../rtl837x_trap.h build/rtl837x_trap.h
	$(CC) $(CFLAGS) -o $@ lacp_sim.c build/lacp_under_test.c

clean:
	rm -rf build

.PHONY: all test verbose clean
