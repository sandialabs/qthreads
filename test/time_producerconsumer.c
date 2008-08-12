#include <stdio.h>		       /* for printf() */
#include <assert.h>		       /* for assert() */
#include <qthread/qthread.h>
#include "qtimer/qtimer.h"

#define ITERATIONS 1000000

aligned_t FEBbuffer = 7;
aligned_t FEBtable[2] = { 0 };

qtimer_t sending[2];
double total_sending_time = 0.0;
double total_roundtrip_time = 0.0;
double total_p1_sending_time = 0.0;
double total_p2_sending_time = 0.0;

aligned_t incrementme = 0;

aligned_t FEB_consumer(qthread_t * me, void *arg)
{
    aligned_t pong = 0;

    assert(qthread_readFE(me, &pong, &FEBbuffer) == 0);
    if (pong != 1) {
	printf("pong = %u\n", (unsigned)pong);
	assert(pong == 1);
    }
    return pong;
}

aligned_t FEB_producer(qthread_t * me, void *arg)
{
    aligned_t ping = 1;

    assert(qthread_writeEF(me, &FEBbuffer, &ping) == 0);
    return ping;
}

aligned_t FEB_producerloop(qthread_t * me, void *arg)
{
    aligned_t timer = 0;
    unsigned int i;

    for (i = 0; i < ITERATIONS; i++) {
	qtimer_start(sending[timer]);
	assert(qthread_writeEF(me, &FEBbuffer, &timer) == 0);
	timer = timer ? 0 : 1;
    }
    return 0;
}

aligned_t FEB_consumerloop(qthread_t * me, void *arg)
{
    aligned_t timer = 0;
    unsigned int i;

    for (i = 0; i < ITERATIONS; i++) {
	assert(qthread_readFE(me, &timer, &FEBbuffer) == 0);
	qtimer_stop(sending[timer]);
	total_sending_time += qtimer_secs(sending[timer]);
    }
    return 0;
}

aligned_t FEB_player2(qthread_t * me, void *arg)
{
    aligned_t paddle = 0;
    unsigned int i;

    for (i = 0; i < ITERATIONS; i++) {
	assert(qthread_readFE(me, &paddle, FEBtable) == 0);
	qtimer_stop(sending[0]);

	total_p1_sending_time += qtimer_secs(sending[0]);

	qtimer_start(sending[1]);
	assert(qthread_writeEF(me, FEBtable + 1, &paddle) == 0);
    }
    return 0;
}

aligned_t FEB_player1(qthread_t * me, void *arg)
{
    aligned_t paddle = 1;
    unsigned int i;
    qtimer_t roundtrip_timer = qtimer_new();

    /* serve */
    qtimer_start(sending[0]);
    qtimer_start(roundtrip_timer);
    assert(qthread_writeEF(me, FEBtable, &paddle) == 0);

    for (i = 0; i < ITERATIONS; i++) {
	assert(qthread_readFE(me, &paddle, FEBtable + 1) == 0);
	qtimer_stop(sending[1]);
	qtimer_stop(roundtrip_timer);

	total_roundtrip_time += qtimer_secs(roundtrip_timer);
	total_p2_sending_time += qtimer_secs(sending[1]);

	if (i + 1 < ITERATIONS) {
	    qtimer_start(sending[0]);
	    qtimer_start(roundtrip_timer);
	    assert(qthread_writeEF(me, FEBtable, &paddle) == 0);
	}
    }
    qtimer_free(roundtrip_timer);
    return 0;
}

aligned_t incrloop(qthread_t * me, void *arg)
{
    unsigned int i;

    for (i = 0; i < ITERATIONS; i++) {
	qthread_incr(&incrementme, 1);
    }
    return 0;
}

char *human_readable_rate(double rate)
{
    static char readable_string[100] = { 0 };
    const double GB = 1024 * 1024 * 1024;
    const double MB = 1024 * 1024;
    const double kB = 1024;

    if (rate > GB) {
	snprintf(readable_string, 100, "(%'.1f GB/s)", rate / GB);
    } else if (rate > MB) {
	snprintf(readable_string, 100, "(%'.1f MB/s)", rate / MB);
    } else if (rate > kB) {
	snprintf(readable_string, 100, "(%'.1f kB/s)", rate / kB);
    }
    return readable_string;
}

