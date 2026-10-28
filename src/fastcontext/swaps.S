// Portions of this file are Copyright (c) 2025 Tactical Computing Labs, LLC; see COPYING
// Portions of this file are Copyright (c) 2005-2006 Russ Cox, MIT; see COPYRIGHT
// Portions of this file are Copyright Sandia National Laboratories

#define QTHREAD_PREPROCESSING_ASM
#include "../../include/qthread/common.h"

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

// Make these blocks compatible with nasm too.
#ifdef __NASM_MAJOR__
#define ptr
#endif

#ifdef _MSC_VER
.code
PUBLIC SET
PUBLIC GET
#endif

#ifndef _MSC_VER
.intel_syntax noprefix
#endif

#ifdef _MSC_VER
SET PROC
#else
.globl SET
SET:
#endif
    // the argument is %rdi, so we do not need to load it from the stack */

    mov     rbp, [rdi+8]  // frame pointer
    mov     rbx, [rdi+16] // base pointer

    // extra callee-saved registers in AMD
    mov     r12, [rdi+24]
    mov     r13, [rdi+32]
    mov     r14, [rdi+40]
    mov     r15, [rdi+48]
    mov     rsp, [rdi+56]   // stack pointer
    push qword ptr [rdi+64] // push new pc onto stack for ret
    mov     rdi, [rdi]      // 1st int arg (arg passing); only necessary for first context swap into a new qthread

    mov     eax, 1 // return value (allows us to escape the swapcontext loop)
    ret
#ifdef _MSC_VER
SET ENDP
#endif


#ifdef _MSC_VER
GET PROC
#else
.globl GET
GET:
#endif
    mov     [rdi+8],  rbp // frame pointer
    mov     [rdi+16], rbx // base pointer

    // extra registers in AMD - callee saved
    mov     [rdi+24], r12 // in the __spare__ area
    mov     [rdi+32], r13
    mov     [rdi+40], r14
    mov     [rdi+48], r15

    // Idiom for saving rsp properly.
    lea     rcx, [rsp+8]
    mov     [rdi+56], rcx

    // the following is broken into two instructions
    // because we can only dereference one memory op at a time
    mov     rcx,      [rsp]
    mov     [rdi+64], rcx

    xor     eax, eax // set return value - success!
    ret
#ifdef _MSC_VER
GET ENDP
#endif

#ifdef _MSC_VER
END
#endif

#elif QTHREAD_ASSEMBLY_ARCH == QTHREAD_ARMV8_A64
// Register Usage:
// X0 - holds pointer to context
// X1 - X30 - GP registers

// clang-cl's preprocessor doesn't currently work
// well together with armasm64.
// Fall back to COFF format, which clang-cl can handle itself.
#if defined(_MSC_VER) && !defined(__clang__)
#define QTHREAD_USE_MARMASM
#endif

#ifdef QTHREAD_USE_MARMASM
    AREA context_swaps, CODE, READONLY
    EXPORT SET
    EXPORT GET
#endif

#if !defined(__APPLE__) && !(defined(_MSC_VER) && defined(__clang__))
#define QTHREAD_USE_TYPE_DIRECTIVES
#endif

#ifdef QTHREAD_USE_MARMASM
GET PROC
#else
.globl GET
.global GET
#ifdef QTHREAD_USE_TYPE_DIRECTIVES
 .type GET, %function
#else
.p2align 4
#endif
GET:
#endif
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
    stp D8, D9, [X0,#112]
    stp D10, D11, [X0,#128]
    stp D12, D13, [X0,#144]
    stp D14, D15, [X0,#160]

    // return 0
    mov X0, #0
    ret
#ifdef QTHREAD_USE_MARMASM
GET ENDP
#endif

#ifdef QTHREAD_USE_MARMASM
SET PROC
#else
.globl SET
.global SET
#ifdef QTHREAD_USE_TYPE_DIRECTIVES
 .type SET, %function
#else
.p2align 4
#endif
SET:
#endif
    ldp X1, X2, [X0]
    mov SP, X2
    ldp X29, X30, [X0,#16]
    ldp X19, X20, [X0,#32]
    ldp X21, X22, [X0,#48]
    ldp X23, X24, [X0,#64]
    ldp X25, X26, [X0,#80]
    ldp X27, X28, [X0,#96]
    ldp D8, D9, [X0,#112]
    ldp D10, D11, [X0,#128]
    ldp D12, D13, [X0,#144]
    ldp D14, D15, [X0,#160]

    mov X0, X1
    ret
#ifdef QTHREAD_USE_MARMASM
SET ENDP
#endif

#ifdef QTHREAD_USE_MARMASM
    END
#endif

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
