#ifndef FUTURELIB_INNARDS_H
#define FUTURELIB_INNARDS_H

typedef struct location_s location_t;

struct location_s
{
    aligned_t vp_count;
    pthread_mutex_t vp_count_lock;
    unsigned int vp_max;
    unsigned int id;
};

void blocking_vp_incr(qthread_t * me, location_t * loc);

#endif
