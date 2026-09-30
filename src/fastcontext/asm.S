// Portions of this file are Copyright (c) 2025 Tactical Computing Labs, LLC; see COPYING
// Portions of this file are Copyright (c) 2005-2006 Russ Cox, MIT; see COPYRIGHT
// Portions of this file are Copyright Sandia National Laboratories

#include "qthread/common.h"

// Some operating systems mangle symbols like this.
#if defined(__APPLE__) || defined(__CYGWIN32__)
# define SET _qt_setmctxt
# define GET _qt_getmctxt
#else
# define SET qt_setmctxt
# define GET qt_getmctxt
#endif

#if QTHREAD_ASSEMBLY_ARCH == QTHREAD_IA32
.globl SET
SET:
    movl    4(%esp), %eax     // get the offset of the uc_mcontext component (arguments are passed via the stack)

    movl    (0*4)(%eax), %edi // restore EDI: general 32-bit register
    movl    (1*4)(%eax), %ebp // restore EBP: Stack frame pointer
    movl    (2*4)(%eax), %ebx // restore EBX: PIC base register
    movl    (3*4)(%eax), %esi // restore ESI: general 32-bit register
    ldmxcsr (6*4)(%eax)       // restore SSE2 control and status word
    fldcw   ((6*4)+4)(%eax)   // restore x87 control word

    movl    (4*4)(%eax), %esp // set the stack
    movl    (5*4)(%eax), %ecx // get the new PC to jump to

    movl    $1,          %eax // return value (allows us to escape the swapcontext loop)
    jmp     *%ecx             // jump to the new PC

.globl GET
GET:
    movl    4(%esp), %eax     // get the offset of the uc_mcontext component

    movl    %edi, (0*4)(%eax) // save EDI: general 32-bit register
    movl    %ebp, (1*4)(%eax) // save EBP: Stack frame pointer
    movl    %ebx, (2*4)(%eax) // save EBX: PIC base register
    movl    %esi, (3*4)(%eax) // save ESI: general 32-bit register
    stmxcsr (6*4)(%eax)       // save SSE2 control and status word
    fnstcw  ((6*4)+4)(%eax)   // save x87 control word

    leal    4(%esp), %ecx     // load the stack pointer without the return address
    movl    %ecx, (4*4)(%eax) // save the stack pointer

    movl    (%esp), %ecx      // load the current return location, which is stored on the stack
    movl    %ecx, (5*4)(%eax) // store the current return location

    movl    $0,   %eax        // return value (allows us to escape the swapcontext loop)
    ret

#elif QTHREAD_ASSEMBLY_ARCH == QTHREAD_AMD64
/* Register Usage:
 *
 * %rax                 temp register, 1st return reg
 * %rbx                 callee-saved, sometimes "base ptr"           PRESERVED
 * %rcx                 4th argument
 * %rdx                 3rd argument, 2nd return
 * %rsp                 stack pointer                                PRESERVED
 * %rbp                 optional frame pointer                       PRESERVED
 * %rsi                 2nd argument
 * %rdi                 1st argument
 * %r8                  5th argument
 * %r9                  6th argument
 * %r10                 temp register, static chain pointer
 * %r11                 temp register
 * %r12-r15             saved                                        PRESERVED
 * %xmm0-xmm1   floating point args, float return
 * %xmm2-xmm7   floating point args
 * %xmm8-xmm15  temp registers
 * %mmx0-mmx7   temp registers
 * %st0,%st1    temp registers, for returning long doubles
 * %st2-st7             temp registers
 * %fs                  reserved for system (thread-specific data)
 * mxcsr                status...
 *  x87 SW              status word
 *  x87 CW              control word                                 PRESERVED
 */
.globl SET
SET:
    // the argument is %rdi, so we do not need to load it from the stack */

    movq    (1*8)(%rdi), %rbp // frame pointer
    movq    (2*8)(%rdi), %rbx // base pointer

    // extra caller-saved registers in AMD
    movq    (3*8)(%rdi), %r12
    movq    (4*8)(%rdi), %r13
    movq    (5*8)(%rdi), %r14
    movq    (6*8)(%rdi), %r15
    ldmxcsr (9*8)(%rdi)       // restore SSE2 control and status word
    fldcw   ((9*8)+4)(%rdi)   // restore x87 control word
    movq    (7*8)(%rdi), %rsp // stack pointer
    pushq   (8*8)(%rdi)       // push new $pc onto stack for `ret`
    movq    (0*8)(%rdi), %rdi // 1st int arg (arg passing); only necessary for first context swap into a new qthread

    movq    $1,          %rax // return value (allows us to escape the swapcontext loop)
    ret

