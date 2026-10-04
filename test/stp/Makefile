CC      ?= cc
SRCROOT  = ../..
CFLAGS   = -std=c11 -g -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unknown-pragmas \
           -Wno-pointer-sign \
           -DRTLP_HOST_TEST \
           -fsanitize=address,undefined -fno-omit-frame-pointer \
           -include ../sdcc_shim.h -I../stub -I$(SRCROOT) -I$(SRCROOT)/httpd -I$(SRCROOT)/uip
ifeq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
CFLAGS  += -Wno-builtin-declaration-mismatch
endif
LDFLAGS  = -fsanitize=address,undefined

SRCS = stp_sim.c $(SRCROOT)/rtl837x_stp.c $(SRCROOT)/rtl837x_stp_cli.c $(SRCROOT)/rtl837x_mstp.c

all: test

test: build/stp_sim
	./build/stp_sim

verbose: build/stp_sim
	./build/stp_sim -v

build/stp_sim: $(SRCS) $(SRCROOT)/rtl837x_stp.h $(SRCROOT)/rtl837x_mstp.h
	mkdir -p build
	$(CC) $(CFLAGS) $(SRCS) -o $@ $(LDFLAGS)

clean:
	rm -rf build

.PHONY: all test verbose clean
