/* Copyright 2022 IBM Corp.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define SPR_HID0		0x3f0
#define SPR_HID0_POWER9_HILE	0x0800000000000000
#define H_SET_MODE		0x31c
#define RESOURCE_LPCR_ILE	4
#define MFLAGS_SET		1
#define H_RTAS			0xf000

/* Load an immediate 64-bit value into a register */
#define LOAD_IMM64(r, e)			\
	lis	r,(e)@highest;			\
	ori	r,r,(e)@higher;			\
	rldicr	r,r, 32, 31;			\
	oris	r,r, (e)@h;			\
	ori	r,r, (e)@l;

#define FIXUP_ENDIAN \
	tdi   0,0,0x48;   /* Reverse endian of b . + 8 */           \
	b     $+44;       /* Skip trampoline if endian is good */   \
	.long 0xa600607d; /* mfmsr r11 */                           \
	.long 0x01006b69; /* xori r11,r11,1 */                      \
	.long 0x00004039; /* li r10,0 */                            \
	.long 0x6401417d; /* mtmsrd r10,1 */                        \
	.long 0x05009f42; /* bcl 20,31,$+4 */                       \
	.long 0xa602487d; /* mflr r10 */                            \
	.long 0x14004a39; /* addi r10,r10,20 */                     \
	.long 0xa6035a7d; /* mtsrr0 r10 */                          \
	.long 0xa6037b7d; /* mtsrr1 r11 */                          \
	.long 0x2400004c  /* rfid */

	.section ".head","ax"

	. = 0x100
	FIXUP_ENDIAN
.global start
start:
	mfmsr	%r9
	rldicl.	%r9,%r9,4,63
	beq	1f
	mfspr	%r10, SPR_HID0
	LOAD_IMM64(%r11, SPR_HID0_POWER9_HILE)
	or	%r10, %r10, %r11
	mtspr	SPR_HID0, %r10
	b 2f
1:	li	%r3,H_SET_MODE
	li	%r4,MFLAGS_SET
	li	%r5,RESOURCE_LPCR_ILE
	li	%r6,0
	li	%r7,0
	sc	1
2:
	LOAD_IMM64(%r1,__stack_top)
	li	%r0,0
	stdu	%r0,-16(%r1)
	LOAD_IMM64(%r12, main)
	mtctr	%r12
	bctrl
	li	%r3,0
	ori	%r3,%r3,H_RTAS
	LOAD_IMM64(%r4, rtas_argbuf)
	sc	1
	b .

#define EXCEPTION(nr)		\
	.= nr			;\
	b	.

	EXCEPTION(0x300)
	EXCEPTION(0x380)
	EXCEPTION(0x400)
	EXCEPTION(0x480)
	EXCEPTION(0x500)
	EXCEPTION(0x600)
	EXCEPTION(0x700)
	EXCEPTION(0x800)
	EXCEPTION(0x900)
	EXCEPTION(0x980)
	EXCEPTION(0xa00)
	EXCEPTION(0xb00)
	EXCEPTION(0xc00)
	EXCEPTION(0xd00)
	EXCEPTION(0xe00)
	EXCEPTION(0xe20)
	EXCEPTION(0xe40)
	EXCEPTION(0xe60)
	EXCEPTION(0xe80)
	EXCEPTION(0xf00)
	EXCEPTION(0xf20)
	EXCEPTION(0xf40)
	EXCEPTION(0xf60)
	EXCEPTION(0xf80)

    .section ".text"
.global test
test:
    mfdec %r3
    blr

.global init_gprs
init_gprs:
	reg = 4
	.rept	27
	ld	reg, reg*8(%r3)
	reg = reg + 1
	.endr
	blr

.global save_gprs
save_gprs:
	reg = 4
	.rept	28
	std	reg, reg*8(%r3)
	reg = reg + 1
	.endr
	blr

/*
 * r3: struct one_spr *
 */
.global init_sprs
init_sprs:
	i = 0
	reg = 0
	.rept	1023
	li	%r9,i
	ldx	%r8,%r3,%r9
	cmpdi	%r8,0
	beq	.+16
	addi	%r9,%r9,8
	ldx	%r8,%r3,%r9
	mtspr	reg,%r8
	i = i + 16
	reg = reg + 1
	.endr
	blr

/*
 * r3: struct one_spr *
 */
.global save_sprs
save_sprs:
	i = 0
	reg = 0
	.rept	1023
	li	%r9,i
	ldx	%r8,%r3,%r9
	cmpdi	%r8,0
	beq	.+16
	mfspr	%r8,reg
	addi	%r9,%r9,8
	stdx	%r8,%r3,%r9
	i = i + 16
	reg = reg + 1
	.endr
	blr

.global rtas_call
rtas_call:
	li	%r3,0
	ori	%r3,%r3,H_RTAS
	LOAD_IMM64(%r4, rtas_argbuf)
	sc	1
	blr
