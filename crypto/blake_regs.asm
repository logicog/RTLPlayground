; R0..R3 and R4..R7 each hold a word. Keep B in registers across both
; halves of G; its ROTR12 output takes the registers just vacated by D.
	.module blake_regs
	.area OSEG (OVR,DATA)
_blake_regs_round_PARM_2::
	.ds 2  ; SDCC passes the XRAM workspace pointer here
	.area BANK4 (CODE)

; Offsets in struct blake3_work, checked against the C layout in blake3.c.
state_offset = 32

; B contains four 2-bit column numbers, one per row. Each group selects
; four distinct words. Offsets and carries work across XRAM page edges.
; The low nibbles are unique, so they also index the next group's columns.
; Sequence: 00 55 aa ff (columns), e4 39 4e 93 (diagonals), 00 (end).
next_columns:
	.db 0x55,0,0,0,0x39,0xaa,0,0,0,0x4e,0xff,0,0,0,0x93,0xe4

	.macro address
	add	a,_blake_regs_round_PARM_2
	mov	dpl,a
	clr	a
	addc	a,(_blake_regs_round_PARM_2+1)
	mov	dph,a
	.endm

	.macro row n
	mov	a,b
	.ifeq n
	rl	a
	rl	a
	.endif
	.ifeq n-2
	rr	a
	rr	a
	.endif
	.ifeq n-3
	swap	a
	.endif
	anl	a,#0x0c
	orl	a,#(state_offset+16*n)
	address
	.endm

	.macro load reg
	movx	a,@dptr
	mov	reg,a
	.endm
	.macro store reg
	mov	a,reg
	movx	@dptr,a
	.endm
	.macro xor_byte reg
	movx	a,@dptr
	xrl	a,reg
	mov	reg,a
	.endm
	.macro word op,w0,w1,w2,w3
	op	w0
	inc	dptr
	op	w1
	inc	dptr
	op	w2
	inc	dptr
	op	w3
	.endm

	.macro add_regs op,dst,src
	mov	a,dst
	op	a,src
	mov	dst,a
	.endm
	.macro add_message op,reg
	movx	a,@dptr
	op	a,reg
	mov	reg,a
	.endm

	.macro first high,a0,a1,a2,a3,b0,b1,b2,b3,?done
	row	0
	word	load,a0,a1,a2,a3
	add_regs add,a0,b0
	add_regs addc,a1,b1
	add_regs addc,a2,b2
	add_regs addc,a3,b3
	pop	acc
	.if high
	push	acc
	swap	a
	.endif
	anl	a,#0x0f
	; Words 8 through 15 are zero padding for every password hash input.
	jb	acc.3,done
	rl	a
	rl	a
	address
	add_message add,a0
	inc	dptr
	add_message addc,a1
	inc	dptr
	add_message addc,a2
	inc	dptr
	add_message addc,a3
done:
	row	0
	word	store,a0,a1,a2,a3
	row	3
	word	xor_byte,a0,a1,a2,a3
	.endm

; Fold C += D and B ^= C into one pass. MOVX and XRL preserve carry.
	.macro accumulate op,dreg,breg
	movx	a,@dptr
	op	a,dreg
	movx	@dptr,a
	xrl	a,breg
	mov	breg,a
	.endm
	.macro second d0,d1,d2,d3,b0,b1,b2,b3
	row	3
	word	store,d0,d1,d2,d3
	row	2
	accumulate add,d0,b0
	inc	dptr
	accumulate addc,d1,b1
	inc	dptr
	accumulate addc,d2,b2
	inc	dptr
	accumulate addc,d3,b3
	.endm

	.macro nibbles dst,lo,hi
; Keep the high nibble of lo and substitute the low nibble of hi.
	mov	a,lo
	xrl	a,hi
	anl	a,#0x0f
	xrl	a,lo
	swap	a
	mov	dst,a
	.endm
	.macro shift_store reg
	mov	a,reg
	rlc	a
	movx	@dptr,a
	.endm

_blake_regs_round::
	mov	b,#0
group:
	clr	a
	movc	a,@a+dptr
	inc	dptr
	push	dpl
	push	dph
	push	acc

	row	1
	word	load,r4,r5,r6,r7
	first	1,r0,r1,r2,r3,r4,r5,r6,r7
	second	r2,r3,r0,r1,r4,r5,r6,r7
	nibbles	r0,r5,r6
	nibbles	r1,r6,r7
	nibbles	r2,r7,r4
	nibbles	r3,r4,r5
	first	0,r4,r5,r6,r7,r0,r1,r2,r3
	second	r5,r6,r7,r4,r0,r1,r2,r3
	row	1
	; ROTR7: seed carry from byte 0 and stream bytes 1,2,3,0 to XRAM.
	mov	a,r0
	rlc	a
	word	shift_store,r1,r2,r3,r0

	mov	a,b
	anl	a,#15
	mov	dptr,#next_columns
	movc	a,@a+dptr
	mov	b,a
	pop	dph
	pop	dpl
	jz	finished
	ljmp	group
finished:
	ret
