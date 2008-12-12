/* Copyright (c) 2005-2006 Russ Cox, MIT; see COPYRIGHT */

#if defined(__FreeBSD__) && defined(__i386__) && __FreeBSD__ < 5
#define NEEDX86CONTEXT 1
#define SET setmcontext
#define GET getmcontext
#endif

#if defined(__APPLE__) && defined(__i386__)
#define NEEDX86CONTEXT 1
#define SET _setmcontext
#define GET _getmcontext
#endif

#if defined(__APPLE__) && defined(__x86_64__)
#define NEEDX86_64CONTEXT 1
#define SET _setmcontext
#define GET _getmcontext
#endif

#if defined(__APPLE__) && defined(__ppc64__)
#define NEEDPOWER64CONTEXT 1
#define SET __setmcontext
#define GET __getmcontext
#endif

#if defined(__APPLE__) && \
   !defined(__i386__) && !defined(__x86_64__) && \
   !defined(__ppc64__)
#define NEEDPOWERCONTEXT 1
#define SET __setmcontext
#define GET __getmcontext
#endif

#if defined(__linux__) && defined(__arm__)
#define NEEDARMCONTEXT 1
#define SET setmcontext
#define GET getmcontext
#endif

#ifdef NEEDX86CONTEXT
.globl SET
SET:
	movl	4(%esp), %eax /* technically doesn't need to be saved? */

	movl	8(%eax), %fs
	movl	12(%eax), %es
	movl	16(%eax), %ds
	movl	76(%eax), %ss
	movl	20(%eax), %edi
	movl	24(%eax), %esi
	movl	28(%eax), %ebp
	movl	36(%eax), %ebx
	movl	40(%eax), %edx
	movl	44(%eax), %ecx

	movl	72(%eax), %esp
	pushl	60(%eax)	/* new %eip */
	movl	48(%eax), %eax
	ret

.globl GET
GET:
	movl	4(%esp), %eax /* load arg0 from stack */

	movl	%fs, 8(%eax)
	movl	%es, 12(%eax)
	movl	%ds, 16(%eax)
	movl	%ss, 76(%eax)
	movl	%edi, 20(%eax)
	movl	%esi, 24(%eax)
	movl	%ebp, 28(%eax)
	movl	%ebx, 36(%eax)
	movl	%edx, 40(%eax)		 
	movl	%ecx, 44(%eax)

	movl	$1, 48(%eax)	/* %eax */
	movl	(%esp), %ecx	/* %eip */
	movl	%ecx, 60(%eax)
	leal	4(%esp), %ecx	/* %esp */
	movl	%ecx, 72(%eax)

	movl	44(%eax), %ecx	/* restore %ecx */
	movl	$0, %eax
	ret
#endif

#ifdef NEEDX86_64CONTEXT
/* Register Usage:
 *
 * %rax			temp register, 1st return reg
 * %rbx			callee-saved, sometimes "base ptr"		PRESERVED
 * %rcx			4th argument
 * %rdx			3rd argument, 2nd return
 * %rsp			stack pointer							PRESERVED
 * %rbp			optional frame pointer					PRESERVED
 * %rsi			2nd argument
 * %rdi			1st argument
 * %r8			5th argument
 * %r9			6th argument
 * %r10			temp register, static chain pointer
 * %r11			temp register
 * %r12-r15		saved									PRESERVED
 * %xmm0-xmm1	floating point args, float return
 * %xmm2-xmm7	floating point args
 * %xmm8-xmm15	temp registers
 * %mmx0-mmx7	temp registers
 * %st0,%st1	temp registers, for returning long doubles
 * %st2-st7		temp registers
 * %fs			reserved for system (thread-specific data)
 * mxcsr		status...
 *  x87 SW		status word
 *  x87 CW		control word							PRESERVED
 */
.globl SET
SET:
	/* the argument is %rdi, so we do not need to load it from the stack */

	movq	(6*8)(%rdi), %rsi /* 2nd int arg (arg passing) */
	movq	(7*8)(%rdi), %rbp /* frame pointer */
	movq	(9*8)(%rdi), %rbx /* base pointer */
	movq	(10*8)(%rdi), %rdx /* 3rd int arg (arg passing) */
	movq	(11*8)(%rdi), %rcx /* 4th int arg (arg passing) */
	movq	(12*8)(%rdi), %rax /* return value (allows us to escape the swapcontext loop) */
	/* extra registers in AMD (arg passing) */
	movq	((20+28+0)*8)(%rdi), %r12 /* store 'em in the __spare__ area */
	movq	((20+28+1)*8)(%rdi), %r13
	movq	((20+28+2)*8)(%rdi), %r14
	movq	((20+28+3)*8)(%rdi), %r15

	movq	(18*8)(%rdi), %rsp /* stack pointer */
	pushq	(15*8)(%rdi) /* push new $pc onto stack for `ret` */
	movq	(5*8)(%rdi), %rdi /* 1st int arg (arg passing) */
	ret

