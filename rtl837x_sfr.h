/* SFR control registers for switch register access */
__sfr __at(0xa0) SFR_EXEC_GO;
__sfr __at(0xa1) SFR_EXEC_STATUS;
__sfr16 __at(0xa2a3) SFR_REG_ADDR_U16;
__sfr __at(0xa2) SFR_REG_ADDRH;
__sfr __at(0xa3) SFR_REG_ADDRL;
__sfr16 __at(0xa4a5) SFR_DATA_U16;
__sfr32 __at(0xa4a5a6a7) SFR_DATA_U32;
__sfr32 __at(0xa7a6a5a4) SFR_DATA_U32LE;
__sfr __at(0xa4) SFR_DATA_24;
__sfr __at(0xa5) SFR_DATA_16;
__sfr __at(0xa6) SFR_DATA_8;
__sfr __at(0xa7) SFR_DATA_0;

// Command bytes to write to SFR_EXEC_GO
#define SFR_EXEC_READ_REG 1
#define SFR_EXEC_WRITE_REG 3
#define SFR_EXEC_READ_SDS 5
#define SFR_EXEC_WRITE_SDS 7
#define SFR_EXEC_READ_SMI 9
#define SFR_EXEC_WRITE_SMI 11

/* SFR control registers for phy access via SMI/MDIO */
__sfr __at(0xc2) SFR_SMI_REG_H;
__sfr __at(0xc3) SFR_SMI_REG_L;
__sfr __at(0xc4) SFR_SMI_DEV;
__sfr __at(0xc5) SFR_SMI_PHYMASK;

#define SFR_SMI_PHY SFR_DATA_16

/* Extended IRQ EIE register for external interrupts */
__sfr __at(0xe8) EIE;
__sbit __at(0xe8) EX2;
__sbit __at(0xe9) EX3;

/* EXIF additional external IRQ control register */
__sfr __at(0x91) EXIF;
/* Extended IRQ EIP register for external interrupt priority */
__sfr __at(0xf8) EIP;
__sbit __at(0xf9) PX3;

/* SFR control registers for serial communication */
__sfr __at(0xc8) T2CON;
__sfr __at(0xca) RCAP2L;
__sfr __at(0xcb) RCAP2H;

/* SFR Bank control register: 0x0-3f. A value of 0 is bank 1 */
__sfr __at(0x96) PSBANK;
// SFR used to store return bank for trampoline
__sfr __at(0xbb) SFR_BANK_RET;

__sfr __at(0x8e) CKCON;

__sfr __at(0x97) SFR_97;	// HADDR?
__sfr __at(0xb9) SFR_b9;
__sfr __at(0xba) SFR_ba;

// Clause 22 PHY access ???
__sfr __at(0x93) SFR_93;
__sfr __at(0x94) SFR_94;

/* FLASH controller SFRs */
__sfr __at(0x80) SFR_FLASH_EXEC;
__sbit __at(0x82) SFR_FLASH_EXEC_GO;
__sbit __at(0x80) SFR_FLASH_EXEC_BUSY;

__sfr __at(0xb1) SFR_FLASH_CMD_R;
__sfr __at(0xb2) SFR_FLASH_CMD;
__sfr __at(0xbc) SFR_FLASH_CONFIG;

__sfr __at(0x9b) SFR_FLASH_CONF_DIV;
__sfr __at(0x9c) SFR_FLASH_CONF_RCMD;
__sfr __at(0x9d) SFR_FLASH_DUMMYCICLES;
__sfr __at(0x9a) SFR_FLASH_MODEB;

__sfr __at(0x9e) SFR_FLASH_TCONF;

__sfr __at(0xaf) SFR_FLASH_DATA24;
__sfr __at(0xae) SFR_FLASH_DATA16;
__sfr __at(0xad) SFR_FLASH_DATA8;
__sfr __at(0xac) SFR_FLASH_DATA0;


__sfr __at(0xab) SFR_FLASH_ADDR16;
__sfr __at(0xaa) SFR_FLASH_ADDR8;
__sfr __at(0xa9) SFR_FLASH_ADDR0;

/*
 * NIC Interface
 * CAREFUL: This is now Little Endian
 */
__sfr __at(0xb7) SFR_NIC_CTRL;
__sfr16 __at(0xb4b3) SFR_NIC_DATA_U16LE;
__sfr __at(0xb3) SFR_NIC_DATA_L;
__sfr __at(0xb4) SFR_NIC_DATA_H;
__sfr16 __at(0xb6b5) SFR_NIC_RING_U16LE;
__sfr __at(0xb5) SFR_NIC_RING_L;
__sfr __at(0xb6) SFR_NIC_RING_H;
