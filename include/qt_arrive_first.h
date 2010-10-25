#ifndef QT_ARRIVEFIRST_H
#define QT_ARRIVEFIRST_H

#include "qthread.h"

struct qt_arrive_first_s {
    size_t activeSize; /* size of barrier */
    size_t allocatedSize; /* lowest size power of 2 equal or bigger than barrier -- for allocations */
    volatile int64_t **present; /* array of counters to track who's arrived */
    int doneLevel; /* height of the tree */
    char arriveFirstDebug; /* flag to turn on internal printf debugging */
};

typedef struct qt_arrive_first_s qt_arrive_first_t;

/* visible function calls */
int64_t qt_global_arrive_first(const qthread_shepherd_id_t shep);
void qt_global_arrive_first_destroy(void);
void qt_global_arrive_first_init(int size, int debug);

#define qt_arrive_first(x) qt_global_arrive_first(x)

#endif
