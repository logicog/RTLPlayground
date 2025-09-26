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
	ljmp	__sdcc_gsinit_startup + 0x1000
	ljmp	_isr_ext0	; 0x03
	.ds     5
	ljmp	_isr_timer0	; 0x0b
	.ds     5
	ljmp	_isr_ext1	; 0x13
	.globl __start__stack

	.area GSINIT0 (CODE)

__sdcc_gsinit_startup:
        mov     sp,#__start__stack - 1

	.area GSFINAL (CODE)
        ljmp	_installer
