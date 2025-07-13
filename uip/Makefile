CC = sdcc
CC_FLAGS = -mmcs51 -I. -I../httpd
ASM = sdas8051
AFLAGS= -plosgff

SRCS = timer.c  uip_arp.c  uip.c  uip-fw.c  uiplib.c  uip-neighbor.c  uip-split.c
OBJS = ${SRCS:.c=.rel}

all: $(OBJS)

%.rel: %.c
	$(CC) $(CC_FLAGS) -c $<

%.rel: %.asm
	${ASM} ${AFLAGS} $^

clean:
	rm *.ihx *.lk *.lst *.map *.mem *.rel *.rst *.sym *.bin

.PHONY: all clean

