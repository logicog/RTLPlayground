BOOTLOADER_ADDRESS=0x100

VERSION=0.1.0
IMAGESIZE = 524288
CONFIG_LOCATION = 458752
HTML_LOCATION = 262144

CC = sdcc
CC_FLAGS = -mmcs51 -I. -Ihttpd -Iuip
ASM = sdas8051
AFLAGS= -plosgff

SUBDIRS := tools uip httpd
SUBDIRSCLEAN=$(addsuffix clean,$(SUBDIRS))

BUILDDIR = output/
VERSION_HEADER := version.h

all: create_build_dir $(VERSION_HEADER) $(SUBDIRS) $(BUILDDIR)rtlplayground.bin

create_build_dir:
	mkdir -p $(BUILDDIR)

SRCS = rtlplayground.c rtl837x_flash.c rtl837x_phy.c rtl837x_port.c cmd_parser.c html_data.c rtl837x_igmp.c rtl837x_stp.c dhcp.c  machine.c cmd_editor.c
OBJS = ${SRCS:%.c=$(BUILDDIR)%.rel}
OBJS += uip/$(BUILDDIR)/timer.rel uip/$(BUILDDIR)/uip-fw.rel uip/$(BUILDDIR)/uip-neighbor.rel uip/$(BUILDDIR)/uip-split.rel uip/$(BUILDDIR)/uip.rel uip/$(BUILDDIR)/uip_arp.rel uip/$(BUILDDIR)/uiplib.rel httpd/$(BUILDDIR)/httpd.rel httpd/$(BUILDDIR)/page_impl.rel

html_data.c html_data.h: html tools
	tools/$(BUILDDIR)fileadder -a $(HTML_LOCATION) -s $(IMAGESIZE) -b BANK1 -d html -p html_data

$(VERSION_HEADER):
	@echo "#ifndef VERSION_H" > $(VERSION_HEADER)
	@echo "#define VERSION_H" >> $(VERSION_HEADER)
	@echo "#define VERSION_SW \"v$(VERSION)-g$(shell git rev-parse --short HEAD)\"" >> $(VERSION_HEADER)
	@echo "#endif" >> $(VERSION_HEADER)

httpd: html_data.h

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	-make -C uip clean
	-make -C httpd clean
	-rm html_data.c html_data.h $(VERSION_HEADER)
	-rm -r $(BUILDDIR)

$(BUILDDIR)crtstart.rel: crtstart.asm
	$(ASM) $(AFLAGS) -o $@ $<

$(BUILDDIR)crc16.rel: crc16.asm
	$(ASM) $(AFLAGS) -o $@ $<

$(BUILDDIR)%.rel: %.c
	$(CC) $(CC_FLAGS) -o $@ -c $<

$(BUILDDIR)%.rel: $(BUILDDIR)%.asm
	${ASM} ${AFLAGS} -o $@ $<
#	mv -f $(addprefix $(basename $^), .lst .rel .sym) .

$(BUILDDIR)rtlplayground.ihx: $(BUILDDIR)crtstart.rel $(OBJS) $(BUILDDIR)crc16.rel
	$(CC) $(CC_FLAGS) -Wl-bHOME=${BOOTLOADER_ADDRESS} -Wl-bBANK1=0x14000 -Wl-bBANK2=0x24000 -Wl-r -o $@ $^

$(BUILDDIR)rtlplayground.img: $(BUILDDIR)rtlplayground.ihx
	objcopy --input-target=ihex -O binary $< $@

$(BUILDDIR)rtlplayground.bin: $(BUILDDIR)rtlplayground.img
	if [ -e $@ ]; then rm $@; fi
	tools/$(BUILDDIR)imagebuilder -i $^ $@
	tools/$(BUILDDIR)fileadder -a $(CONFIG_LOCATION) -s $(IMAGESIZE) -d config.txt $@
	tools/$(BUILDDIR)fileadder -a $(HTML_LOCATION) -s $(IMAGESIZE) -d html -p html_data $@
	tools/$(BUILDDIR)crc_calculator -u $@


.PHONY: clean all $(SUBDIRS)
