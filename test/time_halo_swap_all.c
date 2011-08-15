#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

#include <qthread/qthread.h>
#include <qthread/qloop.h>
#include <qthread/qtimer.h>

#include "argparsing.h"

#define NUM_NEIGHBORS 5
#define NUM_STAGES 2
#define BOUNDARY 42

static int num_timesteps;

////////////////////////////////////////////////////////////////////////////////
typedef enum position {
    NW=0, NORTH, NE, WEST, CENTER, EAST, SW, SOUTH, SE, 
    COL_NORTH, COL_SOUTH, COL_CENTER, 
    ROW_WEST, ROW_EAST, ROW_CENTER,
    NWES
} position_t;
static char *pos_strs[16] = {
    "NW", "NORTH", "NE", "WEST", "CENTER", "EAST", "SW", "SOUTH", "SE",
    "COL-NORTH", "COL-SOUTH", "COL-CENTER",
    "ROW-WEST", "ROW-EAST", "ROW-CENTER",
    "NWES"};

typedef struct partition {
    size_t row;
    size_t col;
    size_t nrows;
    size_t ncols;
    size_t brows;
    size_t bcols;
    position_t pos;
    aligned_t **stages[NUM_STAGES];
    aligned_t *ghosts;
} partition_t;

typedef struct stencil {
    size_t nrows;
    size_t ncols;
    size_t prows;
    size_t pcols;
    size_t brows;
    size_t bcols;
    partition_t **parts;
} stencil_t;

typedef struct update_args {
    stencil_t *points;
    size_t i;
    size_t stage;
} update_args_t;

