#ifndef QTHREAD_ALLPAIRS_H
#define QTHREAD_ALLPAIRS_H

typedef void (*dist_f) (const void *unit1, const void *unit2, void *outstore);

void qt_allpairs(const void *array, const size_t count, const size_t unitsize,
		 void **output, const size_t outsize, const dist_f distfunc);

#endif
