#include <stdlib.h>
#include <stdio.h>

#include <qthread/qthread.h>
#include <qthread/qutil.h>


int main(int argc, char *argv[])
{
    double array[3] = { 1.0, 2.0, 3.0 };
    volatile double sum;

    qthread_init(3);
    sum = qutil_double_FF_sum(NULL, array, 3);
    printf("sum: %lf\n", sum);
    qthread_finalize();
    return 0;
}
