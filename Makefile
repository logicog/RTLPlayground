VERSION=0.1.0
IMAGESIZE = 524288
DEFAULT_CONFIG_LOCATION = 454656
CONFIG_LOCATION = 458752
HTML_LOCATION = 262144

# Startup configuration embedded in the image. Override it to build a specific
# machine with its own configuration: make MACHINE=<name> CONFIG=<file>
CONFIG ?= config.txt

ifneq ($(strip $(PCB)),)
ifneq ($(strip $(MACHINE)),)
$(error Specify only one of MACHINE or PCB)
endif
MACHINE := GENERIC_$(PCB)
GENERIC_PCB_BUILD := 1
endif

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
# Keep default constants in the fixed HOME code area.
CC_FLAGS = -mmcs51 -I. -Imachine -Ihttpd -Iuip
MACHINE_CC_FLAGS =
ASM ?= sdas8051
AFLAGS= -plosgff

SUBDIRS := tools
SUBDIRSCLEAN=$(addsuffix clean,$(SUBDIRS))
MACHINE_REGISTRY = machine/machines.json

# $MACHINE selects a JSON machine description and is passed as -DMACHINE_NAME.
# Its friendly_name and PCB fields select the PCB source and compiler flags.
# Alternatively, PCB selects a PCB source and gives it a generic machine name.
ifneq ($(MACHINE),)
	MACHINE_CC_FLAGS += -DMACHINE_NAME=$(MACHINE)
endif
ifeq ($(GENERIC_PCB_BUILD),1)
	MACHINE_SRC = machine/pcb/$(PCB).c
	MACHINE_PCB_SOURCE = $(MACHINE_SRC)
	FRIENDLY_NAME = GENERIC_PCB
	MACHINE_PCB_NAME = $(PCB)
	MACHINE_LINK_TARGET := $(shell readlink $(MACHINE_SRC) 2>/dev/null)
	ifneq ($(MACHINE_LINK_TARGET),)
		MACHINE_PCB_NAME = $(basename $(notdir $(MACHINE_LINK_TARGET)))
	endif
else ifneq ($(strip $(MACHINE)),)
	MACHINE_DESCRIPTOR = $(MACHINE_REGISTRY)
	ifneq ($(wildcard $(MACHINE_DESCRIPTOR)),)
	FRIENDLY_NAME := $(shell jq -er --arg machine "$(MACHINE)" '[.[] | select(.machine == $$machine)] | if length == 1 then .[0].friendly_name else error("expected exactly one machine entry") end' $(MACHINE_DESCRIPTOR))
	MACHINE_PCB_NAME := $(shell jq -er --arg machine "$(MACHINE)" '[.[] | select(.machine == $$machine)] | if length == 1 then .[0].pcb else error("expected exactly one machine entry") end' $(MACHINE_DESCRIPTOR))
	ifeq ($(strip $(FRIENDLY_NAME)),)
	$(error MACHINE='$(MACHINE)' has no valid entry in $(MACHINE_DESCRIPTOR))
	endif
	MACHINE_SRC = machine/pcb/$(MACHINE_PCB_NAME).c
	MACHINE_PCB_SOURCE = $(MACHINE_SRC)
	else
	$(error Machine registry $(MACHINE_DESCRIPTOR) does not exist)
	endif
else
	MACHINE_SRC =
	MACHINE_PCB_SOURCE =
endif

MACHINE_INIT_DEFAULT := machine/default_machine_init.c
ifneq ($(strip $(MACHINE_PCB_SOURCE)),)
MACHINE_HAS_CUSTOM_INIT := $(shell grep -q 'MACHINE_CUSTOM_INIT' $(MACHINE_PCB_SOURCE) 2>/dev/null && echo yes)
ifeq ($(MACHINE_HAS_CUSTOM_INIT),)
	MACHINE_INIT_SRC := $(MACHINE_INIT_DEFAULT)
endif
endif

# PCB information
ifneq ($(strip $(MACHINE_PCB_SOURCE)),)
MACHINE_CHIPSET := $(shell sed -n 's/^#define MACHINE_CHIPSET //p' $(MACHINE_PCB_SOURCE) 2>/dev/null | head -n 1)
else
MACHINE_CHIPSET =
endif
ifneq ($(strip $(MACHINE)),)
ifneq ($(wildcard $(MACHINE_PCB_SOURCE)),)
ifeq ($(strip $(MACHINE_CHIPSET)),)
$(error MACHINE_CHIPSET is missing from $(MACHINE_PCB_SOURCE))
endif
	MACHINE_CC_FLAGS += -DMACHINE_CHIPSET=$(MACHINE_CHIPSET)
