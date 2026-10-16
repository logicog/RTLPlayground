; Assembler file to intercept register writes/reads and PHY writes/reads
; assemble with as31
; Compile with
; as31 -Fbyte -Orwintercept_zoerax_hg0402xg.hex rwintercept_zoerax_hg0402xg.asm
; ./injector zoerax_hg0402xg.bin RTLPlayground/rwintercept_zoerax_hg0402xg.hex zoerax_hg0402xg_patched.bin
; Start at 0x3b00, where there is always some space in BANK 0
	.equ	valptr, 0xc7


	.equ	SFR_DATA_0, 0xa7
	.equ	SFR_DATA_8, 0xa6
	.equ	SFR_DATA_16, 0xa5
	.equ	SFR_DATA_24, 0xa4

; Intercept Register Write
	.org 0x4d62
	lcall	reg_write
	nop

; Intercept Register Read
	.org 0x4dad
	lcall	reg_read_head
	.org 0x4e6f
	lcall	reg_read_tail

; Intercept PHY write
	.org 0x522c
	lcall	phy_write

; Intercept PHY read
	.org 0x526f
	lcall	phy_read_head
	.org 0x5295
	ljmp	phy_read_tail

; Intercept SDS write
	.org 0x503c
	lcall	sds_write
	nop

; Intercept SDS read
	.org 0x5061
	lcall	sds_read_head
	nop
	.org 0x507b
	ljmp	sds_read_tail

;
; BEGIN actual code at unused end of BANK0
;
	.org 0x3b00

;
; print the address of a register write. The address is in R6/R7. A and r4 can be scratched
;
reg_write:	
	mov	a, #'R'
	acall	putc

	mov	a, r6
	acall	wbyte

	mov	a, r7
	acall	wbyte

	mov	a, #'-'
	acall	putc

	mov	dptr, #valptr
	movx	a, @dptr
	acall	wbyte
	mov	dptr, #valptr + 1
	movx	a, @dptr
	acall	wbyte
	mov	dptr, #valptr + 2
	movx	a, @dptr
	acall	wbyte
	mov	dptr, #valptr + 3
	movx	a, @dptr
	acall	wbyte

	mov	a, #' '
	acall	putc

	mov	a, r7
	mov	r5, a
	mov	a, r6
	mov	r4, a
	ret

;
; print the address of a register read The address is in R2/R3. Registers 4-7 are scratch
;
reg_read_head:
	mov	a, #'r'
	acall	putc

	mov	a, r2
	acall	wbyte

	mov	a, r3
	acall	wbyte

	mov	a, #':'
	acall	putc
	clr	a
	mov	r7, a
	mov	r6, a
	mov	r5, a
	mov	r4, a
	ret

reg_read_tail:
	mov	dptr, #0xc5
	movx	a, @dptr
	acall	wbyte
	mov	dptr, #0xc5 + 1
	movx	a, @dptr
	acall	wbyte
	mov	dptr, #0xc5 + 2
	movx	a, @dptr
	acall	wbyte
	mov	dptr, #0xc5 + 3
	movx	a, @dptr
	acall	wbyte

	mov	a, #' '
	acall	putc

	mov	dptr,#0xc5
	ret


;
; print phy write options
;
phy_write:
	mov	a, #'P'
	acall	putc

	mov	a, r4
	acall	wbyte

; phymask
	mov	a, r6
	acall	wbyte
	mov	a, r7
	acall	wbyte

	mov	a, #'.'
	acall	putc

; dev_id?
	mov	a, r5
	acall	wbyte

; x_phywrite_opt
	mov	dptr, #0xf1
	movx	a, @dptr
	acall	wbyte

; The register going to SFR_C2 and SFR_C3
	mov	a, r2
	acall	wbyte
	mov	a, r3
	acall	wbyte

	mov	a, #':'
	acall	putc

	mov	dptr, #0xf2
	movx	a, @dptr
	acall	wbyte

	
	mov	dptr, #0xf3
	movx	a, @dptr
	acall	wbyte

	mov	a, #' '
	acall	putc

	mov	dptr, #0xeb
	mov	r4, #0
	ret

