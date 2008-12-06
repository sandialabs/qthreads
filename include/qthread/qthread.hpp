#ifndef _QTHREAD_HPP_
#define _QTHREAD_HPP_

#define QTHREAD_HAVE_CXX_INTERFACE
#ifdef qthread_incr
#undef qthread_incr
#endif

#include <qthread/qthread.h>

template <bool> class OnlyTrue;
template <> class OnlyTrue<true> {};
#define QTHREAD_CHECKSIZE(X) sizeof(OnlyTrue<(bool)(sizeof(X) == sizeof(aligned_t))>)

template <typename T>
inline int qthraed_feb_status(const T* addr) 
{
    return qthread_feb_status((aligned_t*) addr);
}

template <typename T>
inline int qthread_empty(const T* dest)
{
    QTHREAD_CHECKSIZE(T);
    return qthread_empty(qthread_self(), (aligned_t*) dest);
}
template <typename T>
inline int qthread_empty(qthread_t *me, const T* dest)
{
    QTHREAD_CHECKSIZE(T);
    return qthread_empty(me, (aligned_t*) dest);
}

template <typename T>
inline int qthread_fill(const T* dest)
{
    QTHREAD_CHECKSIZE(T);
    return qthread_fill(qthread_self(), (aligned_t*) dest);
}
template <typename T>
inline int qthread_fill(qthread_t *me, const T* dest)
{
    QTHREAD_CHECKSIZE(T);
    return qthread_fill(me, (aligned_t*) dest);
}

template <typename T>
inline int qthread_writeEF(T * const dest, const T * const src)
{
    QTHREAD_CHECKSIZE(T);
    return qthread_writeEF(qthread_self(), (aligned_t*) dest, (aligned_t*) src);
}
template <typename T>
inline int qthread_writeEF(qthread_t *me, T * const dest, const T * const src)
{
    QTHREAD_CHECKSIZE(T);
    return qthread_writeEF(me, (aligned_t*) dest, (aligned_t*) src);
}
template <typename T>
inline int qthread_writeEF_const(T * const dest, const T src)
{
    QTHREAD_CHECKSIZE(T);
    return qthread_writeEF(qthread_self(), (aligned_t*) dest, (aligned_t) src);
}
template <typename T>
inline int qthread_writeEF_const(qthread_t *me, T * const dest, const T src)
{
    QTHREAD_CHECKSIZE(T);
    return qthread_writeEF(me, (aligned_t*) dest, (aligned_t) src);
}

template <typename T>
inline int qthread_writeF(T * const dest, const T * const src)
{
    QTHREAD_CHECKSIZE(T);
    return qthread_writeF(qthread_self(), (aligned_t*) dest, (aligned_t*) src);
}
template <typename T>
inline int qthread_writeF(qthread_t *me, T * const dest, const T * const src)
{
    QTHREAD_CHECKSIZE(T);
    return qthread_writeF(me, (aligned_t*) dest, (aligned_t*) src);
}
template <typename T>
inline int qthread_writeF_const(T * const dest, const T src)
{
    QTHREAD_CHECKSIZE(T);
    return qthread_writeF(qthread_self(), (aligned_t*) dest, (aligned_t) src);
}
template <typename T>
inline int qthread_writeF_const(qthread_t *me, T * const dest, const T src)
{
    QTHREAD_CHECKSIZE(T);
    return qthread_writeF(me, (aligned_t*) dest, (aligned_t) src);
}

template <typename T>
inline int qthread_readFF(T * const dest, const T * const src)
{
    QTHREAD_CHECKSIZE(T);
    return qthread_readFF(qthread_self(), (aligned_t*) dest, (aligned_t*) src);
}
template <typename T>
inline int qthread_readFF(qthread_t *me, T * const dest, const T * const src)
{
    QTHREAD_CHECKSIZE(T);
    return qthread_readFF(me, (aligned_t*) dest, (aligned_t*) src);
}

template <typename T>
inline int qthread_readFE(T * const dest, const T * const src)
{
    QTHREAD_CHECKSIZE(T);
    return qthread_readFE(qthread_self(), (aligned_t*) dest, (aligned_t*) src);
}
template <typename T>
inline int qthread_readFE(qthread_t *me, T * const dest, const T * const src)
{
    QTHREAD_CHECKSIZE(T);
    return qthread_readFE(me, (aligned_t*) dest, (aligned_t*) src);
}

template <typename T>
inline int qthread_lock(const T* a)
{
    QTHREAD_CHECKSIZE(T);
    return qthread_lock(qthread_self(), (aligned_t*) a);
}
template <typename T>
inline int qthread_lock(qthread_t *me, const T* a)
{
    QTHREAD_CHECKSIZE(T);
    return qthread_lock(me, (aligned_t*) a);
}

template <typename T>
inline int qthread_unlock(const T* a)
{
    QTHREAD_CHECKSIZE(T);
    return qthread_unlock(qthread_self(), (aligned_t*) a);
}
template <typename T>
inline int qthread_unlock(qthread_t *me, const T* a)
{
    QTHREAD_CHECKSIZE(T);
    return qthread_unlock(me, (aligned_t*) a);
}

inline float qthread_incr(volatile float *operand, const float incr)
{
    return qthread_fincr(operand, incr);
}
inline double qthread_incr(volatile double *operand, const double incr)
{
    return qthread_dincr(operand, incr);
}
// BWB: FIX ME: need to restrict T to integer values...
template <typename T, typename T2>
inline T qthread_incr(volatile T *operand, const T2 incr)
{
    switch (sizeof(T)) {
    case 4:
        return qthread_incr32((volatile uint32_t*) operand, incr);
    case 8:
        return qthread_incr64((volatile uint64_t*) operand, incr);
    default:
        *(int*)(0) = 0;
    }
    return T(0); // never hit - keep compiler happy
}

#endif // QTHREAD_HPP_
