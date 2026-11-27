#ifndef LOADER_LAYOUT_H
#define LOADER_LAYOUT_H

// Flash layout of a switch with the loader, see doc/loader.md.
// tools/bootpack.py and the Makefile read their addresses from this file.

// Loader data in the upper part of bank 5: boot counter, metadata and the
// copy of the application home bank
#define LDR_DATA		0x3a000UL
#define LDR_DATA_END		0x40000UL

#define LDR_BOOTCNT		0x3a000UL
#define LDR_META		0x3b000UL
#define LDR_SLOT		0x3c000UL
#define LDR_HOME_LEN		0x3ffe

// The loader occupies the first LDR_LOADER_LEN bytes of flash, the chip
// copies them to code RAM without the 2 byte header, so flash address A
// shows up at code RAM address A - 2. The last 2 bytes hold the CRC.
#define LDR_LOADER_LEN		0x4000

// app_start lives at LDR_ALIAS_FLASH in flash, up to the end of the loader.
// It is linked to run at LDR_ALIAS_CODE, one of the addresses where the bank
// window can show it, and only uses relative jumps so it runs at the others.
#define LDR_ALIAS_FLASH		0x3e00
#define LDR_ALIAS_LEN		(LDR_LOADER_LEN - LDR_ALIAS_FLASH)
#define LDR_ALIAS_RAM		(LDR_ALIAS_FLASH - 2)
#define LDR_ALIAS_CODE		(0x8000 + LDR_ALIAS_FLASH)
#define LDR_SLOT_BUF		0x4000

// A flash above 2 MiB has no bank window on LDR_ALIAS_FLASH, so the loader
// keeps a copy of app_start here, bank 0x0a shows it at LDR_ALIAS_CODE
#define LDR_ALIAS_COPY		0x77e00UL

// Application banks below and above the loader data
#define LDR_APP_A		0x04000UL
#define LDR_APP_A_END		0x3a000UL
#define LDR_APP_B		0x40000UL
#define LDR_APP_B_END		0x70000UL

#define LDR_UPLOAD		0x80000UL
#define LDR_STAGE		0x71000UL
#define LDR_IMAGE_LEN		0x80000UL

// Stored in the metadata, a boot image for another layout is refused
#define LDR_LAYOUT		1

// Metadata at LDR_META: "RTLB", the application CRC (low byte first)
// and LDR_LAYOUT
#define LDR_META_CRC		4
#define LDR_META_LAYOUT		6
#define LDR_META_LEN		7

#define LDR_RECOVERY_BOOTS	3
#define LDR_CONFIRM_S		15

#endif
