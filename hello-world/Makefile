CC = sdcc
CC_FLAGS = -mmcs51 -I. -I../uip
ASM = sdas8051
AFLAGS= -plosgff

SRCS = hello-world.c
OBJS = ${SRCS:.c=.rel}

all: $(OBJS)

%.rel: %.c
	$(CC) $(CC_FLAGS) -c $<

%.rel: %.asm
	${ASM} ${AFLAGS} $^

clean:
	rm *.ihx *.lk *.lst *.map *.mem *.rel *.rst *.sym *.bin

.PHONY: all clean

