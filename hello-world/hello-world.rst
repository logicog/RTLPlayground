                                      1 ;--------------------------------------------------------
                                      2 ; File Created by SDCC : free open source ANSI-C Compiler
                                      3 ; Version 4.2.0 #13081 (Linux)
                                      4 ;--------------------------------------------------------
                                      5 	.module hello_world
                                      6 	.optsdcc -mmcs51 --model-small
                                      7 	
                                      8 ;--------------------------------------------------------
                                      9 ; Public variables in this module
                                     10 ;--------------------------------------------------------
                                     11 	.globl _uip_listen
                                     12 	.globl _psock_readto
                                     13 	.globl _psock_init
                                     14 	.globl _strtox
                                     15 	.globl _write_char
                                     16 	.globl _hello_world_init
                                     17 	.globl _hello_world_appcall
                                     18 ;--------------------------------------------------------
                                     19 ; special function registers
                                     20 ;--------------------------------------------------------
                                     21 	.area RSEG    (ABS,DATA)
      000000                         22 	.org 0x0000
                                     23 ;--------------------------------------------------------
                                     24 ; special function bits
                                     25 ;--------------------------------------------------------
                                     26 	.area RSEG    (ABS,DATA)
      000000                         27 	.org 0x0000
                                     28 ;--------------------------------------------------------
                                     29 ; overlayable register banks
                                     30 ;--------------------------------------------------------
                                     31 	.area REG_BANK_0	(REL,OVR,DATA)
      000000                         32 	.ds 8
                                     33 ;--------------------------------------------------------
                                     34 ; internal ram data
                                     35 ;--------------------------------------------------------
                                     36 	.area DSEG    (DATA)
                                     37 ;--------------------------------------------------------
                                     38 ; overlayable items in internal ram
                                     39 ;--------------------------------------------------------
                                     40 ;--------------------------------------------------------
                                     41 ; indirectly addressable internal ram data
                                     42 ;--------------------------------------------------------
                                     43 	.area ISEG    (DATA)
                                     44 ;--------------------------------------------------------
                                     45 ; absolute internal ram data
                                     46 ;--------------------------------------------------------
                                     47 	.area IABS    (ABS,DATA)
                                     48 	.area IABS    (ABS,DATA)
                                     49 ;--------------------------------------------------------
                                     50 ; bit data
                                     51 ;--------------------------------------------------------
                                     52 	.area BSEG    (BIT)
                                     53 ;--------------------------------------------------------
                                     54 ; paged external ram data
                                     55 ;--------------------------------------------------------
                                     56 	.area PSEG    (PAG,XDATA)
                                     57 ;--------------------------------------------------------
                                     58 ; external ram data
                                     59 ;--------------------------------------------------------
                                     60 	.area XSEG    (XDATA)
      001B2D                         61 _handle_connection_buf_65536_59:
      001B2D                         62 	.ds 100
                                     63 ;--------------------------------------------------------
                                     64 ; absolute external ram data
                                     65 ;--------------------------------------------------------
                                     66 	.area XABS    (ABS,XDATA)
                                     67 ;--------------------------------------------------------
                                     68 ; external initialized ram data
                                     69 ;--------------------------------------------------------
                                     70 	.area XISEG   (XDATA)
                                     71 	.area HOME    (CODE)
                                     72 	.area GSINIT0 (CODE)
                                     73 	.area GSINIT1 (CODE)
                                     74 	.area GSINIT2 (CODE)
                                     75 	.area GSINIT3 (CODE)
                                     76 	.area GSINIT4 (CODE)
                                     77 	.area GSINIT5 (CODE)
                                     78 	.area GSINIT  (CODE)
                                     79 	.area GSFINAL (CODE)
                                     80 	.area CSEG    (CODE)
                                     81 ;--------------------------------------------------------
                                     82 ; global & static initialisations
                                     83 ;--------------------------------------------------------
                                     84 	.area HOME    (CODE)
                                     85 	.area GSINIT  (CODE)
                                     86 	.area GSFINAL (CODE)
                                     87 	.area GSINIT  (CODE)
                                     88 ;--------------------------------------------------------
                                     89 ; Home
                                     90 ;--------------------------------------------------------
                                     91 	.area HOME    (CODE)
                                     92 	.area HOME    (CODE)
                                     93 ;--------------------------------------------------------
                                     94 ; code
                                     95 ;--------------------------------------------------------
                                     96 	.area BANK1   (CODE)
                                     97 ;------------------------------------------------------------
                                     98 ;Allocation info for local variables in function 'hello_world_init'
                                     99 ;------------------------------------------------------------
                                    100 ;	hello-world.c:43: hello_world_init(void)
                                    101 ;	-----------------------------------------
                                    102 ;	 function hello_world_init
                                    103 ;	-----------------------------------------
      01A00D                        104 _hello_world_init:
                           000007   105 	ar7 = 0x07
                           000006   106 	ar6 = 0x06
                           000005   107 	ar5 = 0x05
                           000004   108 	ar4 = 0x04
                           000003   109 	ar3 = 0x03
                           000002   110 	ar2 = 0x02
                           000001   111 	ar1 = 0x01
                           000000   112 	ar0 = 0x00
                                    113 ;	hello-world.c:46: uip_listen(HTONS(1000));
      01A00D 90 E8 03         [24]  114 	mov	dptr,#0xe803
                                    115 ;	hello-world.c:47: }
      01A010 02 7F C7         [24]  116 	ljmp	_uip_listen
                                    117 ;------------------------------------------------------------
                                    118 ;Allocation info for local variables in function 'hello_world_appcall'
                                    119 ;------------------------------------------------------------
                                    120 ;s                         Allocated to registers r6 r7 
                                    121 ;------------------------------------------------------------
                                    122 ;	hello-world.c:57: hello_world_appcall(void)
                                    123 ;	-----------------------------------------
                                    124 ;	 function hello_world_appcall
                                    125 ;	-----------------------------------------
      01A013                        126 _hello_world_appcall:
                                    127 ;	hello-world.c:64: __xdata struct hello_world_state *s = &(uip_conn->appstate);
      01A013 74 1C            [12]  128 	mov	a,#0x1c
      01A015 25 50            [12]  129 	add	a,_uip_conn
      01A017 FE               [12]  130 	mov	r6,a
      01A018 E4               [12]  131 	clr	a
      01A019 35 51            [12]  132 	addc	a,(_uip_conn + 1)
      01A01B FF               [12]  133 	mov	r7,a
                                    134 ;	hello-world.c:70: if(uip_connected()) {
      01A01C 90 0A C8         [24]  135 	mov	dptr,#_uip_flags
      01A01F E0               [24]  136 	movx	a,@dptr
      01A020 30 E6 1E         [24]  137 	jnb	acc.6,00102$
                                    138 ;	hello-world.c:71: PSOCK_INIT(&s->p, s->inputbuffer, sizeof(s->inputbuffer));
      01A023 74 15            [12]  139 	mov	a,#0x15
      01A025 2E               [12]  140 	add	a,r6
      01A026 F5 66            [12]  141 	mov	_psock_init_PARM_2,a
      01A028 E4               [12]  142 	clr	a
      01A029 3F               [12]  143 	addc	a,r7
      01A02A F5 67            [12]  144 	mov	(_psock_init_PARM_2 + 1),a
      01A02C 75 68 0A         [24]  145 	mov	_psock_init_PARM_3,#0x0a
      01A02F 75 69 00         [24]  146 	mov	(_psock_init_PARM_3 + 1),#0x00
      01A032 8E 82            [24]  147 	mov	dpl,r6
      01A034 8F 83            [24]  148 	mov	dph,r7
      01A036 C0 07            [24]  149 	push	ar7
      01A038 C0 06            [24]  150 	push	ar6
      01A03A 12 70 C4         [24]  151 	lcall	_psock_init
      01A03D D0 06            [24]  152 	pop	ar6
      01A03F D0 07            [24]  153 	pop	ar7
      01A041                        154 00102$:
                                    155 ;	hello-world.c:79: write_char('A');
      01A041 75 82 41         [24]  156 	mov	dpl,#0x41
      01A044 C0 07            [24]  157 	push	ar7
      01A046 C0 06            [24]  158 	push	ar6
      01A048 12 01 D8         [24]  159 	lcall	_write_char
      01A04B D0 06            [24]  160 	pop	ar6
      01A04D D0 07            [24]  161 	pop	ar7
                                    162 ;	hello-world.c:80: handle_connection(s);
      01A04F 8E 82            [24]  163 	mov	dpl,r6
      01A051 8F 83            [24]  164 	mov	dph,r7
                                    165 ;	hello-world.c:81: }
      01A053 02 A0 56         [24]  166 	ljmp	_handle_connection
                                    167 ;------------------------------------------------------------
                                    168 ;Allocation info for local variables in function 'handle_connection'
                                    169 ;------------------------------------------------------------
                                    170 ;s                         Allocated to registers r6 r7 
                                    171 ;PT_YIELD_FLAG             Allocated to registers 
                                    172 ;buf                       Allocated with name '_handle_connection_buf_65536_59'
                                    173 ;------------------------------------------------------------
                                    174 ;	hello-world.c:90: handle_connection(__xdata struct hello_world_state *s)
                                    175 ;	-----------------------------------------
                                    176 ;	 function handle_connection
                                    177 ;	-----------------------------------------
      01A056                        178 _handle_connection:
                                    179 ;	hello-world.c:94: PSOCK_BEGIN(&s->p);
      01A056 AE 82            [24]  180 	mov	r6,dpl
      01A058 AF 83            [24]  181 	mov  r7,dph
      01A05A E0               [24]  182 	movx	a,@dptr
      01A05B FC               [12]  183 	mov	r4,a
      01A05C A3               [24]  184 	inc	dptr
      01A05D E0               [24]  185 	movx	a,@dptr
      01A05E FD               [12]  186 	mov	r5,a
      01A05F BC 00 05         [24]  187 	cjne	r4,#0x00,00123$
      01A062 BD 00 02         [24]  188 	cjne	r5,#0x00,00123$
      01A065 80 08            [24]  189 	sjmp	00101$
      01A067                        190 00123$:
      01A067 BC 63 56         [24]  191 	cjne	r4,#0x63,00108$
      01A06A BD 00 53         [24]  192 	cjne	r5,#0x00,00108$
      01A06D 80 1E            [24]  193 	sjmp	00102$
      01A06F                        194 00101$:
                                    195 ;	hello-world.c:96: strtox(buf, "Hello. What is your name?\n");
      01A06F 75 66 9E         [24]  196 	mov	_strtox_PARM_2,#___str_0
      01A072 75 67 2C         [24]  197 	mov	(_strtox_PARM_2 + 1),#(___str_0 >> 8)
      01A075 90 1B 2D         [24]  198 	mov	dptr,#_handle_connection_buf_65536_59
      01A078 C0 07            [24]  199 	push	ar7
      01A07A C0 06            [24]  200 	push	ar6
      01A07C 12 02 A6         [24]  201 	lcall	_strtox
      01A07F D0 06            [24]  202 	pop	ar6
      01A081 D0 07            [24]  203 	pop	ar7
                                    204 ;	hello-world.c:99: PSOCK_READTO(&s->p, '\n');
      01A083 8E 82            [24]  205 	mov	dpl,r6
      01A085 8F 83            [24]  206 	mov	dph,r7
      01A087 74 63            [12]  207 	mov	a,#0x63
      01A089 F0               [24]  208 	movx	@dptr,a
      01A08A E4               [12]  209 	clr	a
      01A08B A3               [24]  210 	inc	dptr
      01A08C F0               [24]  211 	movx	@dptr,a
      01A08D                        212 00102$:
      01A08D 75 26 0A         [24]  213 	mov	_psock_readto_PARM_2,#0x0a
      01A090 8E 82            [24]  214 	mov	dpl,r6
      01A092 8F 83            [24]  215 	mov	dph,r7
      01A094 C0 07            [24]  216 	push	ar7
      01A096 C0 06            [24]  217 	push	ar6
      01A098 12 6A 24         [24]  218 	lcall	_psock_readto
      01A09B E5 82            [12]  219 	mov	a,dpl
      01A09D D0 06            [24]  220 	pop	ar6
      01A09F D0 07            [24]  221 	pop	ar7
      01A0A1 70 03            [24]  222 	jnz	00106$
      01A0A3 F5 82            [12]  223 	mov	dpl,a
      01A0A5 22               [24]  224 	ret
      01A0A6                        225 00106$:
                                    226 ;	hello-world.c:101: strtox(buf, "Name: ");
      01A0A6 75 66 B9         [24]  227 	mov	_strtox_PARM_2,#___str_1
      01A0A9 75 67 2C         [24]  228 	mov	(_strtox_PARM_2 + 1),#(___str_1 >> 8)
      01A0AC 90 1B 2D         [24]  229 	mov	dptr,#_handle_connection_buf_65536_59
      01A0AF C0 07            [24]  230 	push	ar7
      01A0B1 C0 06            [24]  231 	push	ar6
      01A0B3 12 02 A6         [24]  232 	lcall	_strtox
      01A0B6 D0 06            [24]  233 	pop	ar6
      01A0B8 D0 07            [24]  234 	pop	ar7
                                    235 ;	hello-world.c:104: PSOCK_CLOSE(&s->p);
      01A0BA 90 0A C8         [24]  236 	mov	dptr,#_uip_flags
      01A0BD 74 10            [12]  237 	mov	a,#0x10
      01A0BF F0               [24]  238 	movx	@dptr,a
                                    239 ;	hello-world.c:106: PSOCK_END(&s->p);
      01A0C0                        240 00108$:
      01A0C0 8E 82            [24]  241 	mov	dpl,r6
      01A0C2 8F 83            [24]  242 	mov	dph,r7
      01A0C4 E4               [12]  243 	clr	a
      01A0C5 F0               [24]  244 	movx	@dptr,a
      01A0C6 A3               [24]  245 	inc	dptr
      01A0C7 F0               [24]  246 	movx	@dptr,a
      01A0C8 75 82 02         [24]  247 	mov	dpl,#0x02
                                    248 ;	hello-world.c:107: }
      01A0CB 22               [24]  249 	ret
                                    250 	.area BANK1   (CODE)
                                    251 	.area CONST   (CODE)
                                    252 	.area CONST   (CODE)
      002C9E                        253 ___str_0:
      002C9E 48 65 6C 6C 6F 2E 20   254 	.ascii "Hello. What is your name?"
             57 68 61 74 20 69 73
             20 79 6F 75 72 20 6E
             61 6D 65 3F
      002CB7 0A                     255 	.db 0x0a
      002CB8 00                     256 	.db 0x00
                                    257 	.area BANK1   (CODE)
                                    258 	.area CONST   (CODE)
      002CB9                        259 ___str_1:
      002CB9 4E 61 6D 65 3A 20      260 	.ascii "Name: "
      002CBF 00                     261 	.db 0x00
                                    262 	.area BANK1   (CODE)
                                    263 	.area XINIT   (CODE)
                                    264 	.area CABS    (ABS,CODE)
