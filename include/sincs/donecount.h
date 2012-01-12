#ifndef QT_SINCS_DONECOUNT_H
#define QT_SINCS_DONECOUNT_H

typedef aligned_t qt_sinc_count_t;

typedef void (*qt_sinc_op_f)(void *tgt, void *src);

typedef struct qt_sinc_s {
    // Termination-detection-related info
    qt_sinc_count_t  counter;
    syncvar_t        ready;

    // Value-related info
    void            *restrict values;
    qt_sinc_op_f     op;
    void            *restrict result;
    void            *restrict initial_value;
    size_t           sizeof_value;
    size_t           sizeof_shep_value_part;
    size_t           sizeof_shep_count_part;
} qt_sinc_t;

#endif /* QT_SINCS_DONECOUNT_T */
