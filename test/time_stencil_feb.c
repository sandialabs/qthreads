#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <qthread/qthread.h>
#include <qthread/qloop.h>
#include <qthread/feb_barrier.h>
#include <qthread/qtimer.h>

#include "argparsing.h"

#define NUM_NEIGHBORS 5
#define NUM_STAGES 3
#define BOUNDARY 42

static int num_timesteps;

typedef struct stencil {
    size_t N;
    size_t M;
    aligned_t **stage[NUM_STAGES];
    qt_feb_barrier_t *barrier;
} stencil_t;

typedef struct update_args {
    stencil_t *points;
    size_t i;
    size_t j;
    size_t stage;
    size_t step;
} update_args_t;

////////////////////////////////////////////////////////////////////////////////
static inline void print_stage(stencil_t *points, size_t stage)
{
    for (int i = 0; i < points->N; i++) {
        fprintf(stderr, "%lu", (unsigned long)points->stage[stage][i][0]);
        for (int j = 1; j < points->M; j++) {
            fprintf(stderr, "\t%lu", (unsigned long)points->stage[stage][i][j]);
        }
        fprintf(stderr, "\n");
    }
}

static inline size_t prev_stage(size_t stage)
{
    return (stage == 0) ? NUM_STAGES-1 : stage - 1;
}

static inline size_t next_stage(size_t stage)
{
    return (stage == NUM_STAGES-1) ? 0 : stage + 1;
}

////////////////////////////////////////////////////////////////////////////////
static aligned_t update(void *arg)
{
    stencil_t *points = ((update_args_t *)arg)->points;
    size_t i = ((update_args_t *)arg)->i;
    size_t j = ((update_args_t *)arg)->j;
    size_t stage = ((update_args_t *)arg)->stage;
    size_t step = ((update_args_t *)arg)->step;

    size_t prev = prev_stage(stage);
    size_t next = next_stage(stage);

    // Sum all neighboring values from previous stage
    aligned_t sum = 0;
    aligned_t *neighbor;
    neighbor = &points->stage[prev][i  ][j-1];
    qthread_readFF(neighbor, neighbor);
    sum += *neighbor;
    neighbor = &points->stage[prev][i-1][j  ];
    qthread_readFF(neighbor, neighbor);
    sum += *neighbor;
    neighbor = &points->stage[prev][i  ][j  ];
    qthread_readFF(neighbor, neighbor);
    sum += *neighbor;
    neighbor = &points->stage[prev][i+1][j  ];
    qthread_readFF(neighbor, neighbor);
    sum += *neighbor;
    neighbor = &points->stage[prev][i  ][j+1];
    qthread_readFF(neighbor, neighbor);
    sum += *neighbor;

    // Empty the next stage for this index
    qthread_empty(&points->stage[next][i][j]);

    // Update this point
    qthread_writeEF_const(&points->stage[stage][i][j], sum/NUM_NEIGHBORS);
    
    if (step < num_timesteps) {
        // Spawn next stage
        update_args_t args = {points, i, j, next, step+1};
        qthread_fork_syncvar_copyargs(update, &args, sizeof(update_args_t), NULL);
    }
    else
        qt_feb_barrier_enter(points->barrier);

    return 0;
}

