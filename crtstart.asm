	.globl __start__stack
;--------------------------------------------------------
; Stack segment in internal ram
;--------------------------------------------------------
	.area	SSEG	(DATA)
__start__stack:
	.ds	1

 	.area VECTOR    (CODE)
	.globl __interrupt_vect
__interrupt_vect:
 	ljmp	__sdcc_gsinit_startup
 	ljmp	_isr_ext0	; 0x03
	.ds     5
	ljmp	_isr_timer0	; 0x0b
	.ds     5
	ljmp	_isr_ext1	; 0x13
	.ds     5
	reti
	.ds     7
 	ljmp    _isr_serial	; 0x23
	.ds     5
	reti			; 0x2b  TIMER 2 IRQ
	.ds     7
	reti			; 0x33 NOT used by DW8051
	.ds     7
	reti			; 0x3b Serial port 1 RX/TX IRQ
	.ds     7
 	ljmp    _isr_ext2	; 0x43
	.ds     5
 	ljmp    _isr_ext3	; 0x4b

	.globl __start__stack

	.area GSINIT0 (CODE)

__sdcc_gsinit_startup::
        mov     sp,#__start__stack - 1

	.area GSFINAL (CODE)
        ljmp	_bootloader
