CC = cc
CPPFLAGS =  
LDFLAGS =
ARGP_LIB =

IS_GLIBC := $(shell printf '#include <features.h>\n' | \
    $(CC) $(CPPFLAGS) -dM -E -x c - 2>/dev/null | \
    grep -q '^#define __GLIBC__ ' && echo yes)

ifneq ($(IS_GLIBC),yes)
	ARGP_LIB += -largp
endif

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),OpenBSD)
	CPPFLAGS += -I/usr/local/include -I/usr/local/include/json-c 
	LDFLAGS += -L/usr/local/lib
endif

BUILDDIR = output

TARGETS = \
	$(BUILDDIR)/injector \
	$(BUILDDIR)/fileadder \
	$(BUILDDIR)/httpd_sim \
	$(BUILDDIR)/crc_calculator \
	$(BUILDDIR)/imagebuilder

.PHONY: all clean create_build_dir

all: $(TARGETS)

create_build_dir:
	mkdir -p $(BUILDDIR)

$(TARGETS): | create_build_dir

$(BUILDDIR)/injector: injector.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $<

$(BUILDDIR)/fileadder: fileadder.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $< $(ARGP_LIB) -lz

$(BUILDDIR)/crc_calculator: crc_calculator.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $< $(ARGP_LIB)

$(BUILDDIR)/httpd_sim: httpd_sim.c httpd_sim.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $< -ljson-c

$(BUILDDIR)/imagebuilder: imagebuilder.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $< $(ARGP_LIB)

clean:
	rm -rf $(BUILDDIR)


