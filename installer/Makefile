INSTALLER_ADDRESS=0x1016

CC = sdcc
CC_FLAGS = -mmcs51
ASM = sdas8051
AFLAGS= -plosgff

SRCS = installer.c
OBJS = ${SRCS:.c=.rel}

all: updatebuilder rtlinstall.bin

updatebuilder: updatebuilder.c
	gcc $^ -o $@

%.rel: %.c
	$(CC) $(CC_FLAGS) -–code-loc 0x1000 -c $<

%.rel: %.asm
	${ASM} ${AFLAGS} $^

%.rel: %.c
	$(CC) $(CC_FLAGS) -c $<

rtlinstall.ihx:  crtstart.rel $(OBJS) 
	$(CC) $(CC_FLAGS) -Wl-bHOME=${INSTALLER_ADDRESS} -Wl-r -o $@ $^

%.bin: %.ihx
	objcopy --input-target=ihex -O binary $< $@

clean:
	rm *.lst *.rel *.rst *.sym *.lst *.map *.mem *.lk $(OBJS)

