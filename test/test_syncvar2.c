#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <qthread/qthread.h>
#include "argparsing.h"

static syncvar_t *buff = NULL;

static uint64_t bufferSize = 1024;
static uint64_t numItems;

static aligned_t consumer(qthread_t * t, void *arg)
{
    for (unsigned int i=0; i<numItems; ++i) {
	const unsigned int buffInd = i % bufferSize;
	uint64_t buffVal;
	qthread_syncvar_readFE(t, &buffVal, &buff[buffInd]);
	iprintf("Consumer got: %u\n", (unsigned)buffVal);
    }

    return 0;
}

static aligned_t producer(qthread_t * t, void *arg)
{
    for (unsigned int i=0; i<numItems; ++i) {
	const unsigned int buffInd = i % bufferSize;
	qthread_syncvar_writeEF_const(t, &buff[buffInd], i);
	iprintf("producer wrote value #%u\n", i);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    aligned_t t[2];

    assert(qthread_initialize() == 0);

    CHECK_VERBOSE();
    NUMARG(bufferSize, "BUFFERSIZE");
    numItems = 8*bufferSize;
    NUMARG(numItems, "NUMITEMS");

    iprintf("%i threads...\n", qthread_num_shepherds());

    buff = malloc(sizeof(syncvar_t) * bufferSize);
    for (unsigned int i=0; i<bufferSize; ++i) {
	buff[i] = SYNCVAR_INITIALIZER;
    }

    qthread_fork(consumer, NULL, &t[0]);
    qthread_fork(producer, NULL, &t[1]);
    qthread_readFF(qthread_self(), NULL, &t[0]);
    qthread_readFF(qthread_self(), NULL, &t[1]);

    iprintf("Success!\n");

    free(buff);

    return 0;
}
