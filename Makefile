VERSION=0.1.0
IMAGESIZE = 524288
DEFAULT_CONFIG_LOCATION = 454656
CONFIG_LOCATION = 458752
HTML_LOCATION = 262144

ifeq ($(origin CC),default)
CC = sdcc
endif
CC_FLAGS = -mmcs51 -I. -Imachine -Ihttpd -Iuip
ASM ?= sdas8051
AFLAGS= -plosgff

SUBDIRS := tools
SUBDIRSCLEAN=$(addsuffix clean,$(SUBDIRS))

# $MACHINE selects machine/$(MACHINE).c as the board definition and is passed
# on as -DMACHINE_$(MACHINE), which the rest of the firmware tests with
# #if defined(MACHINE_...), and as -DMACHINE_NAME, which machine.h uses to
# include machine/$(MACHINE).h. Both take the same value, so they cannot drift.
ifneq ($(MACHINE),)
	CC_FLAGS += -DMACHINE_$(MACHINE)
	CC_FLAGS += -DMACHINE_NAME=$(MACHINE)
endif
# Health instrumentation and the "health" console command: HEALTH=1 gmake ...
ifneq ($(HEALTH),)
	CC_FLAGS += -DHEALTH
endif

ifeq ($(CI),1)
	CC_FLAGS += --Werror
endif

# The board definition being built: one file per MACHINE_* symbol in machine.h.
MACHINE_SRC = machine/$(MACHINE).c

BUILDDIR = output/$(MACHINE)
VERSION_HEADER := version.h

GIT_VERSION := $(shell git rev-parse --short HEAD)
ifeq ($(shell git status --porcelain --untracked-files=no),)
else
	GIT_VERSION := $(GIT_VERSION)-dirty
endif

VERSION_EXTENSION = v$(VERSION)-$(GIT_VERSION)
FILENAME_EXTENSION = $(VERSION_EXTENSION)-$(MACHINE)

# Deterministic build date: honor SOURCE_DATE_EPOCH, else the HEAD commit date,
# else wall-clock (no-git fallback). Keeps same-commit builds byte-identical
# (BUILD_DATE is baked into the image and covered by the trailing CRC).
SOURCE_DATE_EPOCH ?= $(shell git show -s --format=%ct HEAD 2>/dev/null)
ifeq ($(SOURCE_DATE_EPOCH),)
BUILD_DATE := $(shell date +"%Y-%m-%d %H:%M:%S")
else
BUILD_DATE := $(shell date -u -d @$(SOURCE_DATE_EPOCH) +"%Y-%m-%d %H:%M:%S" 2>/dev/null \
	|| date -u -r $(SOURCE_DATE_EPOCH) +"%Y-%m-%d %H:%M:%S")
endif

# $(MACHINE_SRC) is listed here so an unknown $MACHINE fails before anything else
# is built, including the tools/ subdirectory.
all: create_build_dir $(VERSION_HEADER) $(MACHINE_SRC) $(SUBDIRS) $(BUILDDIR)/rtlplayground-$(FILENAME_EXTENSION).bin

create_build_dir:
	mkdir -p "$(BUILDDIR)"
	mkdir -p "$(BUILDDIR)/uip"
	mkdir -p "$(BUILDDIR)/httpd"
	mkdir -p "$(BUILDDIR)/machine"

# Every machine is a pair machine/<MACHINE>.c + machine/<MACHINE>.h. Selecting
# them by name means no #if/#elif chain, so an unknown or empty $MACHINE is
# caught here instead of by a #error further down the build.
$(MACHINE_SRC):
	@echo "ERROR: MACHINE='$(MACHINE)' has no definition (expected $(MACHINE_SRC))." >&2
	@echo "       Every machine is one file pair under machine/, named after the" >&2
	@echo "       MACHINE_* symbol it defines; see machine/*.c for the full list." >&2
	@echo "       Example: make MACHINE=KP_9000_6XHML_X2" >&2
	@false

# Keep the per-machine definition in first position to fail immediately on an
# invalid $MACHINE value
SRCS = \
	$(MACHINE_SRC) \
	machine_init.c \
	cmd_editor.c \
	cmd_parser.c \
	dhcp.c \
	html_data.c \
	rtlplayground.c \
	boot.c \
	sfp.c \
	syslog.c \
	udp_apps.c

# RTL837x
SRCS += \
	rtl837x_bandwidth.c \
	rtl837x_flash.c \
	rtl837x_igmp.c \
	rtl837x_init.c \
	rtl837x_leds.c \
	rtl837x_leds_dump.c \
	rtl837x_phy.c \
	rtl837x_pins.c\
	rtl837x_port.c \
	rtl837x_stp.c \
	rtl837x_storm.c
SRCS += \
	httpd/httpd.c \
	httpd/page_impl.c
SRCS += \
	uip/timer.c \
	uip/uip.c \
	uip/uiplib.c \
	uip/uip_arp.c \
	uip/uip-fw.c \
	uip/uip-neighbor.c \
	uip/uip-split.c

OBJS = ${SRCS:%.c=$(BUILDDIR)/%.rel}
DEPS := ${SRCS:%.c=$(BUILDDIR)/%.d}
HTML := $(shell find html -name '*.js' -or -name '*.html' -or -name '*.svg' -or -name '*.css' -or -name '*.ico')

