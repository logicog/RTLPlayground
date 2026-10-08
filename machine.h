#ifndef _MACHINE_H_
#define _MACHINE_H_

#include <stdint.h>

#define MACHINE_STR_HELPER(x)  #x
#define MACHINE_STR(x) MACHINE_STR_HELPER(x)


#define MACHINE_DEF_DIR machine

#ifndef MACHINE_NAME
	#define MACHINE_NAME default_machine
#endif

#define MACHINE_INC(dir,file) MACHINE_STR(dir/file)

#include  MACHINE_INC(MACHINE_DEF_DIR,MACHINE_NAME.h)

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
	uint32_t mac_flash_offset;
};

struct machine_runtime
{
	uint8_t isRTL8373 : 1;
	uint8_t isN : 1;
};

void machine_custom_init(void) __banked;

#endif
