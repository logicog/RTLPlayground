; ChaCha Quater-Round implementation in Assembler
	.module chacha_8051
; Global variables:
	.globl _chacha_20
	.globl _chacha_test_1
	.globl _chacha_qr_r
	.globl _chacha_update
	.globl _chacha_count

;#define CHACHA_QR(A, B, C, D) { \
;    A += B; D ^= A; D = ROTL32(D, 16);  \
;    C += D; B ^= C; B = ROTL32(B, 12);  \
;    A += B; D ^= A; D = ROTL32(D, 8);   \
;    C += D; B ^= C; B = ROTL32(B, 7);   \
;}

; 		r0      r1    r2   r3
; CHACHA_QR( v[ 0], v[ 4], v[ 8], v[12] );

	.area HOME    (CODE)

store_32:
	ar7 = 0x07
	ar6 = 0x06
	ar5 = 0x05
	ar4 = 0x04
	ar3 = 0x03
	ar2 = 0x02
	ar1 = 0x01
	ar0 = 0x00

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
	addc	a, r7
	mov	r7, a
	djnz	dpl, rol_b
	ret

print_regs:
	push	a
	push	ar6
	push	ar7
	push	dpl
	mov	dpl, a
	lcall	_print_byte

	pop	dpl
	pop	ar7
	pop	ar6
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
	xrl	a, r5
	mov	r5, a
	dec	dpl

	movx	a, @dptr
	xrl	a, r4
	mov	r4, a
	dec	dpl
	ret

chacha_qr:
; QR Part: A += B; D ^= A; D = ROTL32(D, 16);
	acall	chacha_plus_xor

; rotate left 16. D is r4, r5, r6, r7 -> r6, r7, r4, r5
	mov	a, r6
	xch	a, r4
	mov	r6, a
	mov	a, r7
	xch	a, r5
	mov	r7, a
	mov     dpl, r3 ; Store D
	acall   store_32

;QR Part: C += D; B ^= C; B = ROTL32(B, 12);
; Swap A <-> C and D <-> B
	mov	a, r0
	xch	a, r2
	mov	r0, a
	mov	a, r1
	xch	a, r3
	mov	r1, a
	acall	chacha_plus_xor

; rotate left 12. D is r4, r5, r6, r7 -> r5, r6, r7, r4
	mov	a, r4
	xch	a, r7
	xch	a, r6
	xch	a, r5
	mov	r4, a

	mov	dpl, #4
	acall	rol_b
	mov	dpl, r3	; Store D (being B)
	acall	store_32

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
line4:
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
	ret

_chacha_test_1:
	mov	r0, #3
	mov	r1, #7
	mov	r2, #11
	mov	r3, #15
	acall	line4
	ret

; QUARTERROUND(2,7,8,13)
_chacha_qr_r:
	mov	r0, #11
	mov	r1, #31
	mov	r2, #35
	mov	r3, #55
	acall	chacha_qr
	ret

;
; Implementation of 20 ChaCha Rounds (10 Double Rounds)
;
_chacha_20:
	push    acc
	push    b
	push    dpl
	push    dph
	push    ar7
	push    ar6
	push    ar5
	push    ar4
	push    ar3
	push    ar2
	push    ar1
	push    ar0
	push    psw

	mov	b, #10 ; 10 Double rounds
chacha_20_loop:
;	CHACHA_QR( v[ 0], v[ 4], v[ 8], v[12] );
	mov r0, #3
	mov r1, #19
	mov r2, #35
	mov r3, #51
	acall	chacha_qr
	
;        CHACHA_QR( v[ 1], v[ 5], v[ 9], v[13] ); 7  23  39  55
	mov r0, #7
	mov r1, #23
	mov r2, #39
	mov r3, #55
	acall	chacha_qr

; 	CHACHA_QR( v[ 2], v[ 6], v[10], v[14] ); 11 27  43  59
	mov r0, #11
	mov r1, #27
	mov r2, #43
	mov r3, #59
	acall	chacha_qr

