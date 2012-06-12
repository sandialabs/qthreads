#ifndef SPR_H
#define SPR_H

#include <qthread/qthread.h>

Q_STARTCXX /* */

/* Flags */
#define SPR_SPMD (1 << 0) /* Launch in SPMD mode (much like MPI_Init()) */

/* Return Values */
#define SPR_OK      0  /* Success! */
#define SPR_BADARGS -1 /* One or more arguments was invalid. */
#define SPR_IGN     -2 /* The call was ignored. */
#define SPR_NOINIT  -3 /* SPR environment was not initialized. */

int spr_init(unsigned int flags,
             qthread_f   *regs);
int spr_fini(void);
int spr_unify(void);

int spr_num_locales(void);
int spr_locale_id(void);

Q_ENDCXX /* */

#endif // ifndef SPR_H
/* vim:set expandtab: */