.globl GET
GET:
	/* arg0 is in %rdi, DO NOT load from stack */
	movq	%rdi, ( 5*8)(%rdi) /* unnecessary */
	movq	%rsi, ( 6*8)(%rdi) /* unnecessary */
	movq	%rbp, ( 7*8)(%rdi) /* frame pointer */
	movq	%rbx, ( 9*8)(%rdi) /* base pointer */
	movq	%rdx, (10*8)(%rdi) /* unnecessary */
	movq	%rcx, (11*8)(%rdi) /* unnecessary */
	/* extra registers in AMD - callee saved */
	movq	%r12, ((20+28+0)*8)(%rdi) /* in the __spare__ area */
	movq	%r13, ((20+28+1)*8)(%rdi)
	movq	%r14, ((20+28+2)*8)(%rdi)
	movq	%r15, ((20+28+3)*8)(%rdi)
	
	movq	$1, (12*8)(%rdi)	/* %rax (aka, my return value) */
	/* the return value is 1 here in order to avoid getting into a
	 * swapcontext loop. If this isn't zero, what will happen is
	 * that every time we swap back to the parent, it will think
	 * we just finished the getcontext, and will immediately swap
	 * back to the child. */

	/* the following is broken into two instructions
	 * because we can only dereference one memory op at a time */
	movq	(%rsp), %rcx	/* return address, storing in %eip */
	movq	%rcx, (15*8)(%rdi)

	/* I don't entirely understand why this is correct */
	leaq	8(%rsp), %rcx	 /* %rsp */
	movq	%rcx, (18*8)(%rdi)

	/* put %rcx back the way it should be, after the
	 * previous shenanigans */
	movq	(11*8)(%rdi), %rcx	/* restore %rcx */

	mov		$0, %rax /* set return value - success! */
	ret
#endif

#ifdef NEEDPOWERCONTEXT
/* get FPR and VR use flags with sc 0x7FF3 */
/* get vsave with mfspr reg, 256 */

.text
.align 2

.globl GET
GET:				/* xxx: instruction scheduling */
	mflr	r0
	mfcr	r5
	mfctr	r6
	mfxer	r7
	stw	r0, 0*4(r3) /* pc - program counter */
	stw	r5, 1*4(r3) /* cr - condition register */
	stw	r6, 2*4(r3) /* ctr - count register */
	stw	r7, 3*4(r3) /* xer */

	stw	r1, 4*4(r3) /* sp */
	stw	r2, 5*4(r3) /* toc */
	li	r5, 1			/* return value for setmcontext */
	stw	r5, 6*4(r3) /* arg1 & return-value */

	stw	r13, (0+7)*4(r3)	/* callee-save GPRs */
	stw	r14, (1+7)*4(r3)	/* not a block move b/c that could be slower */
	stw	r15, (2+7)*4(r3)
	stw	r16, (3+7)*4(r3)
	stw	r17, (4+7)*4(r3)
	stw	r18, (5+7)*4(r3)
	stw	r19, (6+7)*4(r3)
	stw	r20, (7+7)*4(r3)
	stw	r21, (8+7)*4(r3)
	stw	r22, (9+7)*4(r3)
	stw	r23, (10+7)*4(r3)
	stw	r24, (11+7)*4(r3)
	stw	r25, (12+7)*4(r3)
	stw	r26, (13+7)*4(r3)
	stw	r27, (14+7)*4(r3)
	stw	r28, (15+7)*4(r3)
	stw	r29, (16+7)*4(r3)
	stw	r30, (17+7)*4(r3)
	stw	r31, (18+7)*4(r3)

	/* skip 4 bytes for pad... 19+7 = 26 */

	stfd f14, (0)*8+(26*4)(r3)	/* callee-save FPRs */
	stfd f15, (1)*8+(26*4)(r3)
	stfd f16, (2)*8+(26*4)(r3)
	stfd f17, (3)*8+(26*4)(r3)
	stfd f18, (4)*8+(26*4)(r3)
	stfd f19, (5)*8+(26*4)(r3)
	stfd f20, (6)*8+(26*4)(r3)
	stfd f21, (7)*8+(26*4)(r3)
	stfd f22, (8)*8+(26*4)(r3)
	stfd f23, (9)*8+(26*4)(r3)
	stfd f24, (10)*8+(26*4)(r3)
	stfd f25, (11)*8+(26*4)(r3)
	stfd f26, (12)*8+(26*4)(r3)
	stfd f27, (13)*8+(26*4)(r3)
	stfd f28, (14)*8+(26*4)(r3)
	stfd f29, (15)*8+(26*4)(r3)
	stfd f30, (16)*8+(26*4)(r3)
	stfd f31, (17)*8+(26*4)(r3)

	li	r3, 0			/* return */
	blr

