CC = sdcc
CC_FLAGS = -mmcs51 -I. -I../uip
ASM = sdas8051
AFLAGS= -plosgff

SRCS = httpd.c page_impl.c
OBJS = ${SRCS:.c=.rel}

all: $(OBJS)

%.asm: %.c
	$(CC) $(CC_FLAGS) -c -S $<

%.rel: %.asm
	./treatasm.py $^ >$^.new
	mv $^.new $^
	${ASM} ${AFLAGS} $^

clean:
	rm .asm *.lst *.rel *.rst *.sym

.PHONY: all clean

