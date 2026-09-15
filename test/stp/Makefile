CC ?= cc
CFLAGS = -std=c11 -Wall -Wextra -Wno-unknown-pragmas \
         -D__xdata= -D__code= -D__banked= -D__reentrant= \
         -Ishim -I../..

all: build/stp_sim
	./build/stp_sim

verbose: build/stp_sim
	./build/stp_sim -v

build/stp_sim: stp_sim.c ../../rtl837x_stp.c ../../rtl837x_stp_cli.c ../../rtl837x_mstp.c ../../rtl837x_stp.h ../../rtl837x_mstp.h shim/*.h
	mkdir -p build
	cp ../../rtl837x_stp.c build/stp_under_test.c
	cp ../../rtl837x_stp_cli.c build/stp_cli_under_test.c
	cp ../../rtl837x_mstp.c build/mstp_under_test.c
	$(CC) $(CFLAGS) -o $@ stp_sim.c build/stp_under_test.c build/stp_cli_under_test.c build/mstp_under_test.c

clean:
	rm -rf build

.PHONY: all verbose clean
