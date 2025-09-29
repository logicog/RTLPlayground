CC = gcc
CCFLAGS = -Wall -o
BUILDDIR = output/

all: create_build_dir $(BUILDDIR)injector $(BUILDDIR)fileadder $(BUILDDIR)httpd_sim

create_build_dir:
	mkdir -p $(BUILDDIR)

clean:
	rm -r $(BUILDDIR)

$(BUILDDIR)injector: injector.c
	gcc $^ $(CCFLAGS) $@

$(BUILDDIR)fileadder: fileadder.c
	gcc $^ $(CCFLAGS) $@

$(BUILDDIR)httpd_sim: httpd_sim.c httpd_sim.h
	gcc $< $(CCFLAGS) $@ -I/usr/include/json-c -ljson-c
