#ifndef _MACHINE_H_
#define _MACHINE_H_

#include <stdint.h>

/*
 * Select your machine type below
 */
// #define MACHINE_KP_9000_6XHML_X2
// #define MACHINE_KP_9000_6XH_X
#define MACHINE_KP_9000_9XH_X_EU
// #define MACHINE_SWGT024_V2_0
// #define MACHINE_HORACO_ZX_SG4T2
// #define MACHINE_HG0402XG_V1_1
// #define DEFAULT_8C_1SFP

// #define DEFAULT_5C_1SFP

typedef struct {
	// GPIO pins for SDA/SCL
	uint8_t sda; 
	uint8_t scl;
} i2c_bus_t;


#define LED_27 1
#define LED_28 2
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

typedef struct machine {
	char machine_name[30];
	uint8_t isRTL8373;
	uint8_t min_port;
	uint8_t max_port;
	uint8_t n_sfp;
	uint8_t log_to_phys_port[9];
	uint8_t phys_to_log_port[9]; // Starts at 0 for port 1
	uint8_t is_sfp[9];  // 0 for non-SFP ports 1 or 2 for the I2C port number
	// sfp_port[0] is the first SFP-port from the left on the device, sfp_port[1] the next if present 
	struct sfp_port sfp_port[2];
	int8_t reset_pin;
	struct high_leds high_leds;
	uint8_t port_led_set[9];
	uint32_t led_sets[4][4];
};

typedef struct machine_runtime
{
	uint8_t isRTL8373 : 1;
	uint8_t isN : 1;
};

#endif
