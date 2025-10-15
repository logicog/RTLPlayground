CC = sdcc
CC_FLAGS = -mmcs51 -I. -I../uip
ASM = sdas8051
AFLAGS= -plosgff

BUILDDIR = output/

SRCS = httpd.c page_impl.c
OBJS = ${SRCS:%.c=$(BUILDDIR)%.rel}

all: create_build_dir $(OBJS)

create_build_dir:
	mkdir -p $(BUILDDIR)

$(BUILDDIR)%.asm: %.c
	$(CC) $(CC_FLAGS) -o $@ -c -S $<

$(BUILDDIR)%.rel: $(BUILDDIR)%.asm
	${ASM} ${AFLAGS} -o $@ $^

clean:
	rm -r $(BUILDDIR)

.PHONY: all clean

