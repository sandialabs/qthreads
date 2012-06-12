#ifndef SPR_H
#define SPR_H

#include <qthread/qthread.h>

Q_STARTCXX /* */

/* Flags */
#define SPR_SPMD (1 << 0)

/* Return Values */
#define SPR_OK      0
#define SPR_BADARGS -1
#define SPR_IGN     -2

int spr_init(unsigned int flags,
             qthread_f   *regs);
int spr_fini(void);

int spr_num_locales(void);
int spr_locale_id(void);

Q_ENDCXX /* */

#endif // ifndef SPR_H
/* vim:set expandtab: */