int main(int argc, char *argv[])
{
    aligned_t t;
    qtimer_t timer = qtimer_new();
    double rate;

    /* setup */
    qthread_init(16);
    qthread_empty(NULL, &FEBbuffer);
    qthread_empty(NULL, FEBtable);
    qthread_empty(NULL, FEBtable + 1);
    sending[0] = qtimer_new();
    sending[1] = qtimer_new();

    /* SINGLE FEB SEND/RECEIVE TEST */
    qtimer_start(timer);
    qthread_fork(FEB_consumer, NULL, &t);
    qthread_fork(FEB_producer, NULL, NULL);
    qthread_readFF(NULL, NULL, &t);
    qtimer_stop(timer);

    assert(qthread_feb_status(&FEBbuffer) == 0);

    printf("single FEB send/receive:    %11g secs\n", qtimer_secs(timer));
    rate = sizeof(aligned_t) / qtimer_secs(timer);
    printf(" = throughput:              %11g bytes/sec %s\n", rate,
	   human_readable_rate(rate));

    /* FEB PRODUCER/CONSUMER LOOP */
    qtimer_start(timer);
    qthread_fork(FEB_consumerloop, NULL, &t);
    qthread_fork(FEB_producerloop, NULL, NULL);
    qthread_readFF(NULL, NULL, &t);
    qtimer_stop(timer);

    printf("FEB producer/consumer loop: %11g secs\n", qtimer_secs(timer));
    printf(" - total sending time: %16g secs\n", total_sending_time);
    printf(" + external average time: %13g secs\n",
	   qtimer_secs(timer) / ITERATIONS);
    printf(" + internal average time: %13g secs\n",
	   total_sending_time / ITERATIONS);
    printf(" = message throughput: %16g msgs/sec\n",
	   ITERATIONS / total_sending_time);
    rate = (ITERATIONS * sizeof(aligned_t)) / total_sending_time;
    printf(" = data throughput: %19g bytes/sec %s\n", rate,
	   human_readable_rate(rate));

    assert(qthread_feb_status(&FEBbuffer) == 0);

    /* FEB PING-PONG LOOP */
    qtimer_start(timer);
    qthread_fork(FEB_player2, NULL, &t);
    qthread_fork(FEB_player1, NULL, NULL);
    qthread_readFF(NULL, NULL, &t);
    qtimer_stop(timer);

    printf("FEB ping-pong loop: %19g secs\n", qtimer_secs(timer));
    printf(" - total rtts: %24g secs\n", total_roundtrip_time);
    printf(" - total sending time: %16g secs\n",
	   total_p1_sending_time + total_p2_sending_time);
    printf(" + external avg rtt: %18g secs\n",
	   qtimer_secs(timer) / ITERATIONS);
    printf(" + internal avg rtt: %18g secs\n",
	   total_roundtrip_time / ITERATIONS);
    printf(" + average p1 sending time: %11g secs\n",
	   total_p1_sending_time / ITERATIONS);
    printf(" + average p2 sending time: %11g secs\n",
	   total_p2_sending_time / ITERATIONS);
    printf(" + average sending time: %14g secs\n",
	   (total_p1_sending_time +
	    total_p2_sending_time) / (ITERATIONS * 2));
    /* each rt is 2 messages, thus: */
    printf(" = messaging throughput: %14g msgs/sec\n",
	   (ITERATIONS * 2) / total_roundtrip_time);
    /* each rt is 1 message of aligned_t bytes each, thus: */
    rate = (ITERATIONS * sizeof(aligned_t)) / total_roundtrip_time;
    printf(" = data roundtrip tput: %15g bytes/sec %s\n", rate,
	   human_readable_rate(rate));
    /* each send is 1 messsage of aligned_t bytes, thus: */
    rate = (ITERATIONS * sizeof(aligned_t)) / total_p1_sending_time;
    printf(" = p1 hop throughput: %17g bytes/sec %s\n", rate,
	   human_readable_rate(rate));
    rate = (ITERATIONS * sizeof(aligned_t)) / total_p2_sending_time;
    printf(" = p2 hop throughput: %17g bytes/sec %s\n", rate,
	   human_readable_rate(rate));
    rate =
	(ITERATIONS * 2 * sizeof(aligned_t)) / (total_p1_sending_time +
						total_p2_sending_time);
    printf(" = data hop throughput: %15g bytes/sec %s\n", rate,
	   human_readable_rate(rate));

    assert(qthread_feb_status(FEBtable) == 0);
    assert(qthread_feb_status(FEBtable + 1) == 0);

    /* COMPETING INCREMENT LOOP */
    qtimer_start(timer);
    {
	unsigned int i;

	for (i = 0; i < 16; i++) {
	    qthread_fork(incrloop, NULL, &t);
	}
	for (i = 0; i < 16; i++) {
	    qthread_readFF(NULL, NULL, &t);
	}
    }
    qtimer_stop(timer);
    assert(incrementme == ITERATIONS*16);

    printf("competing increment loop: %13g secs\n", qtimer_secs(timer));
    printf(" + average increment time: %12g secs\n",
	   qtimer_secs(timer) / incrementme);
    printf(" + increment speed: %'19f increments/sec\n",
	   incrementme / qtimer_secs(timer));
    rate = (incrementme * sizeof(aligned_t)) / qtimer_secs(timer);
    printf(" = data throughput: %19g bytes/sec %s\n", rate,
	   human_readable_rate(rate));

    qthread_finalize();

    qtimer_free(timer);

    return 0;
}