////////////////////////////////////////////////////////////////////////////////
static inline void print_stencil(stencil_t *stencil)
{
    fprintf(stderr, "Stencil:\n");
    fprintf(stderr, "\tpoints:     %lu x %lu\n",stencil->nrows,stencil->ncols);
    fprintf(stderr, "\tpartitions: %lu x %lu\n",stencil->prows,stencil->pcols);

    const size_t num_parts = stencil->prows * stencil->pcols;
    for (int pi = 0; pi < num_parts; pi++) {
        const partition_t *part = stencil->parts[pi];
        fprintf(stderr, "\tPartition: (%lu,%lu) %s, %lu x %lu\n",
            part->row, part->col, pos_strs[part->pos], part->nrows,part->ncols);
        for (int i = part->nrows-1; i >= 0; i--) {
            fprintf(stderr,"\t\t%02lu",(unsigned long)part->stages[0][i][0]);
            for (int j = 1; j < part->ncols; j++) {
                fprintf(stderr," %02lu",(unsigned long)part->stages[0][i][j]);
            }
            fprintf(stderr, "\n");
        }
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

static inline void get_position(stencil_t *points, partition_t *part)
{
    const size_t i = part->row;
    const size_t j = part->col;
    const size_t prows = points->prows;
    const size_t pcols = points->pcols;

    if (points->prows == 1 && points->pcols == 1)
        part->pos = NWES;
    else if (points->prows == 1) {
        if (j == 0)
            part->pos = ROW_WEST;
        else if (j == pcols-1)
            part->pos = ROW_EAST;
        else
            part->pos = ROW_CENTER;
    } else if (points->pcols == 1) {
        if (i == 0)
            part->pos = COL_SOUTH;
        else if (i == prows-1)
            part->pos = COL_NORTH;
        else
            part->pos = CENTER;
    } else {
        if (i == 0)
            if (j == 0)
                part->pos = SW;
            else if (j == pcols-1)
                part->pos = SE;
            else
                part->pos = SOUTH;
        else if (i == prows-1)
            if (j == 0)
                part->pos = NW;
            else if (j == pcols-1)
                part->pos = NE;
            else
                part->pos = NORTH;
        else
            if (j == 0)
                part->pos = EAST;
            else if (j == pcols-1)
                part->pos = WEST;
            else
                part->pos = CENTER;
    }
}

// Logical-to-physical mapping assumes point (0,0) is in bottom left corner.
static inline void get_pid(size_t lid, size_t ncols, size_t *row, size_t *col)
{
    *row = lid / ncols;
    *col = lid - (*row * ncols);
}

////////////////////////////////////////////////////////////////////////////////
static void setup_stencil(const size_t start, const size_t stop, void *arg)
{
    const size_t part_lid = start;
    stencil_t *points = (stencil_t *)arg;

    points->parts[part_lid] = malloc(sizeof(partition_t));
    assert(points->parts[part_lid]);

    partition_t *part = points->parts[part_lid];
    part->nrows = points->nrows / points->prows;
    part->ncols = points->ncols / points->pcols;
    part->brows = points->brows;
    part->bcols = points->bcols;

    // Calculate position
    get_pid(part_lid, points->pcols, &part->row, &part->col);
    get_position(points, part);

    // Allocate points
    {
        part->nrows += 2;  // Padding
        part->ncols += 2;  // Padding

        part->stages[0] = malloc(part->nrows * sizeof(aligned_t *));
        assert(part->stages[0]);
        part->stages[1] = malloc(part->nrows * sizeof(aligned_t *));
        assert(part->stages[1]);
        for (int pi = 0; pi < part->nrows; pi++) {
            part->stages[0][pi] = malloc(part->ncols * sizeof(aligned_t));
            assert(part->stages[0][pi]);
            part->stages[1][pi] = malloc(part->ncols * sizeof(aligned_t));
            assert(part->stages[1][pi]);
            for (int pj = 0; pj < part->ncols; pj++) {
                part->stages[0][pi][pj] = 0;
                part->stages[1][pi][pj] = 0;
            }
        }
    }

    // Setup boundary
    {
        const position_t pos = part->pos;
        const size_t nrows = part->nrows;
        const size_t ncols = part->ncols;

        if (SW == pos || SOUTH == pos || SE == pos ||
            ROW_EAST == pos || ROW_CENTER == pos || ROW_WEST == pos ||
            COL_SOUTH == pos || NWES == pos)
            // Set southern boundary
            for (int j = 1; j < ncols-1; j++) {
                part->stages[0][0][j] = BOUNDARY;
                part->stages[1][0][j] = BOUNDARY;
            }
        if (SW == pos || WEST == pos || NW == pos ||
            COL_NORTH == pos || COL_CENTER == pos || COL_SOUTH == pos ||
            ROW_WEST == pos || NWES == pos)
            // Set western boundary
            for (int i = 1; i < nrows-1; i++) {
                part->stages[0][i][0] = BOUNDARY+1;
                part->stages[1][i][0] = BOUNDARY+1;
            }
        if (SE == pos || EAST == pos || NE == pos ||
            COL_NORTH == pos || COL_CENTER == pos || COL_SOUTH == pos ||
            ROW_EAST == pos || NWES == pos)
            // Set eastern boundary
            for (int i = 1; i < nrows-1; i++) {
                part->stages[0][i][ncols-1] = BOUNDARY+2;
                part->stages[1][i][ncols-1] = BOUNDARY+2;
            }
        if (NE == pos || NORTH == pos || NW == pos ||
            ROW_EAST == pos || ROW_CENTER == pos || ROW_WEST == pos ||
            COL_NORTH == pos || NWES == pos)
            // Set northern boundary
            for (int j = 1; j < ncols-1; j++) {
                part->stages[0][nrows-1][j] = BOUNDARY+3;
                part->stages[1][nrows-1][j] = BOUNDARY+3;
            }
    }

    // Setup ghostzones
    {
        const position_t pos = part->pos;
        const size_t num_rows = part->nrows;
        const size_t num_cols = part->ncols;
    
        if (NW == pos || NORTH == pos || NE == pos ||
            WEST == pos || CENTER == pos || EAST == pos ||
            COL_NORTH == pos || COL_CENTER == pos) {
            // Empty bottom row
            const size_t i = 0;
            for (int j = 1; j < num_cols-1; j++) {
                part->stages[0][i][j] = 33;
                part->stages[1][i][j] = 33;
                qthread_empty(&part->stages[0][i][j]);
                qthread_empty(&part->stages[1][i][j]);
            }
        }
        if (WEST == pos || CENTER == pos || EAST == pos ||
            SW == pos || SOUTH == pos || SE == pos ||
            COL_CENTER == pos || COL_SOUTH == pos) {
            // Empty top row
            const size_t i = num_rows - 1;
            for (int j = 1; j < num_cols-1; j++) {
                part->stages[0][i][j] = 33;
                part->stages[1][i][j] = 33;
                qthread_empty(&part->stages[0][i][j]);
                qthread_empty(&part->stages[1][i][j]);
            }
        }
        if (NORTH == pos || NE == pos ||
            CENTER == pos || EAST == pos ||
            SOUTH == pos || SE == pos ||
            ROW_CENTER == pos || ROW_EAST == pos) {
            // Empty west col 
            const size_t j = 0;
            for (int i = 1; i < num_rows-1; i++) {
                part->stages[0][i][j] = 33;
                part->stages[1][i][j] = 33;
                qthread_empty(&part->stages[0][i][j]);
                qthread_empty(&part->stages[1][i][j]);
            }
        }
        if (NW == pos || NORTH == pos ||
            WEST == pos || CENTER == pos ||
            SW == pos || SOUTH == pos ||
            ROW_WEST == pos || ROW_CENTER == pos) {
            // Empty east col
            const size_t j = num_cols - 1;
            for (int i = 1; i < num_rows-1; i++) {
                part->stages[0][i][j] = 33;
                part->stages[1][i][j] = 33;
                qthread_empty(&part->stages[0][i][j]);
                qthread_empty(&part->stages[1][i][j]);
            }
        }
    }
}

static inline void destroy_stencil(stencil_t *points)
{
    const size_t num_parts = points->prows * points->pcols;
    for (int i = 0; i < num_parts; i ++) {
        partition_t *part = points->parts[i];
        const size_t nrows = part->nrows;
        for (int pi = 0; pi < nrows; pi++) {
            free(part->stages[0][pi]);
            free(part->stages[1][pi]);
        }
        free(part->stages[0]);
        free(part->stages[1]);
        free(part);
    }
    free(points->parts);
}

////////////////////////////////////////////////////////////////////////////////
int main(int argc, char *argv[])
{
    int nrows = 10;
    int ncols = 10;
    int prows = 1;
    int pcols = 1;
    int brows = 1;
    int bcols = 1;
    num_timesteps = 10;
    int print_final = 0;
    int alltime = 0;

    CHECK_VERBOSE();
    NUMARG(nrows, "NROWS");
    NUMARG(ncols, "NCOLS");
    NUMARG(prows, "PROWS");
    NUMARG(pcols, "PCOLS");
    NUMARG(brows, "BROWS");
    NUMARG(bcols, "BCOLS");
    NUMARG(num_timesteps, "TIMESTEPS");
    NUMARG(print_final, "PRINT_FINAL");
    NUMARG(alltime, "ALL_TIME");

    // Check sanity
    assert(nrows > 0 && ncols > 0 && num_timesteps > 0);
    assert(nrows % prows == 0 && ncols % pcols == 0);
    assert((nrows/prows) % brows == 0 && (ncols/pcols) % bcols == 0);

    // Initialize Qthreads
    assert(qthread_initialize() == 0);

    qtimer_t setup_timer = qtimer_create();
    qtimer_t exec_timer = qtimer_create();

    // Setup stencil and partitions
    stencil_t points = {nrows, ncols, prows, pcols, brows, bcols, NULL};
    {
        qtimer_start(setup_timer);
        points.parts = malloc(prows * pcols * sizeof(partition_t*));
        assert(points.parts);

        qt_loop(0, prows*pcols, setup_stencil, &points);
        qtimer_stop(setup_timer);
    }

    {
        qtimer_start(exec_timer);
        // TODO: kick off work
        qtimer_stop(exec_timer);
    }

    // Print timing info
    if (alltime) {
        fprintf(stderr, "Setup time: %f\n", qtimer_secs(setup_timer));
        fprintf(stderr, "Execution time: %f\n", qtimer_secs(exec_timer));
    } else {
        fprintf(stdout, "%f\n", qtimer_secs(exec_timer));
    }
   
    if (print_final)
        print_stencil(&points);

    qtimer_destroy(setup_timer);
    qtimer_destroy(exec_timer);

    // Free allocated memory
    destroy_stencil(&points);

    return 0;
}

/* vim:set expandtab */
