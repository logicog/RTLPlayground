BOOTLOADER_ADDRESS=0x100

CC = sdcc
CC_FLAGS = -mmcs51 -Ihttpd -Iuip
ASM = sdas8051
AFLAGS= -plosgff

SUBDIRS := tools uip httpd
SUBDIRSCLEAN=$(addsuffix clean,$(SUBDIRS))

all: $(SUBDIRS) rtlplayground.bin

SRCS = rtlplayground.c rtl837x_flash.c rtl837x_phy.c rtl837x_port.c cmd_parser.c html_data.c rtl837x_igmp.c rtl837x_stp.c
OBJS = ${SRCS:.c=.rel}
OBJS += uip/timer.rel uip/uip-fw.rel uip/uip-neighbor.rel uip/uip-split.rel uip/uip.rel uip/uip_arp.rel uip/uiplib.rel httpd/httpd.rel httpd/page_impl.rel

html_data.c html_data.h: html tools
	tools/fileadder -a 458752 -s 524288 -b BANK1 -d html -p html_data

httpd: html_data.h

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	-make -C uip clean
	-make -C httpd clean
	-rm html_data.c html_data.c
	if [ -e rtlplayground.bin ]; then rm rtlplayground.bin; fi
	if [ -e rtlplayground.asm ]; then rm rtlplayground.asm; fi
	-rm *.ihx *.lk *.lst *.map *.mem *.rel *.rst *.sym *.bin


%.rel: %.c
	$(CC) $(CC_FLAGS) -c $<

%.rel: %.asm
	${ASM} ${AFLAGS} $^
#	mv -f $(addprefix $(basename $^), .lst .rel .sym) .

rtlplayground.ihx:  crtstart.rel $(OBJS) 
	$(CC) $(CC_FLAGS) -Wl-bHOME=${BOOTLOADER_ADDRESS}  -Wl-bBANK1=0x14000 -Wl-r -o $@ $^

%.img: %.ihx
	objcopy --input-target=ihex -O binary $< $@

%.bin: %.img
	if [ -e $@ ]; then rm $@; fi
	echo "0000000: 00 40" | xxd -r - $@
	cat $< >> $@
	truncate --size=16K $@
	dd if=$< skip=80 bs=1024 >>$@
	tools/fileadder -a 458752 -s 524288 -d config.txt $@
	tools/fileadder -a 262144 -s 524288 -d html -p html_data $@

.PHONY: clean all $(SUBDIRS)
.PRECIOUS: %.rel %.ihx .img
