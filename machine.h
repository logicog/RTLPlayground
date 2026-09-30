#ifndef _MACHINE_H_
#define _MACHINE_H_

#include <stdint.h>
#include <stdbool.h>

/*
 * Select your machine type below
 */
// Legacy KP-9000 4+2 targets. Prefer the PCB-revision-specific targets below.
// #define MACHINE_KP_9000_6XHML_X2
// #define MACHINE_KP_9000_6XH_X2

// KP-9000 4+2 targets by PCB silkscreen revision.
// #define MACHINE_KP_9000_6XH_X2_V1_1
// #define MACHINE_KP_9000_6XHML_X2_V1_1
// #define MACHINE_KP_9000_6XH_X2_V1_2
// #define MACHINE_KP_9000_6XHML_X2_V1_2
// #define MACHINE_KP_9000_6XH_X2_V2_1
// #define MACHINE_KP_9000_6XHML_X2_V2_1

// #define MACHINE_KP_9000_6XH_X
// #define MACHINE_KP_9000_9XH_X_EU
// #define MACHINE_KP_9000_9XHML_X_V2_2
// #define MACHINE_KP_9000_9XHML_X_V3_1
// #define MACHINE_SWGT024_V2_0_MANAGED
// #define MACHINE_SWGT024_V2_0_UNMANAGED
// #define MACHINE_TRENDNET_TEG_S562
// #define MACHINE_HG0402XG_V1_1
// #define MACHINE_SWTG018AS_A_V_2_0
// #define MACHINE_SWTGW218AS
// #define MACHINE_PCB_SWTG018AS_V2_1_0
// #define MACHINE_PCB_K0402WS_V3
// #define MACHINE_PCB_K0402WS_V2
// #define MACHINE_K0501W_V2_0
// #define MACHINE_LIANGUO_ZX_SWTGW215AS
// #define MACHINE_ZX310S_4T2XH
// #define MACHINE_ZX310S_4T2XT
// #define MACHINE_STEAMEMO_IG204_V1
// #define MACHINE_DEFAULT_8C_1SFP
// #define MACHINE_HI_K0801WS
// #define MACHINE_FNS1200P
// #define MACHINE_PCB_SWTG024AS_A_2_0_1
// #define MACHINE_SWTG024AS_A_2_0_1_5C_1SFP
// #define MACHINE_SWTG024AS_V2_0
// #define MACHINE_FG_4GT_2SX_V2_0
// #define MACHINE_FG_8GT_1SX
// #define MACHINE_LIANGUO_HYWS_SGT0108S
// #define MACHINE_POE_2G080110GS
// #define MACHINE_PB_2132
// #define MACHINE_HASIVO_S600W_4GT_2XGT_SE
// #define MACHINE_HASIVO_S1100WP_8GT_1SX_SE
// #define MACHINE_F7008_2_5

// Port/Mac not used/connected,
#define NOP (0x00)
#define IS_PHYS_PORT_INVALID(port) (port == NOP)

// SDSx in on PORT/MACx
#define MAC_SDS0 (3)
#define MAC_SDS1 (8)

enum sds_type {
	SDS_UNUSED = 0, 
	SDS_SFP,
	SDS_EPHY,
	SDS_FIXED_LINK,

	// Used as error code
	SDS_NOT_A_SDS_PORT = -1
};

// External PHY types.
enum phy_type {
	RTL8224,   // Default 4-port 2.5Gbit PHY which normaly used with a RTL8382
	RTL8221B,  // 1-port 2.5Gbit PHY
	RTL8261BE, // 1-port  10gbit PHY
};

struct sfp_port
{
	uint8_t pin_detect; // gpio number 0-63, 0xFF = don't have it?
	uint8_t pin_los; // gpio number 0-63, 0xFF = don't have it?
	uint8_t pin_tx_disable; // gpio number 0-63, 0xFF = not present
	uint8_t i2c;
};

struct ext_phy
{
	enum phy_type type;
	uint8_t phy_addr;
	uint8_t reset_pin;
};

struct sds_settings {
	enum sds_type usage;
	union {
		struct sfp_port sfp;
		struct ext_phy  ephy;
	} sds_settings_t;
};

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

struct machine {
	char machine_name[30];
	uint8_t isRTL8373;
	// Lowest logical port number
	uint8_t min_port;
	// Highest logical port number
	uint8_t max_port;
	uint8_t log_to_phys_port[9];
	// sfp_port[0] is directly linked to MAC 3 / SDS0 (MAC_SDS0)
	// sfp_port[1] is directly linked to MAC 8 / SDS1 (MAC_SDS1)
	// sfp_port struct holds the settings for the devices connected to the SDSx-port.
	// - In case of SFP-CAGE, i2c-setting, gpio to detect the device etc.
	// - In case of external PHY, MDIO-address, max-linkspeed etc.
	// - In case of fixed-link, linkspeed.
	struct sds_settings sds_settings[2];
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
int8_t phys_to_log_port(uint8_t phys_port);
bool is_slot_sfp(uint8_t slot);
int8_t port_to_sds(uint8_t log_port);
enum sds_type port_to_sds_usage(uint8_t log_port);

#endif

