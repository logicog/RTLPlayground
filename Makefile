VERSION=0.1.0
IMAGESIZE = 524288
DEFAULT_CONFIG_LOCATION = 454656
CONFIG_LOCATION = 458752
HTML_LOCATION = 262144
# RTL8238B PSE image: this chip needs a volatile firmware image uploaded at boot, embedded in
# flash at RTL8238B_IMAGE_LOCATION (must match PSE_IMG_ADDR in poe_rtl8238b.c), only for the
# RTL8238B machines below and only when tools/poe/pse_image.bin is present. A board with a
# different PoE chip uses none of this. See doc/poe-rtl8238b.md.
RTL8238B_IMAGE_LOCATION = 393216
RTL8238B_MACHINES = KP_9000_9XHPML_X_V3_1

CC = sdcc
CC_FLAGS = -mmcs51 -I. -Ihttpd -Iuip
ASM = sdas8051
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
	mkdir -p $(BUILDDIR)
	mkdir -p $(BUILDDIR)/uip
	mkdir -p $(BUILDDIR)/httpd

SRCS = rtlplayground.c rtl837x_flash.c rtl837x_leds.c rtl837x_phy.c rtl837x_port.c cmd_parser.c html_data.c rtl837x_igmp.c
SRCS += rtl837x_stp.c rtl837x_pins.c dhcp.c machine.c cmd_editor.c rtl837x_bandwidth.c rtl837x_init.c syslog.c poe_rtl8238b.c
SRCS += uip/timer.c uip/uip.c uip/uip_arp.c uip/uiplib.c uip/uip-fw.c uip/uip-neighbor.c uip/uip-split.c udp_apps.c
SRCS += httpd/httpd.c httpd/page_impl.c
OBJS = ${SRCS:%.c=$(BUILDDIR)/%.rel}
DEPS := ${SRCS:%.c=$(BUILDDIR)/%.d}
HTML := $(shell find $(html) -name '*.js' -or -name '*.html' -or -name '*.svg')

html_data.c html_data.h: $(HTML) tools/output/fileadder
	tools/output/fileadder -a $(HTML_LOCATION) -s $(IMAGESIZE) -b BANK1 -d html -p html_data

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
	tools/output/fileadder -a $(HTML_LOCATION) -s $(IMAGESIZE) -d html -p html_data -b BANK1 $@
	@if echo " $(RTL8238B_MACHINES) " | grep -q " $(MACHINE) "; then \
		if [ -f tools/poe/pse_image.bin ]; then tools/output/fileadder -a $(RTL8238B_IMAGE_LOCATION) -s $(IMAGESIZE) -d tools/poe/pse_image.bin $@; else echo "WARN: tools/poe/pse_image.bin missing - PoE image not embedded (run tools/poe/extract_rtl8238b_image.py)"; fi; \
	fi
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
