#ifndef _MACHINE_H_
#define _MACHINE_H_

#include <stdint.h>

#define MACHINE_STR_HELPER(x)  #x
#define MACHINE_STR(x) MACHINE_STR_HELPER(x)

#ifndef MACHINE_PCB_NAME
#define MACHINE_PCB_NAME unknown
#endif
#define MACHINE_PCB_INFO MACHINE_STR(MACHINE_PCB_NAME)

// MDI pair polarity of the four internal PHYs, one argument per PHY (port 1
// first). An invalid argument is caught at compile time the same way I2CBUS()
// does it.
//
// A board that does not care leaves the field at zero, which means "not
// overridden" and keeps the reset default. The bit cannot simply default to
// MDI_DEFAULT in MACHINE_COMMON_SETTING because SDCC rejects a board that
// overrides an already specified initializer (error 203).
#define MDI_DEFAULT  0x0c
#define MDI_CFG_SET  0x10
#define MDI_STRAIGHT 0
#define MDI_SWAPPED  1
#define MDI_CFG_VALID(p) (0 * sizeof(char[((p) == MDI_STRAIGHT || (p) == MDI_SWAPPED) ? 1 : 0]))
#define MDI_CFG(p1, p2, p3, p4) \
	((uint8_t)(MDI_CFG_SET | (p1) | ((p2) << 1) | ((p3) << 2) | ((p4) << 3) | \
		   MDI_CFG_VALID(p1) | MDI_CFG_VALID(p2) | MDI_CFG_VALID(p3) | MDI_CFG_VALID(p4)))
// Register value to write: the configured polarity, or the reset default.
#define MDI_CFG_VALUE(cfg) ((cfg) ? (uint8_t)((cfg) & 0x0f) : MDI_DEFAULT)


#ifndef MACHINE_CHIPSET
	#include "machine/default_machine.h"
#endif

#define RTL8370 0x70
#define RTL8372 0x72
#define RTL8373 0x73

#ifndef MACHINE_CHIPSET
#error "MACHINE_CHIPSET shall be defined by the selected PCB source or build"
#endif

#ifndef MACHINE_MIN_PORT
#if MACHINE_CHIPSET == RTL8372
#define MACHINE_MIN_PORT 3
#else
#define MACHINE_MIN_PORT 0
#endif
#endif

#ifndef MACHINE_MAX_PORT
#if MACHINE_CHIPSET == RTL8370
#define MACHINE_MAX_PORT 7
#else
#define MACHINE_MAX_PORT 8
#endif
#endif

#define MACHINE_PORT_VALID(p) \
	((uint8_t)((p) - MACHINE_MIN_PORT) <= (MACHINE_MAX_PORT - MACHINE_MIN_PORT))

// auto derive SOC_FAMILY
#define RTL836X 6
#define RTL837X 7

#ifdef SOC_FAMILY
#error "SOC_FAMILY can not be manually defined, shall be auto derived"
#endif
#if (MACHINE_CHIPSET == RTL8372) || (MACHINE_CHIPSET == RTL8373)
#define SOC_FAMILY RTL837X
#elif (MACHINE_CHIPSET == RTL8370 )
#define SOC_FAMILY RTL836X
#else
#error "Unsupported MACHINE_CHIPSET is specified!"
#endif

#define LED_27 1
// SYSTEM LED
#define LED_28_SYS 2
#define LED_29 4

struct high_leds {
	// Defines MUX and LED enabling for pins 27-29
	uint8_t mux : 3;
	uint8_t enable : 3;
	uint8_t reserved : 2;
};

struct sfp_port
{
	uint8_t pin_detect; // gpio number 0-63, 0xFF = don't have it?
	uint8_t pin_los; // gpio number 0-63, 0xFF = don't have it?
	uint8_t pin_tx_disable; // gpio number 0-63, 0xFF = not present
	uint8_t sds;
	uint8_t i2c;
};

struct machine {
	char machine_name[30];
	uint8_t isRTL8373;
	uint8_t n_sfp;
	uint8_t n_10g;
	uint8_t log_to_phys_port[9];
	uint8_t phys_to_log_port[9]; // Starts at 0 for port 1
	uint8_t is_sfp[9];  // 0 for non-SFP ports 1 or 2 for the I2C port number
	// sfp_port[0] is the first SFP-port from the left on the device, sfp_port[1] the next if present 
	struct sfp_port sfp_port[2];
	uint8_t reset_pin;
	struct high_leds high_leds;
	// Defines which led-set (0-3) will be used for given logical port
	// led-set is physical group of LEDs that can be configured to show different port status combinations (see port_led_set below)
	uint8_t port_led_set[9];
	// Defines led-set configuration, applied to all ports using particular led-set
	// Each led-set can have 4 different hardware LED configurations. Which one should be used, depends how LED is wired on the board
	// See stock RTL837X_REG_LED3_2_SETx and RTL837X_REG_LED1_0_SETx registers for reference configuration
	uint32_t led_sets[4][4];
	uint8_t led_mux_custom;
	uint8_t led_mux[28];
	uint32_t mac_flash_offset;
	// Read the flash with single-IO (CMD_FREAD) instead of DIO for MMIO/code
	// fetch. Set for boards whose flash chip is not usable in DIO mode, which
	// shows up as a board that does not boot at all, garbled console output or
	// random resets. Costs management-plane speed only, the ASIC keeps
	// forwarding in hardware.
	uint8_t flash_sio;
	// Copper PHY MDI pair polarity, built with MDI_CFG() from machine_defs.h.
	// 0 keeps the reset default (0x0c), which every board without mirrored
	// pairs relies on.
	uint8_t mdi_cfg;
};

struct machine_runtime
{
	uint8_t isRTL8373 : 1;
	uint8_t isN : 1;
};

void machine_custom_init(void) __banked;

#endif
