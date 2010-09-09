#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#ifdef HAVE_STDARG_H
# include <stdarg.h>
#endif
#ifdef HAVE_SYS_UCONTEXT_H
# include <sys/ucontext.h>
#endif

#define setcontext(u) setmcontext(&(u)->uc_mcontext)
#define getcontext(u) getmcontext(&(u)->uc_mcontext)
typedef struct mcontext mcontext_t;
typedef struct ucontext ucontext_t;

typedef void (MakeContextCallback)(void);

int swapcontext(ucontext_t *, ucontext_t *);
void makecontext(ucontext_t *, MakeContextCallback *, int, ...);
int getmcontext(mcontext_t *);
void setmcontext(mcontext_t *);

/*-
 * Copyright (c) 1999 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/sys/ucontext.h,v 1.4 1999/10/11 20:33:17 luoqi Exp $
 */

struct mcontext {
	long mc_edi; /* 0: 1st arg (mandatory) */
	long mc_esi; /* 1: 2nd arg (often mandatory) */
#ifdef NEEDX86REGISTERARGS
	long mc_edx; /* 2: 3rd arg */
	long mc_ecx; /* 3: 4th arg */
#endif
	long mc_ebp; /* 4/2: Stack frame pointer */
	long mc_ebx; /* 5/3: PIC base register, also general-purp. reg */
	long mc_eax; /* 6/4: return value */
#ifdef __x86_64__
	long mc_r12; /* 7: extra callee-saved registers */
	long mc_r13; /* 8: extra callee-saved registers */
	long mc_r14; /* 9: extra callee-saved registers */
	long mc_r15; /* 10: extra callee-saved registers */
#endif
	long mc_esp; /* 11/5: machine state; stack pointer */
	long mc_eip; /* 12/6: function pointer */
};

struct ucontext {
	mcontext_t	uc_mcontext;
	stack_t		uc_stack;
};


