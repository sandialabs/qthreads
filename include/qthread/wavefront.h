#ifndef QTHREAD_WAVEFRONT_H
#define QTHREAD_WAVEFRONT_H

#include <qthread/qarray.h>

Q_STARTCXX			       /* */
typedef void (*wave_f) (const void *restrict left,
			const void *restrict leftdown,
			const void *restrict down, void *restrict out);

void qt_wavefront_config(qarray * restrict const *const R, size_t cols,
			 wave_f func, int feb);
#define qt_wavefront(R, cols, func) qt_wavefront_config((R), (cols), (func), 0)
#define qt_wavefront_feb(R, cols, func) qt_wavefront_config((R), (cols), (func), 1)

void qt_basic_wavefront(int *restrict const *constR, size_t cols, size_t rows,
			wave_f func);

Q_ENDCXX			       /* */
#endif
