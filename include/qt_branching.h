#ifndef QT_BRANCHING_H
#define QT_BRANCHING_H

#ifndef _MSC_VER
#define likely(x) (__builtin_expect(!!(x), 1))
#define unlikely(x) (__builtin_expect(!!(x), 0))
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif

#endif
