/* Host model of the RTL8370N SFR boundary; firmware logic remains unchanged. */
#ifndef RTLP_8370N_HW_H
#define RTLP_8370N_HW_H

#include <stdint.h>

static uint8_t mock_status(void);
static void mock_settle(void);
static uint8_t mock_flash_busy(void);
#define __asm__(text) mock_settle()

static uint8_t SFR_EXEC_GO, SFR_REG_ADDRH, SFR_REG_ADDRL;
static uint8_t SFR_DATA_24, SFR_DATA_16, SFR_DATA_8, SFR_DATA_0;
#define SFR_EXEC_STATUS mock_status()
#define SFR_EXEC_READ_REG 1
#define SFR_EXEC_WRITE_REG 3
static uint8_t SFR_NIC_DATA_L, SFR_NIC_DATA_H, SFR_NIC_RING_L, SFR_NIC_RING_H, SFR_NIC_CTRL;
static uint8_t SFR_FLASH_TCONF, SFR_FLASH_CMD_R, SFR_FLASH_CMD, SFR_FLASH_EXEC_GO;
static uint8_t SFR_FLASH_DUMMYCYCLES, SFR_FLASH_MODEB, SFR_FLASH_ADDR16, SFR_FLASH_ADDR8, SFR_FLASH_ADDR0;
static uint8_t SFR_FLASH_DATA0, SFR_FLASH_DATA8, SFR_FLASH_DATA16, SFR_FLASH_DATA24;
static uint8_t IE, CKCON, T2CON, MAP_FLASH_PAGE_TO_XDATA;
#define SFR_FLASH_EXEC_BUSY mock_flash_busy()
static uint8_t SFR_FLASH_CONFIG, SFR_FLASH_CONF_RCMD, SFR_FLASH_CONF_DIV;
static uint16_t RCAP2_U16, T2_U16;

/* This test never runs switch_init; no vendor PHY patch is required. */
#define PHY_PATCH_LEN 1
static const uint16_t phy_patch[PHY_PATCH_LEN][2] = {{0, 0}};
static const uint16_t long_link[8] = {0};

#endif