;	CHACHA_QR( v[ 3], v[ 7], v[11], v[15] ); 15 31  47  63
        mov r0, #15
        mov r1, #31
        mov r2, #47
        mov r3, #63
	acall	chacha_qr

;	CHACHA_QR( v[ 0], v[ 5], v[10], v[15] ); 3  23  43  63
	mov r0, #3
	mov r1, #23
	mov r2, #43
	mov r3, #63
	acall	chacha_qr

;	CHACHA_QR( v[ 1], v[ 6], v[11], v[12] ); 7  27  47  51
	mov r0, #7
	mov r1, #27
	mov r2, #47
	mov r3, #51
	acall	chacha_qr

;	CHACHA_QR( v[ 2], v[ 7], v[ 8], v[13] ); 11 31  35  55
	mov r0, #11
	mov r1, #31
	mov r2, #35
	mov r3, #55
	acall	chacha_qr

;	CHACHA_QR( v[ 3], v[ 4], v[ 9], v[14] ); 15 19  39  59
	mov r0, #15
	mov r1, #19
	mov r2, #39
	mov r3, #59
	acall	chacha_qr
	djnz	b, chacha_20_loop

	pop     psw
	pop     ar0
	pop     ar1
	pop     ar2
	pop     ar3
	pop     ar4
	pop     ar5
	pop     ar6
	pop     ar7
	pop     dph
	pop     dpl
	pop     b
	pop     acc
	ret

;
; Update ChaCHa state
;
_chacha_update:
	push    acc
	push    b
	push    dpl
	push    dph
	push    ar4
	push    ar3
	push    ar2
	push    ar1
	push    ar0
	push    psw

	mov	dptr, #_chacha20
	mov	b, #16 ; 16 uint32
update_loop:
	mov	a, b ; multiply by 4 and subtract 1
	rl	a
	rl	a
	dec	a
; Load Working State variable into registers r0-r3
	mov	dpl, a

	orl	a, #64	; Save pointer to state
	mov	r4, a

	movx	a, @dptr
	mov	r3, a
	dec	dpl
	movx	a, @dptr
	mov	r2, a
	dec	dpl
	movx	a, @dptr
	mov	r1, a
	dec	dpl
	movx	a, @dptr
	mov	r0, a

; Add to State variable
	mov	a, r4
	mov	dpl, a
	movx	a, @dptr
	add	a, r3
	mov	r3, a

	dec	dpl
	movx	a, @dptr
	addc	a, r2
	mov	r2, a

	dec	dpl
	movx	a, @dptr
	addc	a, r1
	mov	r1, a

	dec	dpl
	movx	a, @dptr
	addc	a, r0
	mov	r0, a

; Store back state variable in LSB, first sequence (serialized)
	mov	a, r4
	xrl	a, #64	; Save pointer to working state
	mov	dpl, a
	mov	a, r0
	movx	@dptr, a
	dec	dpl
	mov	a, r1
	movx	@dptr, a
	dec	dpl
	mov	a, r2
	movx	@dptr, a
	dec	dpl
	mov	a, r3
	movx	@dptr, a

	djnz	b, update_loop

	pop     psw
	pop     ar0
	pop     ar1
	pop     ar2
	pop     ar3
	pop     ar4
	pop     dph
	pop     dpl
	pop     b
	pop     acc
	ret

;
; Increase counter in state
;
_chacha_count:
	push    acc
	push    dpl
	push    dph
	mov	dptr, #_chacha20
	mov	dpl, #112 + 3
	movx	a, @dptr
	inc	a
	movx	@dptr, a	
	jnc	chacha_count_done
	dec	dpl
	movx	a, @dptr
	inc	a
	movx	@dptr, a
	jnc	chacha_count_done
	dec	dpl
	movx	a, @dptr
	inc	a
	movx	@dptr, a
	jnc	chacha_count_done
	dec	dpl
	movx	a, @dptr
	inc	a
	movx	@dptr, a
chacha_count_done:
	pop     dph
	pop     dpl
	pop     acc
	ret
