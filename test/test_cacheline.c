#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <qthread/qthread.h>
#include <qthread/cacheline.h>

int main()
{
	int cacheline = 0;

    qthread_init(0);
	cacheline = qthread_cacheline();
	printf("%i bytes\n", cacheline);
	assert(cacheline > 0);
    return 0;
}
