#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <qthread/qthread.h>
#include <qthread/qloop.h>
#include <qthread/qtimer.h>

#include "argparsing.h"

#define NUM_NEIGHBORS 9
#define NUM_STAGES 2
#define BOUNDARY 42

static int num_timesteps;

typedef struct stencil {
    size_t N;
    size_t M;
    aligned_t **stage[NUM_STAGES];
} stencil_t;

typedef struct update_args {
    stencil_t *points;
    size_t i;
    size_t stage;
} update_args_t;

typedef struct rows_args {
    stencil_t *points;
    size_t stage;
} rows_args_t;

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
static void update(const size_t start, const size_t stop, void *arg)
{
    stencil_t *points = ((update_args_t *)arg)->points;
    size_t i = ((update_args_t *)arg)->i;
    size_t j = start;
    size_t stage = ((update_args_t *)arg)->stage;

    size_t prev = prev_stage(stage);

    //fprintf(stderr, "\tStepping (%lu,%lu)\n", i, j);

    aligned_t sum = 0;
    for (size_t x = 0; x < 3; x++)
        for (size_t y = 0; y < 3; y++)
            sum += points->stage[prev][i-1+x][j-1+y];
    points->stage[stage][i][j] = sum/NUM_NEIGHBORS;
}

static void spawn_rows(const size_t start, const size_t stop, void *arg) {
    stencil_t *points = ((rows_args_t *)arg)->points;
    size_t stage = ((rows_args_t *)arg)->stage;

    //fprintf(stderr, "Spawning: %lu %lu\n", start, stage);
    update_args_t args = {points, start, stage};
    qt_loop(1, points->M-1, update, &args);
}

int main(int argc, char *argv[])
{
    int n = 10;
    int m = 10;
    num_timesteps = 10;
    int alltime = 0;

    CHECK_VERBOSE();
    NUMARG(alltime, "ALL_TIME");
    NUMARG(n, "N");
    NUMARG(m, "M");
    NUMARG(num_timesteps, "TIMESTEPS");

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

    points.stage[0] = malloc(points.N*sizeof(aligned_t *));
    assert(NULL != points.stage[0]);
    points.stage[1] = malloc(points.N*sizeof(aligned_t *));
    assert(NULL != points.stage[1]);

    for (int i = 0; i < points.N; i++) {
        points.stage[0][i] = calloc(points.M, sizeof(aligned_t));
        assert(NULL != points.stage[0][i]);
        points.stage[1][i] = calloc(points.M, sizeof(aligned_t));
        assert(NULL != points.stage[1][i]);
    }
    qtimer_stop(alloc_timer);

    // Initialize first stage and set boundary conditions
    qtimer_start(init_timer);
    for (int i = 1; i < points.N-1; i++) {
        for (int j = 1; j < points.M-1; j++) {
            points.stage[0][i][j] = 0;
        }
    }
    for (int i = 0; i < points.N; i++) {
        points.stage[0][i][0] = BOUNDARY;
        points.stage[0][i][points.M-1] = BOUNDARY;
        points.stage[1][i][0] = BOUNDARY;
        points.stage[1][i][points.M-1] = BOUNDARY;
    }
    for (int j = 0; j < points.M; j++) {
        points.stage[0][0][j] = BOUNDARY;
        points.stage[0][points.N-1][j] = BOUNDARY;
        points.stage[1][0][j] = BOUNDARY;
        points.stage[1][points.N-1][j] = BOUNDARY;
    }
    qtimer_stop(init_timer);

    // Spawn tasks to start calculating updates at each point
    qtimer_start(exec_timer);
    rows_args_t args = {&points, 1};
    for (int t = 1; t <= num_timesteps; t++) {
        //fprintf(stderr, "Starting step %d\n", t);
        qt_loop(1, points.N-1, spawn_rows, &args);
        //fprintf(stderr, "\t done step %d\n", t);
    }
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
        iprintf("\nStage %lu:\n", prev_stage(final));
        print_stage(&points, prev_stage(final));
        iprintf("\nStage %lu:\n", final);
        print_stage(&points, final);
    }

    qtimer_destroy(alloc_timer);
    qtimer_destroy(init_timer);
    qtimer_destroy(exec_timer);

    // Free allocated memory
    for (int i = 0; i < points.N; i++) {
        free(points.stage[0][i]);
        free(points.stage[1][i]);
    }
    free(points.stage[0]);
    free(points.stage[1]);

    return 0;
}

/* vim:set expandtab */
