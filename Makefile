BOOTLOADER_ADDRESS=0x100

CC = sdcc
CC_FLAGS = -mmcs51
ASM = sdas8051
AFLAGS= -plosgff

all: rtlplayground.bin injector

SRCS= rtlplayground.c rtl837x_flash.c rtl837x_phy.c
OBJS= ${SRCS:.c=.rel}

clean:
	if [ -e rtlplayground.bin ]; then rm rtlplayground.bin; fi
	if [ -e rtlplayground.asm ]; then rm rtlplayground.asm; fi
	rm *.ihx *.lk *.lst *.map *.mem *.rel *.rst *.sym *.bin

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

injector: injector.c
	gcc $^ -o $@

.PHONY: clean all
.PRECIOUS: %.rel %.ihx .img
