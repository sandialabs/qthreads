#ifndef QT_SINC_H
#define QT_SINC_H

typedef saligned_t qt_sinc_count_t;

typedef void (*qt_sinc_op_f)(void *tgt, void *src);

typedef struct qt_sinc_s {
    void            *restrict values;
    qt_sinc_count_t *restrict counts;
    qt_sinc_op_f     op;
    syncvar_t        ready;
    void            *restrict result;
    void            *restrict initial_value;
    aligned_t        remaining;
    size_t           sizeof_value;
    size_t           sizeof_shep_value_part;
    size_t           sizeof_shep_count_part;
#if defined(SINCS_PROFILE)
    void        *count_incrs;
#endif /* defined(SINCS_PROFILE) */
} qt_sinc_t;

qt_sinc_t *qt_sinc_create(const size_t sizeof_value,
                          const void  *initial_value,
                          qt_sinc_op_f op,
                          const size_t will_spawn);
void qt_sinc_reset(qt_sinc_t   *sinc,
                   const size_t will_spawn);
void qt_sinc_destroy(qt_sinc_t *sinc);
void qt_sinc_willspawn(qt_sinc_t *sinc,
                       size_t     count);
void *qt_sinc_tmpdata(qt_sinc_t *sinc);
void  qt_sinc_submit(qt_sinc_t *sinc,
                     void      *value);
void qt_sinc_wait(qt_sinc_t *sinc,
                  void      *target);

#endif // ifndef QT_SINC_H
/* vim:set expandtab: */
