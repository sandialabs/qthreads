#ifndef QT_ARRIVEFIRST_H
#define QT_ARRIVEFIRST_H

#include "qthread/qthread.h"

struct qt_arrive_first_s {
    size_t activeSize;		/* size of barrier */
    size_t allocatedSize;	/* lowest size power of 2 equal or bigger than barrier -- for allocations */
    int64_t doneLevel;		/* height of the tree */
    int64_t nestLock;           /* backup for nested situations */
    char arriveFirstDebug;	/* flag to turn on internal printf debugging */
    volatile int64_t **present;	/* array of counters to track who's arrived */
};

typedef struct qt_arrive_first_s qt_arrive_first_t;

/* visable function calls */
int64_t qt_global_arrive_first(const qthread_shepherd_id_t shep, int64_t nest);
void qt_global_arrive_first_destroy(void);
void qt_global_arrive_first_init(int size, int debug);

#define qt_arrive_first(x) qt_global_arrive_first(x)

#endif
