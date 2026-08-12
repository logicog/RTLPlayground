VERSION=0.1.0
IMAGESIZE = 524288
DEFAULT_CONFIG_LOCATION = 454656
CONFIG_LOCATION = 458752
HTML_LOCATION = 262144

ifeq ($(origin CC),default)
CC = sdcc
endif
CC_FLAGS = -mmcs51 -I. -Ihttpd -Iuip
ASM ?= sdas8051
AFLAGS= -plosgff

SUBDIRS := tools
SUBDIRSCLEAN=$(addsuffix clean,$(SUBDIRS))

ifeq ($(MACHINE),)
	MACHINE:= $(shell grep "^\s*#define MACHINE_" machine.h | sed "s/^\s*#define MACHINE_//")
else
	CC_FLAGS += -DMACHINE_$(MACHINE)
endif

BUILDDIR = output/$(MACHINE)
VERSION_HEADER := version.h

GIT_VERSION := $(shell git rev-parse --short HEAD)
ifeq ($(shell git status --porcelain --untracked-files=no),)
else
	GIT_VERSION := $(GIT_VERSION)-dirty
endif

VERSION_EXTENSION = v$(VERSION)-$(GIT_VERSION)
FILENAME_EXTENSION = $(VERSION_EXTENSION)-$(MACHINE)

all: create_build_dir $(VERSION_HEADER) $(SUBDIRS) $(BUILDDIR)/rtlplayground-$(FILENAME_EXTENSION).bin

create_build_dir:
	mkdir -p "$(BUILDDIR)"
	mkdir -p "$(BUILDDIR)/uip"
	mkdir -p "$(BUILDDIR)/httpd"

# Keep machine.c in first position to fail immediately on invalid $MACHINE value
SRCS = \
	machine.c \
	cmd_editor.c \
	cmd_parser.c \
	dhcp.c \
	html_data.c \
	rtlplayground.c \
	syslog.c \
	udp_apps.c

# RTL837x
SRCS += \
	rtl837x_bandwidth.c \
	rtl837x_flash.c \
	rtl837x_igmp.c \
	rtl837x_init.c \
	rtl837x_leds.c \
	rtl837x_phy.c \
	rtl837x_pins.c\
	rtl837x_port.c \
	rtl837x_stp.c
SRCS += \
	httpd/httpd.c \
	httpd/page_impl.c
SRCS += \
	uip/timer.c \
	uip/uip.c \
	uip/uiplib.c \
	uip/uip_arp.c \
	uip/uip-fw.c \
	uip/uip-neighbor.c \
	uip/uip-split.c

OBJS = ${SRCS:%.c=$(BUILDDIR)/%.rel}
DEPS := ${SRCS:%.c=$(BUILDDIR)/%.d}
HTML := $(shell find html -name '*.js' -or -name '*.html' -or -name '*.svg' -or -name '*.css' -or -name '*.ico')

# Minified copy of the web UI sources, used as the fileadder input.
# The raw html/ sources stay untouched for development; the minified
# copy is a build artifact under output/.
HTML_MIN := output/html_min
.PHONY: html_min
html_min: $(HTML)
	rm -rf $(HTML_MIN)
	mkdir -p $(HTML_MIN)
	@for f in $(HTML); do python3 tools/minify.py $$f $(HTML_MIN)/$$(basename $$f) || exit 1; done

html_data.c html_data.h: $(HTML) tools/output/fileadder html_min
	tools/output/fileadder -a $(HTML_LOCATION) -s $(IMAGESIZE) -b BANK1 -z -d $(HTML_MIN) -p html_data
# httpd.c includes html_data.h; declare it explicitly so a parallel
# build (-j) does not compile httpd before the header is generated.
$(BUILDDIR)/httpd/httpd.rel: html_data.h

$(VERSION_HEADER):
	@echo "#ifndef VERSION_H" > $(VERSION_HEADER)
	@echo "#define VERSION_H" >> $(VERSION_HEADER)
	@echo "#define VERSION_SW \"$(VERSION_EXTENSION)\"" >> $(VERSION_HEADER)
	@echo "#define BUILD_DATE \"$(shell date +"%Y-%m-%d %H:%M:%S")\"" >> $(VERSION_HEADER)
	@echo "#endif" >> $(VERSION_HEADER)

httpd: html_data.h

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	-rm -f html_data.c html_data.h $(VERSION_HEADER)
	-if [ -d $(BUILDDIR) ]; then find $(BUILDDIR) -type f ! -name "*.bin" -delete; fi

distclean:
	-rm -f html_data.c html_data.h $(VERSION_HEADER)
	-rm -rf $(BUILDDIR)

$(BUILDDIR)/%.rel: %.c
	$(CC) -MMD $(CC_FLAGS) -o $@ -c $<

$(BUILDDIR)/%.rel: %.asm
	${ASM} ${AFLAGS} -o $@ $<
#	mv -f $(addprefix $(basename $^), .lst .rel .sym) .

$(BUILDDIR)/rtlplayground.ihx: $(OBJS) $(BUILDDIR)/crtstart.rel $(BUILDDIR)/crc16.rel
	$(CC) $(CC_FLAGS) -Wl-bHOME=0x00000 -Wl-bBANK1=0x14000 -Wl-bBANK2=0x24000 -Wl-r -o $@ $^

$(BUILDDIR)/rtlplayground.img: $(BUILDDIR)/rtlplayground.ihx
	objcopy --input-target=ihex -O binary $< $@

$(BUILDDIR)/rtlplayground-$(FILENAME_EXTENSION).bin: $(BUILDDIR)/rtlplayground.img
	if [ -e $@ ]; then rm $@; fi
	tools/output/imagebuilder -i $^ $@
	tools/output/fileadder -a $(DEFAULT_CONFIG_LOCATION) -s $(IMAGESIZE) -d config.txt $@
	tools/output/fileadder -a $(CONFIG_LOCATION) -s $(IMAGESIZE) -d config.txt $@
	tools/output/fileadder -a $(HTML_LOCATION) -s $(IMAGESIZE) -z -d $(HTML_MIN) -p html_data -b BANK1 $@
	tools/output/crc_calculator -u $@
	ln -sf $(MACHINE)/rtlplayground-$(FILENAME_EXTENSION).bin output/rtlplayground.bin

.PHONY: clean all $(SUBDIRS) $(VERSION_HEADER)

.PHONY:
machine_check:
	@mkdir -p $(BUILDDIR)/tmp
	@set -eo pipefail; \
	for MACHINE in `grep -e ' MACHINE_' machine.c | sed -e 's%^.* MACHINE_%%' -e 's%[ ]*//.*$$%%' | sort -u`; \
	do \
	echo "Checking $${MACHINE}"; \
	$(CC) $(CC_FLAGS) -DMACHINE_$${MACHINE} -MMD -o $(BUILDDIR)/tmp/machine_check -c machine.c; \
	done
	@rm -rf $(BUILDDIR)/tmp

-include $(DEPS)