.globl SET
SET:
	lwz	r13, (0+7)*4(r3)	/* callee-save GPRs */
	lwz	r14, (1+7)*4(r3)	/* not a block move b/c that could be slower */
	lwz	r15, (2+7)*4(r3)
	lwz	r16, (3+7)*4(r3)
	lwz	r17, (4+7)*4(r3)
	lwz	r18, (5+7)*4(r3)
	lwz	r19, (6+7)*4(r3)
	lwz	r20, (7+7)*4(r3)
	lwz	r21, (8+7)*4(r3)
	lwz	r22, (9+7)*4(r3)
	lwz	r23, (10+7)*4(r3)
	lwz	r24, (11+7)*4(r3)
	lwz	r25, (12+7)*4(r3)
	lwz	r26, (13+7)*4(r3)
	lwz	r27, (14+7)*4(r3)
	lwz	r28, (15+7)*4(r3)
	lwz	r29, (16+7)*4(r3)
	lwz	r30, (17+7)*4(r3)
	lwz	r31, (18+7)*4(r3)

	/* skip 4 bytes for pad... 19+7 = 26 */

	lfd f14, (0)*8+(26*4)(r3)	/* callee-save FPRs */
	lfd f15, (1)*8+(26*4)(r3)
	lfd f16, (2)*8+(26*4)(r3)
	lfd f17, (3)*8+(26*4)(r3)
	lfd f18, (4)*8+(26*4)(r3)
	lfd f19, (5)*8+(26*4)(r3)
	lfd f20, (6)*8+(26*4)(r3)
	lfd f21, (7)*8+(26*4)(r3)
	lfd f22, (8)*8+(26*4)(r3)
	lfd f23, (9)*8+(26*4)(r3)
	lfd f24, (10)*8+(26*4)(r3)
	lfd f25, (11)*8+(26*4)(r3)
	lfd f26, (12)*8+(26*4)(r3)
	lfd f27, (13)*8+(26*4)(r3)
	lfd f28, (14)*8+(26*4)(r3)
	lfd f29, (15)*8+(26*4)(r3)
	lfd f30, (16)*8+(26*4)(r3)
	lfd f31, (17)*8+(26*4)(r3)

	lwz	r1, 4*4(r3)
	lwz	r2, 5*4(r3)

	lwz	r0, 0*4(r3)
	mtlr	r0
	lwz	r0, 1*4(r3)
	mtcr	r0			/* mtcrf 0xFF, r0 */
	lwz	r0, 2*4(r3)
	mtctr	r0
	lwz	r0, 3*4(r3)
	mtxer	r0

	lwz	r3,	6*4(r3)
	blr
#endif

#ifdef NEEDPOWER64CONTEXT
/* get VR use flags with sc 0x7FF3 */
/* get vsave with mfspr reg, 256 */

.text
.align 2

.globl GET
GET:				/* xxx: instruction scheduling */
	mflr	r0
	mfcr	r5
	mfctr	r6
	mfxer	r7
	std	r0, 0*8(r3)
	std	r5, 1*8(r3)
	std	r6, 2*8(r3)
	std	r7, 3*8(r3)

	std	r1, 4*8(r3)
	std	r2, 5*8(r3)
	li	r5, 1			/* return value for setmcontext */
	std	r5, 6*8(r3)

	std	r13, (0+7)*8(r3)	/* callee-save GPRs */
	std	r14, (1+7)*8(r3)	/* xxx: block move */
	std	r15, (2+7)*8(r3)
	std	r16, (3+7)*8(r3)
	std	r17, (4+7)*8(r3)
	std	r18, (5+7)*8(r3)
	std	r19, (6+7)*8(r3)
	std	r20, (7+7)*8(r3)
	std	r21, (8+7)*8(r3)
	std	r22, (9+7)*8(r3)
	std	r23, (10+7)*8(r3)
	std	r24, (11+7)*8(r3)
	std	r25, (12+7)*8(r3)
	std	r26, (13+7)*8(r3)
	std	r27, (14+7)*8(r3)
	std	r28, (15+7)*8(r3)
	std	r29, (16+7)*8(r3)
	std	r30, (17+7)*8(r3)
	std	r31, (18+7)*8(r3)

	/* the pad is unnecessary in 64 bit mode ... (minor bug!) */

	stfd f14, (0+26)*8(r3) /* callee-save FPRs */
	stfd f15, (1+26)*8(r3)
	stfd f16, (2+26)*8(r3)
	stfd f17, (3+26)*8(r3)
	stfd f18, (4+26)*8(r3)
	stfd f19, (5+26)*8(r3)
	stfd f20, (6+26)*8(r3)
	stfd f21, (7+26)*8(r3)
	stfd f22, (8+26)*8(r3)
	stfd f23, (9+26)*8(r3)
	stfd f24, (10+26)*8(r3)
	stfd f25, (11+26)*8(r3)
	stfd f26, (12+26)*8(r3)
	stfd f27, (13+26)*8(r3)
	stfd f28, (14+26)*8(r3)
	stfd f29, (15+26)*8(r3)
	stfd f30, (16+26)*8(r3)
	stfd f31, (17+26)*8(r3)

	li	r3, 0			/* return */
	blr

