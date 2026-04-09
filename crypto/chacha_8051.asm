
;#define CHACHA_QR(A, B, C, D) { \
;    A += B; D ^= A; D = ROTL32(D, 16);  \
;    C += D; B ^= C; B = ROTL32(B, 12);  \
;    A += B; D ^= A; D = ROTL32(D, 8);   \
;    C += D; B ^= C; B = ROTL32(B, 7);   \
;}

; 		r0      r1    r2   r3
; CHACHA_QR( v[ 0], v[ 4], v[ 8], v[12] );

store_32:
	mov	a, r7
	movx	@dptr, a
	dec	dpl
	mov	a, r6
	movx	@dptr, a
	dec	dpl
	mov	a, r5
	movx	@dptr, a
	dec	dpl
	mov	a, r4
	movx	@dptr, a
	ret

rol_b:
	clr	c
	mov	a, r7
	rlc	a
	mov	r7, a
	mov	a, r6
	rlc	a
	mov	r6, a
	mov	a, r5
	rlc	a
	mov	r5, a
	mov	a, r4
	rlc	a
	mov	r4, a
	clr	a
	addc	a, r4
	mov	r4, a
	djnz	dpl, rol_b
	ret

print_regs:
	push	a
	push	r6
	push	r7
	push	dpl
	mov	dpl, a
	lcall	_print_byte

	pop	dpl
	pop	r7
	pop	r6
	pop	a


chacha_plus_xor:
; Load A into registers r4-r7, A pointed to by r0
	mov	dptr, #_chacha20
	mov	dpl, r0
	movx	a, @dptr
	mov	r7, a
	dec	dpl
	movx	a, @dptr
	mov	r6, a
	dec	dpl
	movx	a, @dptr
	mov	r5, a
	dec	dpl
	movx	a, @dptr
	mov	r4, a

; A += B, B pointed to by r1
	mov	dpl, r1
	movx	a, @dptr
	add	a, r7
	mov	r7, a

	dec	dpl
	movx	a, @dptr
	addc	a, r6
	mov	r6, a

	dec	dpl
	movx	a, @dptr
	addc	a, r5
	mov	r5, a

	dec	dpl
	movx	a, @dptr
	addc	a, r4
	mov	r4, a

	mov	dpl, r0		; Store A back
	mov	a, r7
	movx	@dptr, a
	dec	dpl
	mov	a, r6
	movx	@dptr, a
	dec	dpl
	mov	a, r5
	movx	@dptr, a
	dec	dpl
	mov	a, r4
	movx	@dptr, a

; D ^= A
	mov	dpl, r3
	movx	a, @dptr
	xrl	a, r7
	mov	r7, a
	dec	dpl

	movx	a, @dptr
	xrl	a, r6
	mov	r6, a
	dec	dpl

	movx	a, @dptr
	xrl	a, r6
	mov	r6, a
	dec	dpl

	movx	a, @dptr
	xrl	a, r6
	mov	r6, a
	dec	dpl

chacha_qr:
; QR Part: A += B; D ^= A; D = ROTL32(D, 16);
	acall	chacha_plus_xor

; shift left 16. D is r4, r5, r6, r7 -> r6, r7, r4, r5
	mov	a, r6
	xch	a, r4
	mov	a, r7
	xch	a, r5
	mov	dpl, r3	; Store D
	acall	store_32

;QR Part: C += D; B ^= C; B = ROTL32(B, 12);
; Swap A <-> C and D <-> B
	mov	a, r0
	xch	a, r2
	mov	r0, a
	mov	a, r1
	xch	a, r3
	mov	r1, a
	acall	chacha_plus_xor

; shift left 12. D is r4, r5, r6, r7 -> r5, r6, r7, r4
	mov	a, r4
	xch	a, r7
	xch	a, r6
	xch	a, r5
	mov	r4, a
	mov	dpl, #4
	acall	rol_b

; Swap A <-> C and D <-> B
	mov	a, r0
	xch	a, r2
	mov	r0, a
	mov	a, r1
	xch	a, r3
	mov	r1, a

	mov	dpl, r1	; Store B
	acall	store_32

; QR Part A += B; D ^= A; D = ROTL32(D, 8);
	acall	chacha_plus_xor
	mov	a, r4
	xch	a, r7
	xch	a, r6
	xch	a, r5
	mov	r4, a
	mov	dpl, r3	; Store D
	acall	store_32

; QR Part
;    C += D; B ^= C; B = ROTL32(B, 7);

; Swap A <-> C and D <-> B
	mov	a, r0
	xch	a, r2
	mov	r0, a
	mov	a, r1
	xch	a, r3
	mov	r1, a
	acall	chacha_plus_xor

; Roll left 7 bits, start by rolling 8 bits left, then roll 1 to the right
	mov	a, r4
	xch	a, r7
	xch	a, r6
	xch	a, r5
	mov	r4, a

	clr	c
	mov	a, r4
	rrc	a
	mov	r4, a
	mov	a, r5
	rrc	a
	mov	r5, a
	mov	a, r6
	rrc	a
	mov	r6, a
	mov	a, r7
	rrc	a
	mov	r7, a
	clr	a
	rrc	a
	add	a, r4
	mov	r4, a

; Swap A <-> C and D <-> B
	mov	a, r0
	xch	a, r2
	mov	r0, a
	mov	a, r1
	xch	a, r3
	mov	r1, a
	mov	dpl, r1	; Store B
	acall	store_32
