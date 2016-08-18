#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <qthread/qthread.h>
#include "argparsing.h"

// Test that a writeE on a full var performs the write, and changes the FEB
// state to empty.
static void testWriteEOnFull(void)
{
    aligned_t x, val;

    x = 45, val = 55;
    assert(qthread_feb_status(&x) == 1);

    iprintf("Before x=%d, val=%d, x_full=%d\n", x, val, qthread_feb_status(&x));
    qthread_writeE(&x, &val);
    iprintf("After  x=%d, val=%d, x_full=%d\n", x, val, qthread_feb_status(&x));

    assert(qthread_feb_status(&x) == 0);
    assert(x == 55);
    assert(val == 55);
}

// Test that a writeE on an empty var performs the write, and leaves the FEB
// state unchanged
static void testWriteEOnEmpty(void)
{
    aligned_t x, val;

    x = 45, val = 55;
    qthread_empty(&x);

    iprintf("Before x=%d, val=%d, x_full=%d\n", x, val, qthread_feb_status(&x));
    qthread_writeE(&x, &val);
    iprintf("After  x=%d, val=%d, x_full=%d\n", x, val, qthread_feb_status(&x));

    assert(qthread_feb_status(&x) == 0);
    assert(x == 55);
    assert(val == 55);
}

int main(int argc,
         char *argv[])
{
    CHECK_VERBOSE();
    assert(qthread_initialize() == 0);
    iprintf("%i shepherds...\n", qthread_num_shepherds());
    iprintf("  %i threads total\n", qthread_num_workers());

    testWriteEOnFull();
    testWriteEOnEmpty();
}

/* vim:set expandtab */