.globl GET
GET:
    // arg0 is in %rdi, DO NOT load from stack
    movq    %rdi, (0*8)(%rdi) // set rdi properly (???)

    movq    %rbp, (1*8)(%rdi) // frame pointer
    movq    %rbx, (2*8)(%rdi) // base pointer

    // extra registers in AMD - callee saved
    movq    %r12, (3*8)(%rdi) // in the __spare__ area
    movq    %r13, (4*8)(%rdi)
    movq    %r14, (5*8)(%rdi)
    movq    %r15, (6*8)(%rdi)
    stmxcsr (9*8)(%rdi)       // save SSE2 control and status word
    fnstcw  ((9*8)+4)(%rdi)   // save x86 control word

    // the following is broken into two instructions
    // because we can only dereference one memory op at a time
    movq    (%rsp), %rcx
    movq    %rcx, (8*8)(%rdi)

    // I don't entirely understand why this is correct
    leaq    8(%rsp), %rcx     // %rsp
    movq    %rcx, (7*8)(%rdi)

    mov     $0, %rax // set return value - success!
    ret

#elif QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC32
// get FPR and VR use flags with sc 0x7FF3
// get vsave with mfspr reg, 256

.text
.align 2

.globl GET
GET:
    mflr  0
    mfcr  5
    mfctr 6
    mfxer 7
    stw   0, 0*4(3) // link register becomes new program counter
    stw   5, 1*4(3) // cr - condition register
    stw   6, 2*4(3) // ctr - count register
    stw   7, 3*4(3) // fixed-point exception register (xer)

    stw   1, 4*4(3) // stack frame pointer
    stw   2, 5*4(3) // system-reserved (TLS/toc)
    li    5, 1      // return value for qt_setmctxt
    stw   5, 6*4(3) // arg1 & return-value

    stw   13, (0+7)*4(3) // callee-save GPRs
    stw   14, (1+7)*4(3) // not a block move b/c that could be slower
    stw   15, (2+7)*4(3)
    stw   16, (3+7)*4(3)
    stw   17, (4+7)*4(3)
    stw   18, (5+7)*4(3)
    stw   19, (6+7)*4(3)
    stw   20, (7+7)*4(3)
    stw   21, (8+7)*4(3)
    stw   22, (9+7)*4(3)
    stw   23, (10+7)*4(3)
    stw   24, (11+7)*4(3)
    stw   25, (12+7)*4(3)
    stw   26, (13+7)*4(3)
    stw   27, (14+7)*4(3)
    stw   28, (15+7)*4(3)
    stw   29, (16+7)*4(3)
    stw   30, (17+7)*4(3)
    stw   31, (18+7)*4(3)

    // skip 4 bytes for pad... 19+7 = 26

    stfd 14, (0)*8+(26*4)(3) // callee-save FPRs
    stfd 15, (1)*8+(26*4)(3)
    stfd 16, (2)*8+(26*4)(3)
    stfd 17, (3)*8+(26*4)(3)
    stfd 18, (4)*8+(26*4)(3)
    stfd 19, (5)*8+(26*4)(3)
    stfd 20, (6)*8+(26*4)(3)
    stfd 21, (7)*8+(26*4)(3)
    stfd 22, (8)*8+(26*4)(3)
    stfd 23, (9)*8+(26*4)(3)
    stfd 24, (10)*8+(26*4)(3)
    stfd 25, (11)*8+(26*4)(3)
    stfd 26, (12)*8+(26*4)(3)
    stfd 27, (13)*8+(26*4)(3)
    stfd 28, (14)*8+(26*4)(3)
    stfd 29, (15)*8+(26*4)(3)
    stfd 30, (16)*8+(26*4)(3)
    stfd 31, (17)*8+(26*4)(3)

    li      3, 0 // return
    blr

