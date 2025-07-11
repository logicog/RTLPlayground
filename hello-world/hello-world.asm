;--------------------------------------------------------
; File Created by SDCC : free open source ANSI-C Compiler
; Version 4.2.0 #13081 (Linux)
;--------------------------------------------------------
	.module hello_world
	.optsdcc -mmcs51 --model-small
	
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _uip_listen
	.globl _psock_readto
	.globl _psock_init
	.globl _strtox
	.globl _write_char
	.globl _hello_world_init
	.globl _hello_world_appcall
;--------------------------------------------------------
; special function registers
;--------------------------------------------------------
	.area RSEG    (ABS,DATA)
	.org 0x0000
;--------------------------------------------------------
; special function bits
;--------------------------------------------------------
	.area RSEG    (ABS,DATA)
	.org 0x0000
;--------------------------------------------------------
; overlayable register banks
;--------------------------------------------------------
	.area REG_BANK_0	(REL,OVR,DATA)
	.ds 8
;--------------------------------------------------------
; internal ram data
;--------------------------------------------------------
	.area DSEG    (DATA)
;--------------------------------------------------------
; overlayable items in internal ram
;--------------------------------------------------------
;--------------------------------------------------------
; indirectly addressable internal ram data
;--------------------------------------------------------
	.area ISEG    (DATA)
;--------------------------------------------------------
; absolute internal ram data
;--------------------------------------------------------
	.area IABS    (ABS,DATA)
	.area IABS    (ABS,DATA)
;--------------------------------------------------------
; bit data
;--------------------------------------------------------
	.area BSEG    (BIT)
;--------------------------------------------------------
; paged external ram data
;--------------------------------------------------------
	.area PSEG    (PAG,XDATA)
;--------------------------------------------------------
; external ram data
;--------------------------------------------------------
	.area XSEG    (XDATA)
_handle_connection_buf_65536_59:
	.ds 100
;--------------------------------------------------------
; absolute external ram data
;--------------------------------------------------------
	.area XABS    (ABS,XDATA)
;--------------------------------------------------------
; external initialized ram data
;--------------------------------------------------------
	.area XISEG   (XDATA)
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
;--------------------------------------------------------
; global & static initialisations
;--------------------------------------------------------
	.area HOME    (CODE)
	.area GSINIT  (CODE)
	.area GSFINAL (CODE)
	.area GSINIT  (CODE)
;--------------------------------------------------------
; Home
;--------------------------------------------------------
	.area HOME    (CODE)
	.area HOME    (CODE)
;--------------------------------------------------------
; code
;--------------------------------------------------------
	.area BANK1   (CODE)
;------------------------------------------------------------
;Allocation info for local variables in function 'hello_world_init'
;------------------------------------------------------------
;	hello-world.c:43: hello_world_init(void)
;	-----------------------------------------
;	 function hello_world_init
;	-----------------------------------------
_hello_world_init:
	ar7 = 0x07
	ar6 = 0x06
	ar5 = 0x05
	ar4 = 0x04
	ar3 = 0x03
	ar2 = 0x02
	ar1 = 0x01
	ar0 = 0x00
;	hello-world.c:46: uip_listen(HTONS(1000));
	mov	dptr,#0xe803
;	hello-world.c:47: }
	ljmp	_uip_listen
;------------------------------------------------------------
;Allocation info for local variables in function 'hello_world_appcall'
;------------------------------------------------------------
;s                         Allocated to registers r6 r7 
;------------------------------------------------------------
;	hello-world.c:57: hello_world_appcall(void)
;	-----------------------------------------
;	 function hello_world_appcall
;	-----------------------------------------
_hello_world_appcall:
;	hello-world.c:64: __xdata struct hello_world_state *s = &(uip_conn->appstate);
	mov	a,#0x1c
	add	a,_uip_conn
	mov	r6,a
	clr	a
	addc	a,(_uip_conn + 1)
	mov	r7,a
