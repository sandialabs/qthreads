#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <qthread/qthread.h>
#include "argparsing.h"

typedef struct {
    int64_t u:60;
} int60_t;
#define INT64TOINT60(x) ((uint64_t)((x)&0xfffffffffffffffUL))
#define INT60TOINT64(x) ((int64_t)(((x)&0x800000000000000UL)?((x)|0xf800000000000000UL):(x)))

/* More or less, this code matches the prodCons.chpl code from the open-source
 * Chapel compiler. This is a demonstration generated in an attempt to figure
 * out whether a given race condition is in qthreads on in Chapel */

static uint64_t bufferSize = 1024;	/* size of the circular buffer */
static uint64_t numItems;	/* number of items to write to buffer */

/*
 * Circular buffer of synchronization variables, which store
 * full/empty state in addition to their numeric values. By default, reads to
 * these variables will block until they are "full", leaving them "empty".
 * Writes will block until "empty", leaving "full".
 */
static syncvar_t *buff = NULL;

/*
 * the producer loops over the requested number of items, writing them to the
 * buffer starting at location 0 and wrapping around when it hits the end of
 * the buffer. It then writes the value -1 as a sentinel to the next position.
 */
static aligned_t producer(
    qthread_t * t,
    void *arg)
{
    for (unsigned int i = 0; i < numItems; ++i) {
	const unsigned int buffInd = i % bufferSize;
	qthread_syncvar_writeEF_const(t, &buff[buffInd], i);
	iprintf("producer wrote value #%u\n", i);
    }
    qthread_syncvar_writeEF_const(t, &buff[numItems % bufferSize], INT64TOINT60(-1));

    return 0;
}

static int64_t readFromBuff(qthread_t *t) {
    static unsigned int ind = 0;
    uint64_t readVal;
    int64_t nextVal;

    qthread_syncvar_readFE(t, &readVal, &buff[ind]);
    nextVal = INT60TOINT64(readVal);
    if (nextVal != -1) {
	ind = (ind + 1) % bufferSize;
    }
    return nextVal;
}

static aligned_t consumer(
    qthread_t * t,
    void *arg)
{
    int64_t buffVal;
    while ((buffVal = readFromBuff(t)) != -1) {
	iprintf("Consumer got: %li\n", (long)buffVal);
    }

    return 0;
}

int main(
    int argc,
    char *argv[])
{
    aligned_t t[2];

    assert(qthread_initialize() == 0);

    CHECK_VERBOSE();
    NUMARG(bufferSize, "BUFFERSIZE");
    numItems = 8 * bufferSize;
    NUMARG(numItems, "NUMITEMS");

    iprintf("%i threads...\n", qthread_num_shepherds());

    buff = malloc(sizeof(syncvar_t) * bufferSize);
    for (unsigned int i = 0; i < bufferSize; ++i) {
	buff[i] = SYNCVAR_EMPTY_INITIALIZER;
    }

    qthread_fork(consumer, NULL, &t[0]);
    qthread_fork(producer, NULL, &t[1]);
    qthread_readFF(qthread_self(), NULL, &t[0]);
    qthread_readFF(qthread_self(), NULL, &t[1]);

    iprintf("Success!\n");

    free(buff);

    return 0;
}