int main(int argc, char *argv[])
{
    int n = 10;
    int m = 10;
    num_timesteps = 10;
    int alltime = 0;

    CHECK_VERBOSE();
    NUMARG(n,"N");
    NUMARG(m,"M");
    NUMARG(num_timesteps, "TIMESTEPS");
    NUMARG(alltime, "ALL_TIME");

    assert (n > 0 && m > 0);

    // Initialize Qthreads
    assert(qthread_initialize() == 0);

    qtimer_t alloc_timer = qtimer_create();
    qtimer_t init_timer = qtimer_create();
    qtimer_t exec_timer = qtimer_create();

    // Allocate memory for 3-stage stencil (with boundary padding)
    qtimer_start(alloc_timer);
    stencil_t points;
    points.N = n + 2;
    points.M = m + 2;

    points.stage[0] = calloc(points.N,sizeof(aligned_t *));
    assert(NULL != points.stage[0]);
    points.stage[1] = calloc(points.N,sizeof(aligned_t *));
    assert(NULL != points.stage[1]);
    points.stage[2] = calloc(points.N,sizeof(aligned_t *));
    assert(NULL != points.stage[2]);

    for (int i = 0; i < points.N; i++) {
        points.stage[0][i] = calloc(points.M, sizeof(aligned_t));
        assert(NULL != points.stage[0][i]);
        points.stage[1][i] = calloc(points.M, sizeof(aligned_t));
        assert(NULL != points.stage[1][i]);
        points.stage[2][i] = calloc(points.M, sizeof(aligned_t));
        assert(NULL != points.stage[2][i]);
    }
    qtimer_stop(alloc_timer);

    // Initialize first stage and set boundary conditions
    qtimer_start(init_timer);
    for (int i = 1; i < points.N-1; i++) {
        for (int j = 1; j < points.M-1; j++) {
            qthread_writeF_const(&points.stage[0][i][j], 0);
            qthread_empty(&points.stage[1][i][j]);
            qthread_empty(&points.stage[2][i][j]);
        }
    }
    for (int i = 0; i < points.N; i++) {
        qthread_writeF_const(&points.stage[0][i][0], BOUNDARY);
        qthread_writeF_const(&points.stage[0][i][points.M-1], BOUNDARY);
        qthread_writeF_const(&points.stage[1][i][0], BOUNDARY);
        qthread_writeF_const(&points.stage[1][i][points.M-1], BOUNDARY);
        qthread_writeF_const(&points.stage[2][i][0], BOUNDARY);
        qthread_writeF_const(&points.stage[2][i][points.M-1], BOUNDARY);
    }
    for (int j = 0; j < points.M; j++) {
        qthread_writeF_const(&points.stage[0][0][j], BOUNDARY);
        qthread_writeF_const(&points.stage[0][points.N-1][j], BOUNDARY);
        qthread_writeF_const(&points.stage[1][0][j], BOUNDARY);
        qthread_writeF_const(&points.stage[1][points.N-1][j], BOUNDARY);
        qthread_writeF_const(&points.stage[2][0][j], BOUNDARY);
        qthread_writeF_const(&points.stage[2][points.N-1][j], BOUNDARY);
    }
    qtimer_stop(init_timer);

    // Create barrier to synchronize on completion of calculations
    qtimer_start(exec_timer);
    points.barrier = qt_feb_barrier_create(n*m+1);

    // Spawn tasks to start calculating updates at each point
    update_args_t args = {&points, -1, -1, 1, 1};
    for (int i = 1; i < points.N-1; i++) {
        for (int j = 1; j < points.M-1; j++) {
            args.i = i;
            args.j = j;
            qthread_fork_syncvar_copyargs(update, &args, sizeof(update_args_t), NULL);
        }
    }

    // Wait for calculations to finish
    qt_feb_barrier_enter(points.barrier);

    // Stop timer
    qtimer_stop(exec_timer);

    // Print timing info
    if (alltime) {
        fprintf(stderr, "Allocation time: %f\n", qtimer_secs(alloc_timer));
        fprintf(stderr, "Initialization time: %f\n", qtimer_secs(init_timer));
        fprintf(stderr, "Execution time: %f\n", qtimer_secs(exec_timer));
    } else {
        fprintf(stdout, "%f\n", qtimer_secs(exec_timer));
    }


    // Print stencils
    if (verbose) {
        size_t final = (num_timesteps % NUM_STAGES);
        iprintf("Stage %lu:\n", prev_stage(prev_stage(final)));
        print_stage(&points, prev_stage(prev_stage(final)));
        iprintf("\nStage %lu:\n", prev_stage(final));
        print_stage(&points, prev_stage(final));
        iprintf("\nStage %lu:\n", final);
        print_stage(&points, final);
    }

    qt_feb_barrier_destroy(points.barrier);
    qtimer_destroy(alloc_timer);
    qtimer_destroy(init_timer);
    qtimer_destroy(exec_timer);

    // Free allocated memory
    for (int i = 0; i < points.N; i++) {
        free(points.stage[0][i]);
        free(points.stage[1][i]);
        free(points.stage[2][i]);
    }
    free(points.stage[0]);
    free(points.stage[1]);
    free(points.stage[2]);

    return 0;
}

/* vim:set expandtab */
