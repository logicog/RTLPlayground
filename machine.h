#ifndef _MACHINE_H_
#define _MACHINE_H_

#include <stdint.h>

/*
 * Select your machine type below
 */
// #define MACHINE_KP_9000_6XHML_X2
// #define MACHINE_KP_9000_6XH_X
// #define MACHINE_KP_9000_6XH_X2
// #define MACHINE_KP_9000_9XH_X_EU
// #define MACHINE_KP_9000_9XHML_X_V2_2
// #define MACHINE_KP_9000_9XHML_X_V3_1
// #define MACHINE_KP_9000_9XHPML_X_V3_1   // same board as V3_1 + PoE (RTL8238B PSE)
// #define MACHINE_SWGT024_V2_0_MANAGED
// #define MACHINE_SWGT024_V2_0_UNMANAGED
// #define MACHINE_TRENDNET_TEG_S562
// #define MACHINE_HG0402XG_V1_1
// #define MACHINE_SWTG018AS_A_V_2_0
// #define MACHINE_SWTGW218AS
// #define MACHINE_PCB_K0402WS_V3
// #define MACHINE_K0501W_V2_0
// #define MACHINE_LIANGUO_ZX_SWTGW215AS
// #define MACHINE_ZX310S_4T2XH
// #define MACHINE_ZX310S_4T2XT
// #define MACHINE_DEFAULT_8C_1SFP
// #define MACHINE_HI_K0801WS
// #define MACHINE_FNS1200P
// #define MACHINE_PCB_SWTG024AS_A_2_0_1

/*
 * PoE capability per machine. A board with a software-controllable PSE defines its chip-driver
 * macro POE_CHIP_<chip>, which selects which driver source compiles (e.g. poe_rtl8238b.c) and
 * implies POE_PRESENT. POE_PRESENT is the generic gate: it #ifdef-includes the `poe` console
 * command, the /poe.json endpoint, the boot bring-up and the PSE-image embedding (Makefile).
 * The chip-agnostic interface those consumers call lives in poe.h; the per-board parameters
 * (chip, I2C addresses, port count) live in the machine descriptor's `poe` field below.
 * Boards without a POE_CHIP_* macro compile no PoE code at all.
 */
/* The KP-9000-9XHPML-X V3.1 is the KP-9000-9XHML-X V3.1 board plus an RTL8238B PSE: selecting it
 * reuses the base board's machine descriptor (in machine.c) and just adds the PoE driver, so
 * there is only one config to maintain - no duplicated machine block. */
#if defined(MACHINE_KP_9000_9XHPML_X_V3_1)
#  define MACHINE_KP_9000_9XHML_X_V3_1	/* reuse the base board config */
#  define POE_CHIP_RTL8238B		/* + the RTL8238B PSE driver */
#endif

/* Any chip-driver macro implies the generic PoE layer (interface + consumers) is present. */
#if defined(POE_CHIP_RTL8238B)
#  define POE_PRESENT
#endif

typedef struct {
	// GPIO pins for SDA/SCL
	uint8_t sda;
	uint8_t scl;
} i2c_bus_t;


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
	i2c_bus_t i2c;
};

// PoE PSE chip type for struct machine.poe (POE_NONE = no software-controlled PoE)
enum { POE_NONE = 0, POE_RTL8238B = 1 };

struct poe_config {
	uint8_t chip;        // POE_NONE / POE_RTL8238B
	uint8_t addr0;       // first PSE controller I2C address (e.g. 0x20)
	uint8_t addr1;       // second controller address (0x21), 0 = single controller
	uint8_t n_ports;     // total number of PoE ports
};

typedef struct machine {
	char machine_name[32];	// max 31 chars + NUL; e.g. "keepLink KP-9000-9XHPML-X V3.1" is 30
	uint8_t isRTL8373;
	// Lowest logical port number
	uint8_t min_port;
	// Highest logical port number
	uint8_t max_port;
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
	// PoE PSE descriptor; chip = POE_NONE on boards without software-controlled PoE
	struct poe_config poe;
};

typedef struct machine_runtime
{
	uint8_t isRTL8373 : 1;
	uint8_t isN : 1;
};

void machine_custom_init(void);

#endif