ifeq ($(strip $(FRIENDLY_NAME)),)
$(error friendly_name is missing from $(MACHINE_DESCRIPTOR))
endif
	MACHINE_CC_FLAGS += -DFRIENDLY_NAME='"$(FRIENDLY_NAME)"'
ifneq ($(MACHINE_PCB_NAME),)
	MACHINE_CC_FLAGS += -DMACHINE_PCB_NAME=$(MACHINE_PCB_NAME)
endif
endif
endif
# Health instrumentation and the "health" console command: HEALTH=1 gmake ...
ifneq ($(HEALTH),)
	CC_FLAGS += -DHEALTH
endif
ifneq ($(DEBUG),)
	CC_FLAGS += -DDEBUG
endif

ifeq ($(CI),1)
	CC_FLAGS += --Werror
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

# Path and file name of the final image. BUILDDIR still holds the intermediate
# objects and tools. OUTPUT_DIR is the destination directory. OUTPUT_PREFIX is
# prepended to the automatic file name (version, git hash and machine);
# OUTPUT_NAME replaces that whole name and takes precedence, so use only one of
# OUTPUT_PREFIX and OUTPUT_NAME. Both refer to OUTPUT_DIR. OUTPUT sets a
# complete path and overrides all three:
# make MACHINE=<name> OUTPUT_DIR=/path/to/firmware
# make MACHINE=<name> OUTPUT_DIR=/path/to/firmware OUTPUT_PREFIX=my-switch-
# make MACHINE=<name> OUTPUT_DIR=/path/to/firmware OUTPUT_NAME=my-switch.bin
# make MACHINE=<name> OUTPUT=/path/to/firmware.bin
OUTPUT_DIR ?= $(BUILDDIR)
OUTPUT_PREFIX ?= rtlplayground-
OUTPUT_NAME ?= $(OUTPUT_PREFIX)$(FILENAME_EXTENSION).bin
OUTPUT ?= $(OUTPUT_DIR)/$(OUTPUT_NAME)

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

# $(MACHINE_DESCRIPTOR), $(MACHINE_SRC) and $(CONFIG) are listed here so an
# unknown $MACHINE, missing PCB source or missing config fails before anything
# else is built, including the tools/ subdirectory.
ifeq ($(strip $(MACHINE)),)
all:
	@echo "ERROR: Specify MACHINE=<name> or PCB=<name>." >&2
	@echo "       See machine/machines.json and machine/pcb/*.c for supported targets." >&2
	@false
else
all: create_build_dir $(VERSION_HEADER) $(MACHINE_DESCRIPTOR) $(MACHINE_SRC) $(CONFIG) $(SUBDIRS) $(OUTPUT)
endif

bank_size:
	tools/bank_occupancy.sh "$(MACHINE)"

create_build_dir: $(SDCC)
	mkdir -p "$(BUILDDIR)"
	mkdir -p "$(BUILDDIR)/uip"
	mkdir -p "$(BUILDDIR)/httpd"
	mkdir -p "$(BUILDDIR)/machine"
	mkdir -p "$(BUILDDIR)/machine/pcb"
	mkdir -p "$(BUILDDIR)/crypto"

$(MACHINE_REGISTRY):
	@echo "ERROR: machine registry is missing (expected $(MACHINE_REGISTRY))." >&2
	@false

# Machine builds compile the PCB source named in machine/machines.json.
# Generic PCB builds use machine/pcb/<PCB>.c directly.
ifeq ($(GENERIC_PCB_BUILD),1)
$(MACHINE_SRC):
	@echo "ERROR: PCB='$(PCB)' has no source (expected $(MACHINE_SRC))." >&2
	@false
else
ifneq ($(strip $(MACHINE)),)
ifneq ($(strip $(MACHINE_SRC)),)
$(MACHINE_SRC):
	@echo "ERROR: selected machine has no PCB source (expected $(MACHINE_SRC))." >&2
	@echo "       See machine/machines.json and machine/pcb/*.c for supported machines." >&2
	@echo "       Example: make MACHINE=KP_9000_6XHML_X2" >&2
	@false
endif
endif
endif