.globl SET
SET:
	ld	r13, (0+7)*8(r3)	/* callee-save GPRs */
	ld	r14, (1+7)*8(r3)	/* xxx: block move */
	ld	r15, (2+7)*8(r3)
	ld	r16, (3+7)*8(r3)
	ld	r17, (4+7)*8(r3)
	ld	r18, (5+7)*8(r3)
	ld	r19, (6+7)*8(r3)
	ld	r20, (7+7)*8(r3)
	ld	r21, (8+7)*8(r3)
	ld	r22, (9+7)*8(r3)
	ld	r23, (10+7)*8(r3)
	ld	r24, (11+7)*8(r3)
	ld	r25, (12+7)*8(r3)
	ld	r26, (13+7)*8(r3)
	ld	r27, (14+7)*8(r3)
	ld	r28, (15+7)*8(r3)
	ld	r29, (16+7)*8(r3)
	ld	r30, (17+7)*8(r3)
	ld	r31, (18+7)*8(r3)

	lfd f14, (0+26)*8(r3) /* callee-save FPRs */
	lfd f15, (1+26)*8(r3)
	lfd f16, (2+26)*8(r3)
	lfd f17, (3+26)*8(r3)
	lfd f18, (4+26)*8(r3)
	lfd f19, (5+26)*8(r3)
	lfd f20, (6+26)*8(r3)
	lfd f21, (7+26)*8(r3)
	lfd f22, (8+26)*8(r3)
	lfd f23, (9+26)*8(r3)
	lfd f24, (10+26)*8(r3)
	lfd f25, (11+26)*8(r3)
	lfd f26, (12+26)*8(r3)
	lfd f27, (13+26)*8(r3)
	lfd f28, (14+26)*8(r3)
	lfd f29, (15+26)*8(r3)
	lfd f30, (16+26)*8(r3)
	lfd f31, (17+26)*8(r3)

	ld	r1, 4*8(r3)
	ld	r2, 5*8(r3)

	ld	r0, 0*8(r3)
	mtlr	r0
	ld	r0, 1*8(r3)
	mtcr	r0			/* mtcrf 0xFF, r0 */
	ld	r0, 2*8(r3)
	mtctr	r0
	ld	r0, 3*8(r3)
	mtxer	r0

	ld	r3,	6*8(r3)
	blr
#endif

#ifdef NEEDARMCONTEXT
.globl GET
GET:
	str	r1, [r0,#4]
	str	r2, [r0,#8]
	str	r3, [r0,#12]
	str	r4, [r0,#16]
	str	r5, [r0,#20]
	str	r6, [r0,#24]
	str	r7, [r0,#28]
	str	r8, [r0,#32]
	str	r9, [r0,#36]
	str	r10, [r0,#40]
	str	r11, [r0,#44]
	str	r12, [r0,#48]
	str	r13, [r0,#52]
	str	r14, [r0,#56]
	/* store 1 as r0-to-restore */
	mov	r1, #1
	str	r1, [r0]
	/* return 0 */
	mov	r0, #0
	mov	pc, lr

.globl SET
SET:
	ldr	r1, [r0,#4]
	ldr	r2, [r0,#8]
	ldr	r3, [r0,#12]
	ldr	r4, [r0,#16]
	ldr	r5, [r0,#20]
	ldr	r6, [r0,#24]
	ldr	r7, [r0,#28]
	ldr	r8, [r0,#32]
	ldr	r9, [r0,#36]
	ldr	r10, [r0,#40]
	ldr	r11, [r0,#44]
	ldr	r12, [r0,#48]
	ldr	r13, [r0,#52]
	ldr	r14, [r0,#56]
	ldr	r0, [r0]
	mov	pc, lr
#endif

#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif

