#include "qalloc.h"

#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>

int main()
{
    void *r, *r2;
    char teststring[16] = "This is a test.";
    char *ts, *ts2;
    off_t size = 4;

    size *= 1024;
    size *= 1024;
    printf("filesize requested: %" PRIuMAX "\n", (uintmax_t) size);
    r2 = qalloc_makemap(size, NULL, "test2.img", 12, 3);
    //r2 = qalloc_loadmap("test2.img");
    printf("r = %p\n", r2);
    r = qalloc_makemap(size, NULL, "test.img", 16, 3);
    printf("r = %p\n", r);
    ts = qalloc_malloc(r);
    ts2 = qalloc_malloc(r2);
    sprintf(ts, teststring);
    sprintf(ts2, "012345678901");
    qalloc_checkpoint();
    qalloc_free(ts, r);
    qalloc_free(ts2, r2);
    qalloc_cleanup();
    return 0;
}
