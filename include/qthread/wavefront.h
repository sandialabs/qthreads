#ifndef QTHREAD_WAVEFRONT_H
#define QTHREAD_WAVEFRONT_H

#include <qthread/qarray.h>

Q_STARTCXX			       /* */
typedef void (*wave_f) (const void *restrict left,
			const void *restrict leftdown,
			const void *restrict down, void *restrict out);

void qt_wavefront(qarray * restrict const *const R, size_t cols, wave_f func);

void qt_basic_wavefront(int *restrict const *const R, size_t cols,
			size_t rows, wave_f func);

Q_ENDCXX			       /* */
#endif