# Fail with a clear message when CONFIG names a file that does not exist.
$(CONFIG):
	@echo "ERROR: CONFIG='$(CONFIG)' does not exist." >&2
	@echo "       Point CONFIG at a configuration file, or omit it to use config.txt." >&2
	@false

# Keep the selected PCB source first so an invalid $MACHINE value fails early.
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
	timer.c \
	udp_apps.c \
	crypto/chacha20.c

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
	httpd/html_out.c \
	httpd/page_stp.c
SRCS += \
	uip/uip.c \
	uip/uiplib.c \
	uip/uip_arp.c \
	uip/uip-fw.c \
	uip/uip-neighbor.c \
	uip/uip-split.c

OBJS = ${SRCS:%.c=$(BUILDDIR)/%.rel}
MACHINE_OBJ = $(BUILDDIR)/$(MACHINE_SRC:.c=.rel)
STARTUP_OBJ = $(BUILDDIR)/rtlplayground.rel
DEPS := ${SRCS:%.c=$(BUILDDIR)/%.d}
HTML := $(shell find html -name '*.html' -or -name '*.svg' -or -name '*.css' -or -name '*.ico')

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

$(HTML_MIN_STAMP): $(HTML) html/app.js $(HTML_MIN_LIST) tools/minify.py tools/rjsmin.py $(if $(wildcard $(HTML_MIN)),,FORCE)
	rm -rf $(HTML_MIN)
	mkdir -p $(HTML_MIN)
	@for f in $(HTML); do python3 tools/minify.py $$f $(HTML_MIN)/$$(basename $$f) || exit 1; done
	python3 tools/rjsmin.py < html/app.js > $(HTML_MIN)/app.js || exit 1
	@touch $@

html_data.c html_data.h &: $(HTML_MIN_STAMP) | tools
	tools/$(BUILDDIR)/fileadder -a $(HTML_LOCATION) -s $(IMAGESIZE) -l $(DEFAULT_CONFIG_LOCATION) -b BANK1 -z -d $(HTML_MIN) -p html_data

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
	@echo "Building the SDCC toolchain; this may take several minutes."
	sh sdcc/build.sh $(abspath $(SDCC_PREFIX))
endif

$(SUBDIRSCLEAN):
	$(MAKE) -C $(@:clean=) clean

# Objects depend on the flags they were built with, through a stamp file that
# is rewritten only when the compiler flags actually change.
CCFLAGS_STAMP := $(BUILDDIR)/.ccflags

.PHONY: FORCE
FORCE:

$(CCFLAGS_STAMP): FORCE | create_build_dir
	@echo '$(CC_FLAGS) $(MACHINE_CC_FLAGS)' | cmp -s - $@ 2>/dev/null || echo '$(CC_FLAGS) $(MACHINE_CC_FLAGS)' > $@

# The same for $(CONFIG): its path is stamped so switching to a config file
# older than the image still rebuilds it, while $(CONFIG) itself as a normal
# prerequisite covers edits to the selected file.
$(CONFIG_STAMP): FORCE | create_build_dir
	@echo '$(CONFIG)' | cmp -s - $@ 2>/dev/null || echo '$(CONFIG)' > $@

$(BUILDDIR)/%.rel: %.c $(CCFLAGS_STAMP) | create_build_dir html_data.h
	$(CC) -MMD $(CC_FLAGS) $(MACHINE_CC_FLAGS) -o $@ -c $<

ifneq ($(strip $(MACHINE_DESCRIPTOR)),)
$(BUILDDIR)/machine/pcb/$(MACHINE_PCB_NAME).rel: $(MACHINE_DESCRIPTOR)
endif

$(BUILDDIR)/%.rel: %.asm $(CCFLAGS_STAMP) | create_build_dir
	${ASM} ${AFLAGS} -o $@ $<
#	mv -f $(addprefix $(basename $^), .lst .rel .sym) .

# Keep the reset vector first in HOME and append the machine descriptor last.
$(BUILDDIR)/rtlplayground.ihx: $(STARTUP_OBJ) $(filter-out $(STARTUP_OBJ) $(MACHINE_OBJ),$(OBJS)) $(BUILDDIR)/crtbank.rel $(BUILDDIR)/crc16.rel $(BUILDDIR)/crypto/chacha_8051.rel $(MACHINE_OBJ)
	$(CC) $(CC_FLAGS) $(MACHINE_CC_FLAGS) --xram-size 49151 -Wl-bHOME=0x00000 -Wl-bBANK1=0x14000 -Wl-bBANK2=0x24000 -Wl-bBANK3=0x34000 -Wl-bBANK4=0x44000 -Wl-r -o $@ $^