# Minified copy of the web UI sources, used as the fileadder input.
# The raw html/ sources stay untouched for development; the minified
# copy is a build artifact under output/.
HTML_MIN := output/html_min
.PHONY: html_min
html_min: $(HTML)
	rm -rf $(HTML_MIN)
	mkdir -p $(HTML_MIN)
	@for f in $(HTML); do python3 tools/minify.py $$f $(HTML_MIN)/$$(basename $$f) || exit 1; done

html_data.c html_data.h &: $(HTML) | tools html_min
	tools/output/fileadder -a $(HTML_LOCATION) -s $(IMAGESIZE) -b BANK1 -z -d $(HTML_MIN) -p html_data

$(VERSION_HEADER): FORCE
	@printf '%s\n' "#ifndef VERSION_H" "#define VERSION_H" \
		"#define VERSION_SW \"$(VERSION_EXTENSION)\"" \
		"#define BUILD_DATE \"$(BUILD_DATE)\"" \
		"#endif" > $(VERSION_HEADER).tmp
	@cmp -s $(VERSION_HEADER).tmp $(VERSION_HEADER) \
		&& rm -f $(VERSION_HEADER).tmp \
		|| mv $(VERSION_HEADER).tmp $(VERSION_HEADER)

httpd: html_data.h

$(SUBDIRS):
	$(MAKE) -C $@

clean: $(SUBDIRSCLEAN)
	-rm -f html_data.c html_data.h $(VERSION_HEADER)
	-if [ -d $(BUILDDIR) ]; then find $(BUILDDIR) -type f ! -name "*.bin" -delete; fi

distclean: $(SUBDIRSCLEAN)
	-rm -f html_data.c html_data.h $(VERSION_HEADER)
	-rm -rf $(BUILDDIR)

$(SUBDIRSCLEAN):
	$(MAKE) -C $(@:clean=) clean

# Objects depend on the flags they were built with, through a stamp file that
# is rewritten only when CC_FLAGS actually changes.
CCFLAGS_STAMP := $(BUILDDIR)/.ccflags

.PHONY: FORCE
FORCE:

$(CCFLAGS_STAMP): FORCE | create_build_dir
	@echo '$(CC_FLAGS)' | cmp -s - $@ 2>/dev/null || echo '$(CC_FLAGS)' > $@

$(BUILDDIR)/%.rel: %.c $(CCFLAGS_STAMP) | create_build_dir html_data.h
	$(CC) -MMD $(CC_FLAGS) -o $@ -c $<

$(BUILDDIR)/%.rel: %.asm $(CCFLAGS_STAMP) | create_build_dir
	${ASM} ${AFLAGS} -o $@ $<
#	mv -f $(addprefix $(basename $^), .lst .rel .sym) .

$(BUILDDIR)/rtlplayground.ihx: $(OBJS) $(BUILDDIR)/crtbank.rel $(BUILDDIR)/crc16.rel
	$(CC) $(CC_FLAGS) --xram-size 49151 -Wl-bHOME=0x00000 -Wl-bBANK1=0x14000 -Wl-bBANK2=0x24000 -Wl-bBANK3=0x34000 -Wl-r -o $@ $^

$(BUILDDIR)/rtlplayground.img: $(BUILDDIR)/rtlplayground.ihx
	objcopy --input-target=ihex -O binary $< $@

$(BUILDDIR)/rtlplayground-$(FILENAME_EXTENSION).bin: $(BUILDDIR)/rtlplayground.img | tools
	if [ -e $@ ]; then rm $@; fi
	tools/output/imagebuilder -i $^ $@
	tools/output/fileadder -a $(DEFAULT_CONFIG_LOCATION) -s $(IMAGESIZE) -d config.txt $@
	tools/output/fileadder -a $(CONFIG_LOCATION) -s $(IMAGESIZE) -d config.txt $@
	tools/output/fileadder -a $(HTML_LOCATION) -s $(IMAGESIZE) -z -d $(HTML_MIN) -b BANK1 $@
	tools/output/crc_calculator -u $@
	ln -sf $(MACHINE)/rtlplayground-$(FILENAME_EXTENSION).bin output/rtlplayground.bin

.PHONY: clean distclean all $(SUBDIRS) $(SUBDIRSCLEAN) create_build_dir

# Every machine/ definition is compiled, so a machine added to machine/ is
# covered without registering it anywhere. The machine-specific defines are
# filtered out of $(CC_FLAGS) and supplied per iteration instead, so this also
# works as "make machine_check MACHINE=...".
MACHINE_DEFS := $(sort $(patsubst machine/%.c,%,$(wildcard machine/*.c)))
MACHINE_LESS_CC_FLAGS = $(filter-out -DMACHINE_%,$(CC_FLAGS))

.PHONY:
machine_check:
	@mkdir -p $(BUILDDIR)/tmp
	@set -eo pipefail; \
	for MACHINE in $(MACHINE_DEFS); \
	do \
	echo "Checking $${MACHINE}"; \
	$(CC) $(MACHINE_LESS_CC_FLAGS) -DMACHINE_$${MACHINE} -DMACHINE_NAME=$${MACHINE} -MMD -o $(BUILDDIR)/tmp/machine_check -c machine/$${MACHINE}.c; \
	$(CC) $(MACHINE_LESS_CC_FLAGS) -DMACHINE_$${MACHINE} -DMACHINE_NAME=$${MACHINE} -MMD -o $(BUILDDIR)/tmp/machine_check -c machine_init.c; \
	done
	@rm -rf $(BUILDDIR)/tmp

-include $(DEPS)
