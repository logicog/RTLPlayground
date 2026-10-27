VERSION=0.1.0
IMAGESIZE = 524288
DEFAULT_CONFIG_LOCATION = 454656
CONFIG_LOCATION = 458752
HTML_LOCATION = 262144

# Startup configuration embedded in the image. Override it to build a specific
# machine with its own configuration: make MACHINE=<name> CONFIG=<file>
CONFIG ?= config.txt

SDCC_TOOLCHAIN = sdcc-$(shell cat sdcc/* | cksum | cut -d' ' -f1)
SDCC_PREFIX = toolchain/$(SDCC_TOOLCHAIN)
ifeq ($(origin CC),default)
SDCC = $(SDCC_PREFIX)/bin/sdcc
CC = $(SDCC)
ASM = $(SDCC_PREFIX)/bin/sdas8051
SDCC_REV := $(shell git log -1 --format=%h -- sdcc 2>/dev/null)
ifneq ($(SDCC_REV),)
ifneq ($(shell git status --porcelain --untracked-files=no -- sdcc 2>/dev/null),)
SDCC_REV := $(SDCC_REV)-dirty
endif
endif
SDCC_BANNER = $(if $(SDCC_REV),sdcc/ at $(SDCC_REV),$(SDCC_TOOLCHAIN)) ($(words $(wildcard sdcc/*.patch)) patches)
else
SDCC_BANNER = system
endif
CC_FLAGS = -mmcs51 -I. -Imachine -Ihttpd -Iuip
ASM ?= sdas8051
AFLAGS= -plosgff

SUBDIRS := tools
SUBDIRSCLEAN=$(addsuffix clean,$(SUBDIRS))

# $MACHINE selects machine/$(MACHINE).c as the board definition and is passed
# on as -DMACHINE_NAME, which machine.h uses to include machine/$(MACHINE).h.
# Board selection itself needs no MACHINE_* symbol for #if chains, so the
# definition is all the compiler has to know.
ifneq ($(MACHINE),)
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

MACHINE_INIT_DEFAULT := machine/default_machine_init.c
MACHINE_INIT_SRC := $(wildcard machine/$(MACHINE)_init.c)
ifeq ($(MACHINE_INIT_SRC),)
	MACHINE_INIT_SRC := $(MACHINE_INIT_DEFAULT)
endif

ifndef BUILDDIR
	BUILDDIR = output/$(MACHINE)
endif

CONFIG_STAMP := $(BUILDDIR)/.config
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

# $(MACHINE_SRC) and $(CONFIG) are listed here so an unknown $MACHINE or a
# missing config file fails before anything else is built, including the tools/
# subdirectory.
all: create_build_dir $(VERSION_HEADER) $(MACHINE_SRC) $(CONFIG) $(SUBDIRS) $(BUILDDIR)/rtlplayground-$(FILENAME_EXTENSION).bin

bank_size:
	tools/bank_occupancy.sh "$(MACHINE)"

create_build_dir: $(SDCC)
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

# Fail with a clear message when CONFIG names a file that does not exist.
$(CONFIG):
	@echo "ERROR: CONFIG='$(CONFIG)' does not exist." >&2
	@echo "       Point CONFIG at a configuration file, or omit it to use config.txt." >&2
	@false

# Keep the per-machine definition in first position to fail immediately on an
# invalid $MACHINE value
SRCS = \
	$(MACHINE_SRC) \
	$(MACHINE_INIT_SRC) \
	cmd_editor.c \
	cmd_parser.c \
	dhcp.c \
	html_data.c \
	rtlplayground.c \
	boot.c \
	sfp.c \
	dns.c \
	dns_cli.c \
	sessions.c \
	ntp.c \
	ntp_cli.c \
	syslog.c \
	udp_apps.c

# RTL837x
SRCS += \
	rtl837x_bandwidth.c \
	rtl837x_cabletest.c \
	rtl837x_flash.c \
	rtl837x_igmp.c \
	rtl837x_init.c \
	rtl837x_nic.c \
	rtl837x_leds.c \
	rtl837x_leds_dump.c \
	rtl837x_phy.c \
	rtl837x_pins.c\
	rtl837x_port.c \
	rtl837x_stp.c \
	rtl837x_lldp.c \
	rtl837x_storm.c \
	rtl837x_lacp.c \
	rtl837x_trap.c \
	rtl837x_stp_cli.c
SRCS += \
	httpd/httpd.c \
	httpd/page_impl.c \
	httpd/page_stp.c
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
HTML_MIN := $(BUILDDIR)/html_min
HTML_MIN_LIST := $(BUILDDIR)/html_min.list
HTML_MIN_STAMP := $(BUILDDIR)/html_min.stamp
.PHONY: html_min
html_min: $(HTML_MIN_STAMP)

$(HTML_MIN_LIST): FORCE
	@mkdir -p $(BUILDDIR)
	@echo $(sort $(HTML)) > $@.tmp
	@cmp -s $@.tmp $@ && rm -f $@.tmp || mv $@.tmp $@

$(HTML_MIN_STAMP): $(HTML) $(HTML_MIN_LIST) tools/minify.py $(if $(wildcard $(HTML_MIN)),,FORCE)
	rm -rf $(HTML_MIN)
	mkdir -p $(HTML_MIN)
	@for f in $(HTML); do python3 tools/minify.py $$f $(HTML_MIN)/$$(basename $$f) || exit 1; done
	@touch $@

html_data.c html_data.h &: $(HTML_MIN_STAMP) | tools
	tools/$(BUILDDIR)/fileadder -a $(HTML_LOCATION) -s $(IMAGESIZE) -b BANK1 -z -d $(HTML_MIN) -p html_data

$(VERSION_HEADER): FORCE
	@printf '%s\n' "#ifndef VERSION_H" "#define VERSION_H" \
		"#define VERSION_SW \"$(VERSION_EXTENSION)\"" \
		"#define BUILD_DATE \"$(BUILD_DATE)\"" \
		"#define SDCC_TOOLCHAIN \"$(SDCC_BANNER)\"" \
		"#endif" > $(VERSION_HEADER).tmp
	@cmp -s $(VERSION_HEADER).tmp $(VERSION_HEADER) \
		&& rm -f $(VERSION_HEADER).tmp \
		|| mv $(VERSION_HEADER).tmp $(VERSION_HEADER)

httpd: html_data.h

$(SUBDIRS):
	$(MAKE) -C $@ BUILDDIR=$(BUILDDIR)

clean: $(SUBDIRSCLEAN)
	-rm -f html_data.c html_data.h $(VERSION_HEADER) $(HTML_MIN_LIST) $(HTML_MIN_STAMP)
	-if [ -d $(BUILDDIR) ]; then find $(BUILDDIR) -type f ! -name "*.bin" -delete; fi

distclean: $(SUBDIRSCLEAN)
	-rm -f html_data.c html_data.h $(VERSION_HEADER) $(HTML_MIN_LIST) $(HTML_MIN_STAMP)
	-rm -rf $(BUILDDIR)
	-rm -rf toolchain

ifneq ($(SDCC),)
$(SDCC):
	sh sdcc/build.sh $(abspath $(SDCC_PREFIX))
endif

$(SUBDIRSCLEAN):
	$(MAKE) -C $(@:clean=) clean

# Objects depend on the flags they were built with, through a stamp file that
# is rewritten only when CC_FLAGS actually changes.
CCFLAGS_STAMP := $(BUILDDIR)/.ccflags

.PHONY: FORCE
FORCE:

$(CCFLAGS_STAMP): FORCE | create_build_dir
	@echo '$(CC_FLAGS)' | cmp -s - $@ 2>/dev/null || echo '$(CC_FLAGS)' > $@

# The same for $(CONFIG): its path is stamped so switching to a config file
# older than the image still rebuilds it, while $(CONFIG) itself as a normal
# prerequisite covers edits to the selected file.
$(CONFIG_STAMP): FORCE | create_build_dir
	@echo '$(CONFIG)' | cmp -s - $@ 2>/dev/null || echo '$(CONFIG)' > $@

$(BUILDDIR)/%.rel: %.c $(CCFLAGS_STAMP) | create_build_dir html_data.h
	$(CC) -MMD $(CC_FLAGS) -o $@ -c $<

$(BUILDDIR)/%.rel: %.asm $(CCFLAGS_STAMP) | create_build_dir
	${ASM} ${AFLAGS} -o $@ $<
#	mv -f $(addprefix $(basename $^), .lst .rel .sym) .

$(BUILDDIR)/rtlplayground.ihx: $(OBJS) $(BUILDDIR)/crtbank.rel $(BUILDDIR)/crc16.rel
	$(CC) $(CC_FLAGS) --xram-size 49151 -Wl-bHOME=0x00000 -Wl-bBANK1=0x14000 -Wl-bBANK2=0x24000 -Wl-bBANK3=0x34000 -Wl-bBANK4=0x44000 -Wl-r -o $@ $^

$(BUILDDIR)/rtlplayground.img: $(BUILDDIR)/rtlplayground.ihx
	objcopy --input-target=ihex -O binary $< $@

$(BUILDDIR)/rtlplayground-$(FILENAME_EXTENSION).bin: $(BUILDDIR)/rtlplayground.img $(CONFIG) $(CONFIG_STAMP) | tools
	if [ -e $@ ]; then rm $@; fi
	tools/$(BUILDDIR)/imagebuilder -i $(BUILDDIR)/rtlplayground.img $@
	tools/$(BUILDDIR)/fileadder -a $(DEFAULT_CONFIG_LOCATION) -s $(IMAGESIZE) -d "$(CONFIG)" $@
	tools/$(BUILDDIR)/fileadder -a $(CONFIG_LOCATION) -s $(IMAGESIZE) -d "$(CONFIG)" $@
	tools/$(BUILDDIR)/fileadder -a $(HTML_LOCATION) -s $(IMAGESIZE) -z -d $(HTML_MIN) -b BANK1 $@
	tools/$(BUILDDIR)/crc_calculator -u $@
	ln -sf $(MACHINE)/rtlplayground-$(FILENAME_EXTENSION).bin $(BUILDDIR)/../rtlplayground.bin

.PHONY: clean distclean all $(SUBDIRS) $(SUBDIRSCLEAN) create_build_dir

# Every machine/ definition is compiled, so a machine added to machine/ is
# covered without registering it anywhere. A board only counts once both its
# <machine>.c and its <machine>.h are present: the header is what the rest of
# the build includes, so a .c without one is an incomplete board. The _init.c
# files are excluded here: they are the boot hooks, compiled separately below
# (the board's own if it exists, otherwise default_machine_init.c). The
# machine-specific defines are filtered out of $(CC_FLAGS) and supplied per
# iteration instead, so this also works as "make machine_check MACHINE=...".
MACHINE_DEFS := $(sort $(filter $(patsubst machine/%.h,%,$(wildcard machine/*.h)),$(patsubst machine/%.c,%,$(filter-out machine/%_init.c,$(wildcard machine/*.c)))))
MACHINE_LESS_CC_FLAGS = $(filter-out -DMACHINE_%,$(CC_FLAGS))

.PHONY: machine_check
machine_check: $(SDCC)
	@mkdir -p $(BUILDDIR)/tmp
	@set -eo pipefail; \
	for MACHINE in $(MACHINE_DEFS); \
	do \
	echo "Checking $${MACHINE}"; \
	$(CC) $(MACHINE_LESS_CC_FLAGS) -DMACHINE_NAME=$${MACHINE} -MMD -o $(BUILDDIR)/tmp/machine_check -c machine/$${MACHINE}.c; \
	INIT=machine/$${MACHINE}_init.c; \
	if [ ! -e $$INIT ] && [ ! -L $$INIT ]; then INIT=$(MACHINE_INIT_DEFAULT); fi; \
	$(CC) $(MACHINE_LESS_CC_FLAGS) -DMACHINE_NAME=$${MACHINE} -MMD -o $(BUILDDIR)/tmp/machine_check -c $$INIT; \
	done
	@rm -rf $(BUILDDIR)/tmp

-include $(DEPS)
