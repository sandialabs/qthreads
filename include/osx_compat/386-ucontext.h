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

/*extern*/ int swapcontext(ucontext_t *, ucontext_t *);
/*extern*/ void makecontext(ucontext_t *, MakeContextCallback *, int, ...);
/*extern*/ int getmcontext(mcontext_t *);
/*extern*/ void setmcontext(mcontext_t *);

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

/* #include <machine/ucontext.h> */

struct mcontext {
	/*
	 * The first 20 fields must match the definition of
	 * sigcontext. So that we can support sigcontext
	 * and ucontext_t at the same time.
	 */
	long mc_onstack;		/* XXX - sigcontext compat. */
	long mc_gs; /* processor-control in 64-bit Windows, unused elsewhere */
	long mc_fs; /* thread-specific data */
	long mc_es; /* flat segment group (do not touch) */
	long mc_ds; /* flat segment group (do not touch) */
	long mc_edi; /* general purpose 32-bit-only register */
	long mc_esi; /* general purpose 32-bit-only register */
	long mc_ebp; /* Stack frame pointer */
	long mc_isp;
	long mc_ebx; /* PIC base register, also general-purp. reg */
	long mc_edx; /* UNSAVED "dividend register", general purp. */
	long mc_ecx; /* UNSAVED "count register", general purp. */
	long mc_eax; /* UNSAVED "accumulation register", general purp. */
	long mc_trapno;
	long mc_err;
	long mc_eip; /* UNSAVED instruction pointer */
	long mc_cs; /* flat segment group (do not touch) */
	long mc_eflags;
	long mc_esp;			/* machine state; stack pointer */
	long mc_ss; /* flat segment group (do not touch) */

	long mc_fpregs[28];		/* env87 + fpacc87 + u_long */
	long __spare__[17];
};

struct ucontext {
	/*
	 * Keep the order of the first two fields. Also,
	 * keep them the first two fields in the structure.
	 * This way we can have a union with struct
	 * sigcontext and ucontext_t. This allows us to
	 * support them both at the same time.
	 * note: the union is not defined, though.
	 */
	sigset_t	uc_sigmask;
	mcontext_t	uc_mcontext;

	struct __ucontext *uc_link;
	stack_t		uc_stack;
	long		__spare__[8];
};


