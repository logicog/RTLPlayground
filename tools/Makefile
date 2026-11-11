CC = gcc
CCFLAGS = -Wall -o
ifndef BUILDDIR
	BUILDDIR = output
endif

all: create_build_dir $(BUILDDIR)/injector $(BUILDDIR)/fileadder \
	$(BUILDDIR)/crc_calculator $(BUILDDIR)/imagebuilder

create_build_dir:
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR)

$(BUILDDIR)/injector: injector.c
	gcc $^ $(CCFLAGS) $@

$(BUILDDIR)/fileadder: fileadder.c
	gcc $^ $(CCFLAGS) $@ $(shell pkg-config --cflags --libs zlib)

$(BUILDDIR)/crc_calculator: crc_calculator.c
	gcc $^ $(CCFLAGS) $@

$(BUILDDIR)/imagebuilder: imagebuilder.c
	gcc $^ $(CCFLAGS) $@
