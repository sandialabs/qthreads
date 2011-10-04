#ifndef QT_SINC_H
#define QT_SINC_H

typedef long qt_sinc_count_t;

typedef void (*qt_sinc_op_f)(void *tgt, void *src);

typedef struct qt_sinc_s {
    void         *values;
    void         *counts;
    qt_sinc_op_f  op;
    syncvar_t     ready;
    void         *result;
    void         *initial_value;
    size_t        sizeof_value;
    size_t        sizeof_count;
    size_t        sizeof_shep_value_part;
    size_t        sizeof_shep_count_part;
} qt_sinc_t;

qt_sinc_t *qt_sinc_create(const size_t  sizeof_value, 
                          const void   *initial_value,
                          qt_sinc_op_f  op,
                          const size_t  will_spawn);
void       qt_sinc_reset(qt_sinc_t *sinc,
                         const size_t will_spawn);
void       qt_sinc_destroy(qt_sinc_t *sinc);
void       qt_sinc_willspawn(qt_sinc_t *sinc, 
                             size_t     count);
void       qt_sinc_submit(qt_sinc_t *sinc, 
                          void      *value);
void       qt_sinc_wait(qt_sinc_t *sinc, 
                        void      *target);

#endif