.globl SET
SET:
    lwz 13, (0+7)*4(3) // callee-save GPRs
    lwz 14, (1+7)*4(3) // not a block move b/c that could be slower
    lwz 15, (2+7)*4(3)
    lwz 16, (3+7)*4(3)
    lwz 17, (4+7)*4(3)
    lwz 18, (5+7)*4(3)
    lwz 19, (6+7)*4(3)
    lwz 20, (7+7)*4(3)
    lwz 21, (8+7)*4(3)
    lwz 22, (9+7)*4(3)
    lwz 23, (10+7)*4(3)
    lwz 24, (11+7)*4(3)
    lwz 25, (12+7)*4(3)
    lwz 26, (13+7)*4(3)
    lwz 27, (14+7)*4(3)
    lwz 28, (15+7)*4(3)
    lwz 29, (16+7)*4(3)
    lwz 30, (17+7)*4(3)
    lwz 31, (18+7)*4(3)

    // skip 4 bytes for pad... 19+7 = 26

    lfd 14, (0)*8+(26*4)(3) // callee-save FPRs
    lfd 15, (1)*8+(26*4)(3)
    lfd 16, (2)*8+(26*4)(3)
    lfd 17, (3)*8+(26*4)(3)
    lfd 18, (4)*8+(26*4)(3)
    lfd 19, (5)*8+(26*4)(3)
    lfd 20, (6)*8+(26*4)(3)
    lfd 21, (7)*8+(26*4)(3)
    lfd 22, (8)*8+(26*4)(3)
    lfd 23, (9)*8+(26*4)(3)
    lfd 24, (10)*8+(26*4)(3)
    lfd 25, (11)*8+(26*4)(3)
    lfd 26, (12)*8+(26*4)(3)
    lfd 27, (13)*8+(26*4)(3)
    lfd 28, (14)*8+(26*4)(3)
    lfd 29, (15)*8+(26*4)(3)
    lfd 30, (16)*8+(26*4)(3)
    lfd 31, (17)*8+(26*4)(3)

    lwz   1, 4*4(3)
    lwz   2, 5*4(3)

    lwz   0, 0*4(3)
    mtlr  0
    lwz   0, 1*4(3)
    mtcr  0         // mtcrf 0xFF, 0
    lwz   0, 2*4(3)
    mtctr 0
    lwz   0, 3*4(3)
    mtxer 0

    lwz   3, 6*4(3)
    blr

#elif QTHREAD_ASSEMBLY_ARCH == QTHREAD_POWERPC64
// get VR use flags with sc 0x7FF3 */
// get vsave with mfspr reg, 256 */

.text
.align 2

.globl GET
GET:
    mflr  0
    mfcr  5
    mfctr 6
    mfxer 7

    std 0, 0*8(3) // link register becomes new program counter
    std 5, 1*8(3) // condition codes
    std 6, 2*8(3) // loop counter register (maybe does not need to be saved)
    std 7, 3*8(3) // fixed point exception register (maybe don't save?)

    std 1, 4*8(3) // stack frame pointer
    std 2, 5*8(3) // TOC pointer (TLS)
    li  5, 1      // return value for qt_setmctxt
    std 5, 6*8(3)

    // std   13, (0+7)*8(3) not saving 13 because I want thread migration
    std 14, (1+7)*8(3) // callee-save GPRs
    std 15, (2+7)*8(3) // xxx: block move
    std 16, (3+7)*8(3)
    std 17, (4+7)*8(3)
    std 18, (5+7)*8(3)
    std 19, (6+7)*8(3)
    std 20, (7+7)*8(3)
    std 21, (8+7)*8(3)
    std 22, (9+7)*8(3)
    std 23, (10+7)*8(3)
    std 24, (11+7)*8(3)
    std 25, (12+7)*8(3)
    std 26, (13+7)*8(3)
    std 27, (14+7)*8(3)
    std 28, (15+7)*8(3)
    std 29, (16+7)*8(3)
    std 30, (17+7)*8(3)
    std 31, (18+7)*8(3)

    // the pad is unnecessary in 64 bit mode ... (minor bug!)

    stfd 14, (0+26)*8(3) // callee-save FPRs
    stfd 15, (1+26)*8(3)
    stfd 16, (2+26)*8(3)
    stfd 17, (3+26)*8(3)
    stfd 18, (4+26)*8(3)
    stfd 19, (5+26)*8(3)
    stfd 20, (6+26)*8(3)
    stfd 21, (7+26)*8(3)
    stfd 22, (8+26)*8(3)
    stfd 23, (9+26)*8(3)
    stfd 24, (10+26)*8(3)
    stfd 25, (11+26)*8(3)
    stfd 26, (12+26)*8(3)
    stfd 27, (13+26)*8(3)
    stfd 28, (14+26)*8(3)
    stfd 29, (15+26)*8(3)
    stfd 30, (16+26)*8(3)
    stfd 31, (17+26)*8(3)

    li  3, 0 // return
    blr