;
; print phy read options
;
phy_read_head:
	mov	r1, 4	; actually r4

	mov	a, #'p'
	acall	putc

	mov	a, r7
	acall	wbyte

	mov	a, r5
	acall	wbyte

	mov	a, #'.'
	acall	putc

	mov	a, r2
	acall	wbyte
	mov	a, r3
	acall	wbyte

	mov	a, #':'
	acall	putc

	mov	r1, 7
	mov	a, r2
	ret

phy_read_tail:
	mov	r6, 4
	mov	a, r6
	acall	wbyte
	mov	a, r7
	acall	wbyte

	mov	a, #' '
	acall	putc
	mov	r4, 6
	ret

	
;
; print SDS write options
;
sds_write:
	mov	a, r4
	mov	r2, a	; R2 is scratch

	mov	a, #'Q'
	acall	putc


	mov	a, r7
	acall	wbyte
	mov	a, r5
	acall	wbyte
	mov	a, r3
	acall	wbyte
	
	mov	a, #':'
	acall	putc

	mov	dptr, #0xe1
	movx	a, @dptr
	acall	wbyte

	
	mov	dptr, #0xe2
	movx	a, @dptr
	acall	wbyte

	mov	a, #' '
	acall	putc

	mov	a, r7
	mov	r1, a
	mov	a, r5
	mov	r7, a
	mov	a, r2
	mov	r4, a
	ret

;
; print SDS read options
;
sds_read_head:
	mov	a, r4
	mov	r1, a	; R1 is scratch

	mov	a, #'q'
	acall	putc

	mov	a, r7	; sds_index bit
	acall	wbyte
	mov	a, r5	; SFR_94 (2 x r5 | r7)
	acall	wbyte
	mov	a, r3	; SFR_93
	acall	wbyte

	mov	a, #':'
	acall	putc

	mov	a, r1
	mov	r4, a	; R1 is scratch

	mov	a, r7
	mov	r1, a
	mov	a, r5 
	mov	r7, a 

	ret

;
; print SDS read result
;
sds_read_tail:
	mov	a, r4
	mov	r6, a	; R6 is returned and preserved

	mov	a, r7	; = r7 = SFR_DATA_8
	acall	wbyte
	mov	r7, SFR_DATA_0
	mov	a, r7	; SFR_DATA_0
	acall	wbyte

	mov	a, #' '
	acall	putc

	mov	a, r6	; R6 is returned and preserved
	mov	r4, a	
	ret

;
;	Write a byte as hex to serial
;
	nop
	nop
wbyte:
	mov	r4, a
	swap	a
	anl	a, #0x0f
	mov	dptr,#hex
	movc	a, @a+dptr

	jb	ET0, wbyte1
	ret
wbyte1:
	jbc	TI, wbyte2
	sjmp	wbyte1
wbyte2:
	mov	SBUF,a
	
	mov	a, r4
	anl	a, #0x0f
	mov	dptr,#hex
	movc	a, @a+dptr

wbyte3:
	jbc	TI, wbyte4
	sjmp	wbyte3
wbyte4:
	mov	SBUF,a

	ret

;
;	Intercept isr_ext0
;
isr_ext0:
	mov	a, #'i'
	acall	putc
	mov	r0,#0x51
	inc	@r0
	ret

;
;	Intercept isr_ext1
;
isr_ext1:
	mov	a, #'I'
	acall	putc
	mov	r0,#0x25
	mov	a, @r0
	ret
;
;	Intercept isr_ext2
;
isr_ext2:
	mov	a, #'J'
	acall	putc
	mov	r0, #0x1e
	mov	a, #0xa
	ret

;
;	Intercept isr_ext3
;
isr_ext3:
	mov	a, #'j'
	acall	putc
	mov	r0,#0x1e
	mov	a,#0x14
	ret

;
;	Write a character to serial
;
	nop
	nop
putc:
	jb	ET0, putc1
	ret
putc1:
	jbc	TI,putc2
	sjmp	putc1
putc2:
	mov	SBUF,a
	ret

hex:
	.byte "0123456789abcdef"
	.end
