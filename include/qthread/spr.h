#ifndef SPR_H
#define SPR_H

#include <qthread.h>

Q_STARTCXX /* */

#define SPR_SPMD (1<<0)

int spr_init(unsigned int flags, qthread_f *regs);

Q_ENDCXX /* */

#endif