;	hello-world.c:70: if(uip_connected()) {
	mov	dptr,#_uip_flags
	movx	a,@dptr
	jnb	acc.6,00102$
;	hello-world.c:71: PSOCK_INIT(&s->p, s->inputbuffer, sizeof(s->inputbuffer));
	mov	a,#0x15
	add	a,r6
	mov	_psock_init_PARM_2,a
	clr	a
	addc	a,r7
	mov	(_psock_init_PARM_2 + 1),a
	mov	_psock_init_PARM_3,#0x0a
	mov	(_psock_init_PARM_3 + 1),#0x00
	mov	dpl,r6
	mov	dph,r7
	push	ar7
	push	ar6
	lcall	_psock_init
	pop	ar6
	pop	ar7
00102$:
;	hello-world.c:79: write_char('A');
	mov	dpl,#0x41
	push	ar7
	push	ar6
	lcall	_write_char
	pop	ar6
	pop	ar7
;	hello-world.c:80: handle_connection(s);
	mov	dpl,r6
	mov	dph,r7
;	hello-world.c:81: }
	ljmp	_handle_connection
;------------------------------------------------------------
;Allocation info for local variables in function 'handle_connection'
;------------------------------------------------------------
;s                         Allocated to registers r6 r7 
;PT_YIELD_FLAG             Allocated to registers 
;buf                       Allocated with name '_handle_connection_buf_65536_59'
;------------------------------------------------------------
;	hello-world.c:90: handle_connection(__xdata struct hello_world_state *s)
;	-----------------------------------------
;	 function handle_connection
;	-----------------------------------------
_handle_connection:
;	hello-world.c:94: PSOCK_BEGIN(&s->p);
	mov	r6,dpl
	mov  r7,dph
	movx	a,@dptr
	mov	r4,a
	inc	dptr
	movx	a,@dptr
	mov	r5,a
	cjne	r4,#0x00,00123$
	cjne	r5,#0x00,00123$
	sjmp	00101$
00123$:
	cjne	r4,#0x63,00108$
	cjne	r5,#0x00,00108$
	sjmp	00102$
00101$:
;	hello-world.c:96: strtox(buf, "Hello. What is your name?\n");
	mov	_strtox_PARM_2,#___str_0
	mov	(_strtox_PARM_2 + 1),#(___str_0 >> 8)
	mov	dptr,#_handle_connection_buf_65536_59
	push	ar7
	push	ar6
	lcall	_strtox
	pop	ar6
	pop	ar7
;	hello-world.c:99: PSOCK_READTO(&s->p, '\n');
	mov	dpl,r6
	mov	dph,r7
	mov	a,#0x63
	movx	@dptr,a
	clr	a
	inc	dptr
	movx	@dptr,a
00102$:
	mov	_psock_readto_PARM_2,#0x0a
	mov	dpl,r6
	mov	dph,r7
	push	ar7
	push	ar6
	lcall	_psock_readto
	mov	a,dpl
	pop	ar6
	pop	ar7
	jnz	00106$
	mov	dpl,a
	ret
00106$:
;	hello-world.c:101: strtox(buf, "Name: ");
	mov	_strtox_PARM_2,#___str_1
	mov	(_strtox_PARM_2 + 1),#(___str_1 >> 8)
	mov	dptr,#_handle_connection_buf_65536_59
	push	ar7
	push	ar6
	lcall	_strtox
	pop	ar6
	pop	ar7
;	hello-world.c:104: PSOCK_CLOSE(&s->p);
	mov	dptr,#_uip_flags
	mov	a,#0x10
	movx	@dptr,a
;	hello-world.c:106: PSOCK_END(&s->p);
00108$:
	mov	dpl,r6
	mov	dph,r7
	clr	a
	movx	@dptr,a
	inc	dptr
	movx	@dptr,a
	mov	dpl,#0x02
;	hello-world.c:107: }
	ret
	.area BANK1   (CODE)
	.area CONST   (CODE)
	.area CONST   (CODE)
___str_0:
	.ascii "Hello. What is your name?"
	.db 0x0a
	.db 0x00
	.area BANK1   (CODE)
	.area CONST   (CODE)
___str_1:
	.ascii "Name: "
	.db 0x00
	.area BANK1   (CODE)
	.area XINIT   (CODE)
	.area CABS    (ABS,CODE)
