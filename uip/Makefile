CC = sdcc
CC_FLAGS = -mmcs51 -I. -I.. -I../httpd
ASM = sdas8051
AFLAGS= -plosgff

BUILDDIR = output/

SRCS = timer.c uip_arp.c uip.c uip-fw.c uiplib.c uip-split.c
OBJS = ${SRCS:%.c=$(BUILDDIR)%.rel}

all: create_build_dir $(OBJS)

create_build_dir:
	mkdir -p $(BUILDDIR)

$(BUILDDIR)%.rel: %.c
	$(CC) $(CC_FLAGS) -o $@ -c $<

$(BUILDDIR)%.rel: $(BUILDDIR)%.asm
	${ASM} ${AFLAGS} $^

clean:
	rm -r $(BUILDDIR)

.PHONY: all clean