.globl SET
SET:
    // ld    13, (0+7)*8(3) not restoring 13 because I want thread migration
    ld 14, (1+7)*8(3) // callee-save GPRs
    ld 15, (2+7)*8(3) // xxx: block move
    ld 16, (3+7)*8(3)
    ld 17, (4+7)*8(3)
    ld 18, (5+7)*8(3)
    ld 19, (6+7)*8(3)
    ld 20, (7+7)*8(3)
    ld 21, (8+7)*8(3)
    ld 22, (9+7)*8(3)
    ld 23, (10+7)*8(3)
    ld 24, (11+7)*8(3)
    ld 25, (12+7)*8(3)
    ld 26, (13+7)*8(3)
    ld 27, (14+7)*8(3)
    ld 28, (15+7)*8(3)
    ld 29, (16+7)*8(3)
    ld 30, (17+7)*8(3)
    ld 31, (18+7)*8(3)

    lfd 14, (0+26)*8(3) // callee-save FPRs
    lfd 15, (1+26)*8(3)
    lfd 16, (2+26)*8(3)
    lfd 17, (3+26)*8(3)
    lfd 18, (4+26)*8(3)
    lfd 19, (5+26)*8(3)
    lfd 20, (6+26)*8(3)
    lfd 21, (7+26)*8(3)
    lfd 22, (8+26)*8(3)
    lfd 23, (9+26)*8(3)
    lfd 24, (10+26)*8(3)
    lfd 25, (11+26)*8(3)
    lfd 26, (12+26)*8(3)
    lfd 27, (13+26)*8(3)
    lfd 28, (14+26)*8(3)
    lfd 29, (15+26)*8(3)
    lfd 30, (16+26)*8(3)
    lfd 31, (17+26)*8(3)

    ld 1, 4*8(3)
    ld 2, 5*8(3)

    ld    0, 0*8(3)
    mtlr  0
    ld    0, 1*8(3)
    mtcr  0         // mtcrf 0xFF, 0
    ld    0, 2*8(3)
    mtctr 0
    ld    0, 3*8(3)
    mtxer 0

    ld    3,     6*8(3)
    blr

#elif QTHREAD_ASSEMBLY_ARCH == QTHREAD_ARMV8_A64
// Register Usage:
// X0 - holds pointer to context
// X1 - X30 - GP registers

.globl GET
.global GET
#ifndef __APPLE__
 .type GET, %function
#else
.p2align 4
#endif

