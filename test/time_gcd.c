#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "qtimer.h"

size_t qarray_gcd(size_t a, size_t b)
{
    size_t k = 0;
    if (a == 0) return b;
    if (b == 0) return a;
    while ((a & 1) == 0 && (b&1) == 0) { /* while they're both even */
	a >>= 1; /* divide by 2 */
	b >>= 1; /* divide by 2 */
	k++; /* power of 2 to the result */
    }
    /* now, one or the other is odd */
    do {
	if ((a&1) == 0)
	    a >>= 1;
	else if ((b&1) == 0)
	    b >>= 1;
	else if (a >= b) /* both odd */
	    a = (a-b) >> 1;
	else
	    b = (b-a) >> 1;
    } while (a > 0);
    return b << k;
}

size_t old_gcd(size_t a, size_t b)
{
    while (1) {
	if (a == 0) return b;
	b %= a;
	if (b == 0) return a;
	a %= b;
    }
}

#define BIGNUM 1000000

int main ()
{
    struct pair {size_t a, b; };
    struct pair *bigset = malloc(sizeof(struct pair) * BIGNUM);
    size_t *answer1 = malloc(sizeof(size_t) * BIGNUM);
    size_t *answer2 = malloc(sizeof(size_t) * BIGNUM);
    size_t i;
    qtimer_t timer1 = qtimer_new();
    qtimer_t timer2 = qtimer_new();
    for (i=0;i<BIGNUM;i++) {
	bigset[i].a = random();
	bigset[i].b = 4096;
    }
    for (i=0;i<BIGNUM;i++) {
	answer1[i] = qarray_gcd(bigset[i].a, bigset[i].b);
    }
    qtimer_start(timer1);
    for (i=0;i<BIGNUM;i++) {
	answer1[i] = qarray_gcd(bigset[i].a, bigset[i].b);
    }
    qtimer_stop(timer1);
    for (i=0;i<BIGNUM;i++) {
	answer2[i] = old_gcd(bigset[i].a, bigset[i].b);
    }
    qtimer_start(timer2);
    for (i=0;i<BIGNUM;i++) {
	answer2[i] = old_gcd(bigset[i].a, bigset[i].b);
    }
    qtimer_stop(timer2);
    for (i=0;i<BIGNUM;i++) {
	if (answer1[i] != answer2[i]) {
	    printf("ERROR! %i\n", i);
	}
    }
    printf("new secs: %f\n", qtimer_secs(timer1));
    printf("old secs: %f\n", qtimer_secs(timer2));
    assert(qtimer_secs(timer1) > qtimer_secs(timer2));
    qtimer_free(timer1);
    qtimer_free(timer2);
    free(bigset);
    free(answer1);
    free(answer2);
}
