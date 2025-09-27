BOOTLOADER_ADDRESS=0x100

IMAGESIZE = 524288
CONFIG_LOCATION = 458752
HTML_LOCATION = 262144

CC = sdcc
CC_FLAGS = -mmcs51 -Ihttpd -Iuip
ASM = sdas8051
AFLAGS= -plosgff

SUBDIRS := tools uip httpd
SUBDIRSCLEAN=$(addsuffix clean,$(SUBDIRS))

BUILDDIR = output/

all: create_build_dir $(SUBDIRS) $(BUILDDIR)rtlplayground.bin

create_build_dir:
	mkdir -p $(BUILDDIR)

SRCS = rtlplayground.c rtl837x_flash.c rtl837x_phy.c rtl837x_port.c cmd_parser.c html_data.c rtl837x_igmp.c rtl837x_stp.c
OBJS = ${SRCS:%.c=$(BUILDDIR)%.rel}
OBJS += uip/timer.rel uip/uip-fw.rel uip/uip-neighbor.rel uip/uip-split.rel uip/uip.rel uip/uip_arp.rel uip/uiplib.rel httpd/httpd.rel httpd/page_impl.rel

html_data.c html_data.h: html tools
	tools/fileadder -a $(HTML_LOCATION) -s $(IMAGESIZE) -b BANK1 -d html -p html_data

httpd: html_data.h

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	-make -C uip clean
	-make -C httpd clean
	-rm html_data.c html_data.h
	-rm -r $(BUILDDIR)

$(BUILDDIR)crtstart.rel: crtstart.asm
	$(ASM) $(AFLAGS) -o $@ $<

$(BUILDDIR)%.rel: %.c
	$(CC) $(CC_FLAGS) -o $@ -c $<

$(BUILDDIR)%.rel: $(BUILDDIR)%.asm
	${ASM} ${AFLAGS} -o $@ $<
#	mv -f $(addprefix $(basename $^), .lst .rel .sym) .

$(BUILDDIR)rtlplayground.ihx: $(BUILDDIR)crtstart.rel $(OBJS) 
	$(CC) $(CC_FLAGS) -Wl-bHOME=${BOOTLOADER_ADDRESS}  -Wl-bBANK1=0x14000 -Wl-r -o $@ $^

$(BUILDDIR)rtlplayground.img: $(BUILDDIR)rtlplayground.ihx
	objcopy --input-target=ihex -O binary $< $@

$(BUILDDIR)rtlplayground.bin: $(BUILDDIR)rtlplayground.img
	if [ -e $@ ]; then rm $@; fi
	echo "0000000: 00 40" | xxd -r - $@
	cat $< >> $@
	truncate --size=16K $@
	dd if=$< skip=80 bs=1024 >>$@
	tools/fileadder -a $(CONFIG_LOCATION) -s $(IMAGESIZE) -d config.txt $@
	tools/fileadder -a $(HTML_LOCATION) -s $(IMAGESIZE) -d html -p html_data $@


.PHONY: clean all $(SUBDIRS)