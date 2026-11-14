#pragma codeseg ALIAS
#pragma constseg ALIAS

#include <8051.h>
#include <stdint.h>

#include "../rtl837x_sfr.h"
#include "../rtl837x_regs.h"
#include "layout.h"

// The application starts at its reset vector, as after a chip reset
#define APP_RESET_VECTOR	0x0000

extern __xdata uint8_t slotbuf[LDR_SLOT_BUF];

void app_start(void) __naked
{
	__asm
;
; app_start copies the application home bank from slotbuf (XDATA) into
; code RAM and jumps to the reset vector of the application.
;
; It overwrites the code RAM the rest of the loader runs from, so it runs
; from a flash alias window instead (see alias_find in loader.c). The
; window address depends on the flash size, so only relative jumps are
; used and one absolute jump to APP_RESET_VECTOR at the end. There are no
; calls: a call needs a stack and an absolute address, so the register
; access is written out with macros wherever it is needed. The Makefile
; checks this in the listing.
;
; Registers:
;   r0, r1   address of the code RAM write register (low, high)
;   r2 - r5  saved value of HW_CONF (data bytes 24, 16, 8, 0)
;   r6       copy attempts left
;   r7       code RAM page
;   dptr     source in slotbuf
;
; r0 - r7 are all taken, so the busy wait counter lives in two IRAM bytes.
; The loader is finished at this point and none of its IRAM is needed any
; more, 0x08 and 0x09 are simply the first bytes above register bank 0.
;
	WAIT_LO = 0x08
	WAIT_HI = 0x09
	CODE_RAM_END = RTL837X_REG_CODE_RAM + 4 * CODE_RAM_PAGE_SIZE
	RESET_SOC = 1 << RESET_SOC_BIT

; A register access normally finishes within a few polls, the limit of
; 65536 polls only keeps a stuck register interface from hanging here.
; A failed copy is caught by the compare and tried again, the same 3
; attempts the application makes when it writes a new loader.
	TRIES = 3

; Wait until the register interface is idle, give up after 65536 polls
	.macro	REG_WAIT ?poll, ?done
	mov	WAIT_LO,#0
	mov	WAIT_HI,#0
poll:
	mov	a,_SFR_EXEC_STATUS
	jz	done
	djnz	WAIT_LO,poll
	djnz	WAIT_HI,poll
done:
	.endm

; Select a register by its fixed address
	.macro	REG_ADDR addr
	mov	_SFR_REG_ADDRH,#((addr) >> 8)
	mov	_SFR_REG_ADDRL,#((addr) & 0xff)
	.endm

; Set the 4 data bytes for the next write, highest byte first
	.macro	REG_DATA d24, d16, d8, d0
	mov	_SFR_DATA_24,d24
	mov	_SFR_DATA_16,d16
	mov	_SFR_DATA_8,d8
	mov	_SFR_DATA_0,d0
	.endm

; Run a read or write command on the selected register and wait for it
	.macro	REG_GO cmd
	mov	_SFR_EXEC_GO,#cmd
	REG_WAIT
	.endm

; The interrupt vectors and handlers are in the code RAM that is about to
; be overwritten, so no interrupt may run from here on.
	clr	_EA

; Read HW_CONF and keep it in r2 - r5
	REG_ADDR RTL837X_REG_HW_CONF
	REG_GO	SFR_EXEC_READ_REG
	mov	r2,_SFR_DATA_24
	mov	r3,_SFR_DATA_16
	mov	r4,_SFR_DATA_8
	mov	r5,_SFR_DATA_0
	mov	r6,#TRIES

; Copy all code RAM pages from slotbuf
copy_attempt:
	mov	dptr,#_slotbuf
	mov	r7,#0

; Open code RAM for writes and select page r7
copy_page:
	mov	a,r7
	.rept	HW_CONF_CODE_RAM_PAGE_SHIFT
	rl	a
	.endm
	mov	b,a
	mov	a,r5
	anl	a,#(~(HW_CONF_CODE_RAM_WRITE | HW_CONF_CODE_RAM_PAGE_MASK) & 0xff)
	orl	a,#HW_CONF_CODE_RAM_WRITE
	orl	a,b
	REG_ADDR RTL837X_REG_HW_CONF
	REG_DATA r2, r3, r4, a
	REG_GO	SFR_EXEC_WRITE_REG

; Write the page, one byte per register, 4 register addresses apart
	mov	r0,#(RTL837X_REG_CODE_RAM & 0xff)
	mov	r1,#(RTL837X_REG_CODE_RAM >> 8)
copy_byte:
	movx	a,@dptr
	inc	dptr
	mov	_SFR_REG_ADDRH,r1
	mov	_SFR_REG_ADDRL,r0
	REG_DATA #0, #0, #0, a
	REG_GO	SFR_EXEC_WRITE_REG
	mov	a,r0
	add	a,#4
	mov	r0,a
	mov	a,r1
	addc	a,#0
	mov	r1,a
	cjne	r0,#(CODE_RAM_END & 0xff),copy_byte
	cjne	r1,#(CODE_RAM_END >> 8),copy_byte
	inc	r7
	cjne	r7,#CODE_RAM_PAGES,copy_page
	sjmp	restore_hw_conf

; copy_attempt is out of reach of the djnz below, so it jumps here first
retry_copy:
	sjmp	copy_attempt

; Close code RAM again by writing back the saved HW_CONF value
restore_hw_conf:
	REG_ADDR RTL837X_REG_HW_CONF
	REG_DATA r2, r3, r4, r5
	REG_GO	SFR_EXEC_WRITE_REG

; Compare code RAM with slotbuf and start the application on a match.
; The 4 pages cover 2 bytes more than the home bank, those 2 bytes are not
; part of the image and are not compared.
	mov	r0,#0
	mov	r1,#0
verify_byte:
	mov	a,r0
	add	a,#_slotbuf
	mov	dpl,a
	mov	a,r1
	addc	a,#(_slotbuf >> 8)
	mov	dph,a
	movx	a,@dptr
	mov	b,a
	mov	dpl,r0
	mov	dph,r1
	clr	a
	movc	a,@a+dptr
	cjne	a,b,verify_failed
	inc	r0
	cjne	r0,#0,verify_next
	inc	r1
verify_next:
	cjne	r0,#(LDR_HOME_LEN & 0xff),verify_byte
	cjne	r1,#(LDR_HOME_LEN >> 8),verify_byte
	ljmp	APP_RESET_VECTOR

; On a mismatch copy again, after the last attempt reset the chip. The
; reset takes effect while the write runs, so there is nothing to wait
; for, the loop below only holds the CPU until then.
verify_failed:
	djnz	r6,retry_copy
	REG_ADDR RTL837X_REG_RESET
	REG_DATA #0, #0, #0, #RESET_SOC
	mov	_SFR_EXEC_GO,#SFR_EXEC_WRITE_REG
wait_for_reset:
	sjmp	wait_for_reset
	__endasm;
}
