	.area HOME    (CODE)
	.area GSINIT0 (CODE)
	.area GSINIT1 (CODE)
	.area GSINIT2 (CODE)
	.area GSINIT3 (CODE)
	.area GSINIT4 (CODE)
	.area GSINIT5 (CODE)
	.area GSINIT  (CODE)
	.area GSFINAL (CODE)
	.area CSEG    (CODE)


	.area HOME    (CODE)

__sdcc_banked_call::
	push	_PSBANK
	xch	a,r0
	push	a
	mov	a,r1
	push	a
	mov	a,r2
	anl	a,#0x1f
	mov	_PSBANK, a
	xch	a, r0
	ret

__sdcc_banked_ret::
	pop	_PSBANK
	ret
