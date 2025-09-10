/* Portions of this file are Copyright (c) 2025 Tactical Computing Labs, LLC; see COPYING */
#include "qthread/common.h"

#include <stddef.h>
#include <stdint.h>

#include "qt_visibility.h"

#define setcontext(u) qt_setmctxt(&(u)->mc)
#define getcontext(u) qt_getmctxt(&(u)->mc)
typedef struct mctxt mctxt_t;
typedef struct uctxt uctxt_t;

struct mctxt {
  /* Saved main processor registers. */
#ifdef NEEDARMA64CONTEXT
  uint64_t regs[32];     /* callee saves x0-x30, SP */
  uint64_t fpu_regs[32]; /* 32 64 bit FPU Registers */
#else
  uint32_t regs[16]; /* callee saves r0-r15 */
#endif
  char first;
};

struct uctxt {
  struct {
    void *ss_sp;
    size_t ss_size;
  } uc_stack;

  // sigset_t uc_sigmask;
  mctxt_t mc;
  struct uctxt *uc_link; /* unused */
};

int INTERNAL qt_swapctxt(uctxt_t *, uctxt_t *);
void INTERNAL qt_makectxt(uctxt_t *, void (*)(void), int, ...);
int INTERNAL qt_getmctxt(mctxt_t *);
void INTERNAL qt_setmctxt(mctxt_t *);
/* vim:set expandtab: */
