CODE_LOCATION=0x1000
INSTALLER_ADDRESS=0x1100

CC = sdcc
CC_FLAGS = -mmcs51
ASM = sdas8051
AFLAGS= -plosgff

BUILDDIR = output/

SRCS = installer.c
OBJS = ${SRCS:%.c=$(BUILDDIR)%.rel}

all: create_build_dir $(BUILDDIR)updatebuilder $(BUILDDIR)rtlplayground.bin

create_build_dir:
	mkdir -p $(BUILDDIR)

$(BUILDDIR)updatebuilder: updatebuilder.c
	gcc $^ -o $@

$(BUILDDIR)installer.rel: installer.c
	$(CC) $(CC_FLAGS) --code-loc ${CODE_LOCATION} -o $@ -c $<

$(BUILDDIR)crtstart.rel: crtstart.asm
	$(ASM) $(AFLAGS) -o $@ $<

$(BUILDDIR)%.rel: $(BUILDDIR)%.asm
	${ASM} ${AFLAGS} -o $@ $^

$(BUILDDIR)%.rel: %.c
	$(CC) $(CC_FLAGS) -o $@ -c $<

$(BUILDDIR)rtlinstaller.ihx: $(BUILDDIR)crtstart.rel $(OBJS)
	$(CC) $(CC_FLAGS) -Wl-bHOME=${INSTALLER_ADDRESS} -Wl-r -o $@ $^

$(BUILDDIR)rtlplayground.bin: $(BUILDDIR)rtlinstaller.ihx ../$(BUILDDIR)/rtlplayground.bin
	cp ../$(BUILDDIR)/rtlplayground.bin $(BUILDDIR)
	./$(BUILDDIR)/updatebuilder -i $< $(BUILDDIR)rtlplayground.bin

clean:
	rm -r $(BUILDDIR)