GET:
    // Store 1 as X0-to-restore
    mov X1, #1
    mov X2, SP
    stp X1, X2, [X0]
    stp X29, X30, [X0,#16] // X30 is LR
    stp X19, X20, [X0,#32]
    stp X21, X22, [X0,#48]
    stp X23, X24, [X0,#64]
    stp X25, X26, [X0,#80]
    stp X27, X28, [X0,#96]

    // Since the context swap call signature doesn't use
    // SVE registers for passing arguments, this is
    // sufficient in that setting too.
    stp Q8, Q9, [X0,#112]
    stp Q10, Q11, [X0,#144]
    stp Q12, Q13, [X0,#176]
    stp Q14, Q15, [X0,#208]

    // return 0
    mov X0, #0
    ret

.globl SET
.global SET
#ifndef __APPLE__
 .type SET, %function
#else
.p2align 4
#endif
SET:
    ldp X1, X2, [X0]
    mov SP, X2
    ldp X29, X30, [X0,#16]
    ldp X19, X20, [X0,#32]
    ldp X21, X22, [X0,#48]
    ldp X23, X24, [X0,#64]
    ldp X25, X26, [X0,#80]
    ldp X27, X28, [X0,#96]
    ldp Q8, Q9, [X0,#112]
    ldp Q10, Q11, [X0,#144]
    ldp Q12, Q13, [X0,#176]
    ldp Q14, Q15, [X0,#208]

    mov X0, X1
    ret

#elif QTHREAD_ASSEMBLY_ARCH == QTHREAD_ARM
/* Register Usage:
 *
 * r0/a1       - Argument/result/scratch1
 * r1/a2       - Argument/result/scratch2
 * r2/a3       - Argument/scratch3
 * r3/a4       - Argument/scratch4
 * r4/v1       - Variable register (1)    CALLEE-SAVED
 * r5/v2       - Variable register (2)    CALLEE-SAVED
 * r6/v3       - Variable register (3)    CALLEE-SAVED
 * r7/v4       - Variable register (4)    CALLEE-SAVED
 * r8/v5       - Variable register (5)    CALLEE-SAVED
 * r9/v6/SB/TR - Platform regiter; implementation specific
 * r10/v7      - Variable register (7)    CALLEE-SAVED
 * r11/v8      - Variable register (8)    CALLEE-SAVED
 * r12/IP      - Intra-Procedure-call scratch register
 * r13/SP      - Stack Pointer            CALLEE-SAVED
 * r14/LR      - Link Register
 * r15/PC      - Program Counter
 */
.globl GET
.global GET
.type GET, %function
GET:
    str r1,  [r0,#4]   // ARG 2
    str r2,  [r0,#8]   // ARG 3
    str r3,  [r0,#12]  // ARG 4
    str r4,  [r0,#16]  // var 1
    str r5,  [r0,#20]  // var 2
    str r6,  [r0,#24]  // var 3
    str r7,  [r0,#28]  // var 4
    str r8,  [r0,#32]  // var 5
    str r9,  [r0,#36]  // ? SB/TR/v6 ?
    str r10, [r0,#40]  // var 7
    str r11, [r0,#44]  // var 8
    str r12, [r0,#48]  // IP
    str sp,  [r0,#52]  // SP
    str lr,  [r0,#56]  // LR
    mov r1,  #1        // store 1 as r0-to-restore
    str r1,  [r0]      // arg 1 / RET 1
    mov r0,  #0        // return 0 
    bx  lr

.globl SET
.global SET
.type SET, %function
SET:
    ldr r1,  [r0,#4]
    ldr r2,  [r0,#8]
    ldr r3,  [r0,#12]
    ldr r4,  [r0,#16]
    ldr r5,  [r0,#20]
    ldr r6,  [r0,#24]
    ldr r7,  [r0,#28]
    ldr r8,  [r0,#32]
    ldr r9,  [r0,#36]
    ldr r10, [r0,#40]
    ldr r11, [r0,#44]
    ldr r12, [r0,#48]
    ldr r13, [r0,#52]
    ldr r14, [r0,#56]
    ldr r0,  [r0]
    bx  lr

#elif QTHREAD_ASSEMBLY_ARCH == QTHREAD_RISCV
.globl GET
.global GET
.type GET, %function
GET:
    sd   s1,  8(a0)
    sd   s2,  16(a0)
    sd   s3,  24(a0)
    sd   s4,  32(a0)
    sd   s5,  40(a0)
    sd   s6,  48(a0)
    sd   s7,  56(a0)
    sd   s8,  64(a0)
    sd   s9,  72(a0)
    sd   s10, 80(a0)
    sd   s11, 88(a0)
    sd   s0,  96(a0)
    sd   ra,  104(a0)
    sd   sp,  112(a0)
    addi t0,  a0,     120
    fsd  f8,  0(t0)
    fsd  f9,  8(t0)
    fsd  f18, 16(t0)
    fsd  f19, 24(t0)
    fsd  f20, 32(t0)
    fsd  f21, 40(t0)
    fsd  f22, 48(t0)
    fsd  f23, 56(t0)
    fsd  f24, 64(t0)
    fsd  f25, 72(t0)
    fsd  f26, 80(t0)
    fsd  f27, 88(t0)
    li   t1,  1
    sd   t1,  0(a0)
    li   a0,  0
    ret

.globl SET
.global SET
.type SET, %function
SET:
    ld   s1,  8(a0)
    ld   s2,  16(a0)
    ld   s3,  24(a0)
    ld   s4,  32(a0)
    ld   s5,  40(a0)
    ld   s6,  48(a0)
    ld   s7,  56(a0)
    ld   s8,  64(a0)
    ld   s9,  72(a0)
    ld   s10, 80(a0)
    ld   s11, 88(a0)
    ld   s0,  96(a0)
    ld   ra,  104(a0)
    ld   sp,  112(a0)
    addi t0,  a0,     120
    fld  f8,  0(t0)
    fld  f9,  8(t0)
    fld  f18, 16(t0)
    fld  f19, 24(t0)
    fld  f20, 32(t0)
    fld  f21, 40(t0)
    fld  f22, 48(t0)
    fld  f23, 56(t0)
    fld  f24, 64(t0)
    fld  f25, 72(t0)
    fld  f26, 80(t0)
    fld  f27, 88(t0)
    ld   a0,  0(a0)
    ret

#else
#error Unknown architecture, no context swap available.
#endif

#if defined(__ELF__)
.section .note.GNU-stack,"",%progbits
#endif
