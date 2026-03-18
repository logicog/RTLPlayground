VERSION=0.1.0
IMAGESIZE = 524288
DEFAULT_CONFIG_LOCATION = 454656
CONFIG_LOCATION = 458752
HTML_LOCATION = 262144

CC = sdcc
CC_FLAGS = -mmcs51 -I. -Ihttpd -Iuip
ASM = sdas8051
AFLAGS= -plosgff

SUBDIRS := tools
SUBDIRSCLEAN=$(addsuffix clean,$(SUBDIRS))

BUILDDIR = output
VERSION_HEADER := version.h

ifeq ($(MACHINE),)
else
	CC_FLAGS += -DMACHINE_$(MACHINE)
endif

all: create_build_dir $(VERSION_HEADER) $(SUBDIRS) $(BUILDDIR)/rtlplayground.bin

create_build_dir:
	mkdir -p $(BUILDDIR)
	mkdir -p $(BUILDDIR)/uip
	mkdir -p $(BUILDDIR)/httpd

SRCS = rtlplayground.c rtl837x_flash.c rtl837x_leds.c rtl837x_phy.c rtl837x_port.c cmd_parser.c html_data.c rtl837x_igmp.c
SRCS += rtl837x_stp.c rtl837x_pins.c dhcp.c machine.c cmd_editor.c rtl837x_bandwidth.c
SRCS += uip/timer.c uip/uip.c uip/uip_arp.c uip/uiplib.c uip/uip-fw.c uip/uip-neighbor.c uip/uip-split.c
SRCS += httpd/httpd.c httpd/page_impl.c
OBJS = ${SRCS:%.c=$(BUILDDIR)/%.rel}
DEPS := ${SRCS:%.c=$(BUILDDIR)/%.d}
HTML := $(shell find $(html) -name '*.js' -or -name '*.html' -or -name '*.svg')

html_data.c html_data.h: $(HTML) tools/$(BUILDDIR)/fileadder
	tools/$(BUILDDIR)/fileadder -a $(HTML_LOCATION) -s $(IMAGESIZE) -b BANK1 -d html -p html_data

$(VERSION_HEADER):
	@echo "#ifndef VERSION_H" > $(VERSION_HEADER)
	@echo "#define VERSION_H" >> $(VERSION_HEADER)
	@echo "#define VERSION_SW \"v$(VERSION)-g$(shell git rev-parse --short HEAD)\"" >> $(VERSION_HEADER)
	@echo "#define BUILD_DATE \"$(shell date +"%Y-%m-%d %H:%M:%S")\"" >> $(VERSION_HEADER)
	@echo "#endif" >> $(VERSION_HEADER)

httpd: html_data.h

$(SUBDIRS):
	$(MAKE) -C $@

clean:
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

$(BUILDDIR)/rtlplayground.bin: $(BUILDDIR)/rtlplayground.img
	if [ -e $@ ]; then rm $@; fi
	tools/$(BUILDDIR)/imagebuilder -i $^ $@
	tools/$(BUILDDIR)/fileadder -a $(DEFAULT_CONFIG_LOCATION) -s $(IMAGESIZE) -d config.txt $@
	tools/$(BUILDDIR)/fileadder -a $(CONFIG_LOCATION) -s $(IMAGESIZE) -d config.txt $@
	tools/$(BUILDDIR)/fileadder -a $(HTML_LOCATION) -s $(IMAGESIZE) -d html -p html_data -b BANK1 $@
	tools/$(BUILDDIR)/crc_calculator -u $@


.PHONY: clean all $(SUBDIRS) $(VERSION_HEADER)

-include $(DEPS)
