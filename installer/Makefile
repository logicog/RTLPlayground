CODE_LOCATION=0x1000
INSTALLER_ADDRESS=0x1100

CC = sdcc
CC_FLAGS = -mmcs51
ASM = sdas8051
AFLAGS= -plosgff

SRCS = installer.c
OBJS = ${SRCS:.c=.rel}

all: updatebuilder rtlplayground.bin

updatebuilder: updatebuilder.c
	gcc $^ -o $@

installer.rel: installer.c
	$(CC) $(CC_FLAGS) --code-loc ${CODE_LOCATION} -c $<

%.rel: %.asm
	${ASM} ${AFLAGS} $^

%.rel: %.c
	$(CC) $(CC_FLAGS) -c $<

rtlinstaller.ihx:  crtstart.rel $(OBJS)
	$(CC) $(CC_FLAGS) -Wl-bHOME=${INSTALLER_ADDRESS} -Wl-r -o $@ $^

rtlplayground.bin: rtlinstaller.ihx ../rtlplayground.bin
	cp ../rtlplayground.bin .
	./updatebuilder -i $< rtlplayground.bin

clean:
	rm *.ihx *.lst *.rel *.rst *.sym *.lst *.map *.mem *.lk $(OBJS) installer.asm
