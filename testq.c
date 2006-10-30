#include "qalloc.h"

#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>

int main()
{
    void *r, *r2;
    char teststring[16] = "This is a test.";
    char *ts, *ts2;
    off_t size = 4;

    size *= 1024;
    size *= 1024;
    printf("filesize requested: %" PRIuMAX "\n", (uintmax_t) size);
    /* making maps */
    r2 = qalloc_makedynmap(size, NULL, "test2.img", 3);
    /*r2 = qalloc_loadmap("test2.img");*/
    printf("r = %p\n", r2);
    r = qalloc_makestatmap(size, NULL, "test.img", sizeof(teststring), 3);
    printf("r = %p\n", r);

    ts = qalloc_statmalloc(r);
    printf("qalloc_malloc: %p\n", ts);
    ts2 = qalloc_dynmalloc(r2, strlen("012345678901")+1);
    printf("qalloc_dynmalloc: %p\n", ts2);
    sprintf(ts, teststring);
    sprintf(ts2, "012345678901");
    qalloc_checkpoint();
    printf("qalloc_checkpoint\n");
    qalloc_free(ts, r);
    printf("qalloc_free\n");
    qalloc_free(ts2, r2);
    printf("qalloc_dynfree\n");
    ts2 = qalloc_malloc(r2, 128);
    memset(ts2, 0x55, 128);
    qalloc_free(ts2, r2);
    qalloc_cleanup();
    printf("qalloc_cleanup\n");
    return 0;
}
