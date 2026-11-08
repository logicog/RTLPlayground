#pragma codeseg ALIAS
#pragma constseg ALIAS

#include <8051.h>
#include <stdint.h>

#include "../rtl837x_sfr.h"
#include "../rtl837x_regs.h"
#include "layout.h"

extern __xdata uint8_t slotbuf[LDR_SLOT_BUF];

void app_start(void) __naked
{
	__asm
	clr	_EA
	mov	_SFR_REG_ADDRH,#0x60
	mov	_SFR_REG_ADDRL,#0x40
	mov	_SFR_EXEC_GO,#0x01
	mov	0x08,#0
	mov	0x09,#0
20101$:
	mov	a,_SFR_EXEC_STATUS
	jz	20102$
	djnz	0x08,20101$
	djnz	0x09,20101$
20102$:
	mov	r2,_SFR_DATA_24
	mov	r3,_SFR_DATA_16
	mov	r4,_SFR_DATA_8
	mov	r5,_SFR_DATA_0
	mov	r6,#3
20110$:
	mov	dptr,#_slotbuf
	mov	r7,#0
20111$:
	mov	_SFR_REG_ADDRH,#0x60
	mov	_SFR_REG_ADDRL,#0x40
	mov	_SFR_DATA_24,r2
	mov	_SFR_DATA_16,r3
	mov	_SFR_DATA_8,r4
	mov	a,r7
	rl	a
	rl	a
	mov	b,a
	mov	a,r5
	anl	a,#0xf1
	orl	a,#0x02
	orl	a,b
	mov	_SFR_DATA_0,a
	mov	_SFR_EXEC_GO,#0x03
	mov	0x08,#0
	mov	0x09,#0
20112$:
	mov	a,_SFR_EXEC_STATUS
	jz	20113$
	djnz	0x08,20112$
	djnz	0x09,20112$
20113$:
	mov	r0,#0xb0
	mov	r1,#0x80
20114$:
	mov	_SFR_REG_ADDRH,r1
	mov	_SFR_REG_ADDRL,r0
	clr	a
	mov	_SFR_DATA_24,a
	mov	_SFR_DATA_16,a
	mov	_SFR_DATA_8,a
	movx	a,@dptr
	inc	dptr
	mov	_SFR_DATA_0,a
	mov	_SFR_EXEC_GO,#0x03
	mov	0x08,#0
	mov	0x09,#0
20115$:
	mov	a,_SFR_EXEC_STATUS
	jz	20116$
	djnz	0x08,20115$
	djnz	0x09,20115$
20116$:
	mov	a,r0
	add	a,#4
	mov	r0,a
	mov	a,r1
	addc	a,#0
	mov	r1,a
	cjne	r0,#0xb0,20114$
	cjne	r1,#0xc0,20114$
	inc	r7
	cjne	r7,#4,20111$
	sjmp	20120$
20117$:
	sjmp	20110$
20120$:
	mov	_SFR_REG_ADDRH,#0x60
	mov	_SFR_REG_ADDRL,#0x40
	mov	_SFR_DATA_24,r2
	mov	_SFR_DATA_16,r3
	mov	_SFR_DATA_8,r4
	mov	_SFR_DATA_0,r5
	mov	_SFR_EXEC_GO,#0x03
	mov	0x08,#0
	mov	0x09,#0
20121$:
	mov	a,_SFR_EXEC_STATUS
	jz	20122$
	djnz	0x08,20121$
	djnz	0x09,20121$
20122$:
	mov	r0,#0
	mov	r1,#0
20123$:
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
	cjne	a,b,20125$
	inc	r0
	cjne	r0,#0,20124$
	inc	r1
20124$:
	cjne	r0,#(LDR_HOME_LEN & 0xff),20123$
	cjne	r1,#(LDR_HOME_LEN >> 8),20123$
	ljmp	0x0000
20125$:
	djnz	r6,20117$
	mov	_SFR_REG_ADDRH,#0x00
	mov	_SFR_REG_ADDRL,#0x24
	clr	a
	mov	_SFR_DATA_24,a
	mov	_SFR_DATA_16,a
	mov	_SFR_DATA_8,a
	mov	_SFR_DATA_0,#0x01
	mov	_SFR_EXEC_GO,#0x03
20126$:
	sjmp	20126$
	__endasm;
}