$(BUILDDIR)/rtlplayground.img: $(BUILDDIR)/rtlplayground.ihx
	objcopy --input-target=ihex -O binary $< $@

$(OUTPUT): $(BUILDDIR)/rtlplayground.img $(CONFIG) $(CONFIG_STAMP) | tools
	@mkdir -p "$(dir $@)"
	if [ -e "$@" ]; then rm "$@"; fi
	tools/$(BUILDDIR)/imagebuilder -i $(BUILDDIR)/rtlplayground.img "$@"
	tools/$(BUILDDIR)/fileadder -a $(DEFAULT_CONFIG_LOCATION) -s $(IMAGESIZE) -l $(CONFIG_LOCATION) -d "$(CONFIG)" "$@"
	tools/$(BUILDDIR)/fileadder -a $(CONFIG_LOCATION) -s $(IMAGESIZE) -l $(IMAGESIZE) -d "$(CONFIG)" "$@"
	tools/$(BUILDDIR)/fileadder -a $(HTML_LOCATION) -s $(IMAGESIZE) -l $(DEFAULT_CONFIG_LOCATION) -z -d $(HTML_MIN) -b BANK1 "$@"
	tools/$(BUILDDIR)/crc_calculator -u "$@"
	if [ "$(OUTPUT)" = "$(BUILDDIR)/rtlplayground-$(FILENAME_EXTENSION).bin" ]; then \
		ln -sf $(MACHINE)/rtlplayground-$(FILENAME_EXTENSION).bin "$(BUILDDIR)/../rtlplayground.bin"; \
	fi

.PHONY: clean distclean all $(SUBDIRS) $(SUBDIRSCLEAN) create_build_dir

# Every machine description is checked by compiling its selected PCB source.
MACHINE_DEFS := $(shell jq -er 'if type == "array" then .[].machine else error("expected an array") end' $(MACHINE_REGISTRY))

.PHONY: machine_check
machine_check: $(SDCC) $(MACHINE_REGISTRY)
	@mkdir -p $(BUILDDIR)/tmp
	@set -eo pipefail; \
	test -n "$(MACHINE_DEFS)" || { echo "ERROR: No machine entries found in $(MACHINE_REGISTRY)." >&2; exit 1; }; \
	for MACHINE in $(MACHINE_DEFS); \
	do \
	echo "Checking $${MACHINE}"; \
	DESCRIPTION=$(MACHINE_REGISTRY); \
	FRIENDLY_NAME=$$(jq -er --arg machine "$${MACHINE}" '.[] | select(.machine == $$machine) | .friendly_name' $$DESCRIPTION); \
	PCB=$$(jq -er --arg machine "$${MACHINE}" '.[] | select(.machine == $$machine) | .pcb' $$DESCRIPTION); \
	CHIPSET_SOURCE=machine/pcb/$${PCB}.c; \
	CHIPSET=$$(sed -n 's/^#define MACHINE_CHIPSET //p' $$CHIPSET_SOURCE 2>/dev/null | head -n 1); \
	test -n "$$CHIPSET" || { echo "ERROR: $$CHIPSET_SOURCE has no MACHINE_CHIPSET definition." >&2; exit 1; }; \
	$(CC) $(CC_FLAGS) -DMACHINE_NAME=$${MACHINE} -DMACHINE_CHIPSET=$$CHIPSET "-DFRIENDLY_NAME=\"$$FRIENDLY_NAME\"" -DMACHINE_PCB_NAME=$${PCB} -MMD -o $(BUILDDIR)/tmp/machine_check -c $$CHIPSET_SOURCE; \
	if ! grep -q 'MACHINE_CUSTOM_INIT(' $$CHIPSET_SOURCE; then \
		$(CC) $(CC_FLAGS) -DMACHINE_NAME=$${MACHINE} -DMACHINE_CHIPSET=$$CHIPSET "-DFRIENDLY_NAME=\"$$FRIENDLY_NAME\"" -DMACHINE_PCB_NAME=$${PCB} -MMD -o $(BUILDDIR)/tmp/machine_check -c $(MACHINE_INIT_DEFAULT); \
	fi; \
	done
	@rm -rf $(BUILDDIR)/tmp

-include $(DEPS)
