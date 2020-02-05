#include <stdio.h>
#include <stdlib.h>
#include <qthread/qloop.h>


unsigned int total = 0;

static void qtds_acc(void *a, const void *b){
    *(double*)a += *(double*)b;
    qthread_incr(&total,1);
}

static void qtds_worker(const size_t startat, 
                        const size_t stopat, 
                        void* array, 
                        void *ret){
    size_t i;
    double sum = 0; 
    for(i = startat; i < stopat; i++){
        sum += ((double*)array)[i];
    }
    *(double*)ret = sum;
    qthread_incr(&total,1);
}

double qt_double_sum_example(double *array, size_t length){
    double ret;
    qt_loopaccum_balance(0, 
                         length, 
                         sizeof(double), 
                         &ret, 
                         qtds_worker, 
                         array, 
                         qtds_acc);
    return ret;
}

int main(){
    qthread_initialize();
    size_t length = 100;
    double array[length]; 
    size_t i;
    for(i=0; i<length; i++){
        array[i] = rand();
    }
    double ret = qt_double_sum_example(array, length);
    printf("Sum = %f\nTotal = %d\n", ret,total+1);
    return 0;
}
