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

// Initial settings of point values (for debugging)
#define INTERNAL       0
#define GHOST_POINT 0
#define EDGE_POINT 0

#define NORTH_OF(stage, i, j) stage[i+1][j  ]
#define WEST_OF(stage, i, j)  stage[i  ][j-1]
#define EAST_OF(stage, i, j)  stage[i  ][j+1]
#define SOUTH_OF(stage, i, j) stage[i-1][j  ]

#define GHOST_NORTH(pos) (WEST == pos || CENTER == pos || EAST == pos || \
                          SW == pos || SOUTH == pos || SE == pos || \
                          COL_CENTER == pos || COL_SOUTH == pos)
#define GHOST_WEST(pos) (NORTH == pos || NE == pos || CENTER == pos || \
                         EAST == pos || SOUTH == pos || SE == pos || \
                         ROW_CENTER == pos || ROW_EAST == pos)
#define GHOST_EAST(pos) (NW == pos || NORTH == pos || WEST == pos || \
                         CENTER == pos || SW == pos || SOUTH == pos || \
                         ROW_WEST == pos || ROW_CENTER == pos)
#define GHOST_SOUTH(pos) (NW == pos || NORTH == pos || NE == pos || \
                          WEST == pos || CENTER == pos || EAST == pos || \
                          COL_NORTH == pos || COL_CENTER == pos)

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

typedef struct partition_s {
    position_t pos;                 // Position of this partition in matrix
    size_t row;                     // Row index in partition matrix
    size_t col;                     // Col index in partition matrix
    size_t nrows;                   // Number of rows in this partition
    size_t ncols;                   // Number of cols in this partition
    size_t brows;                   // Number of row blocks
    size_t bcols;                   // Number of col blocks
    aligned_t **stages[NUM_STAGES]; // The local stencil points
} partition_t;

typedef struct stencil_s {
    size_t nrows;         // Number of rows in stencil
    size_t ncols;         // Number of cols in stencil
    size_t prows;         // Number of rows in partition matrix
    size_t pcols;         // Number of cols in partition matrix
    size_t brows;         // Total number of row blocks
    size_t bcols;         // Total number of col blocks
    partition_t **parts;  // The stencil partitions
} stencil_t;

////////////////////////////////////////////////////////////////////////////////
static inline void print_stencil(stencil_t *stencil, size_t step)
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
            fprintf(stderr,"\t\t%02lu",(unsigned long)part->stages[step][i][0]);
            for (int j = 1; j < part->ncols; j++) {
                fprintf(stderr," %02lu",(unsigned long)part->stages[step][i][j]);
            }
            fprintf(stderr, "\n");
        }
    }
}

static inline size_t prev_stage(size_t stage)
{ /*{{{*/
    return (stage == 0) ? NUM_STAGES-1 : stage - 1;
} /*}}}*/

static inline size_t next_stage(size_t stage)
{ /*{{{*/
    return (stage == NUM_STAGES-1) ? 0 : stage + 1;
} /*}}}*/

// Find the position of the partition in the partition matrix
static inline void get_position(stencil_t *points, partition_t *part)
{ /*{{{*/
    const size_t i = part->row;
    const size_t j = part->col;
    const size_t prows = points->prows;
    const size_t pcols = points->pcols;

    if (prows > 1 && pcols > 1) {
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
                part->pos = WEST;
            else if (j == pcols-1)
                part->pos = EAST;
            else
                part->pos = CENTER;
    } else if (prows == 1 && pcols > 1) {
        if (j == 0)
            part->pos = ROW_WEST;
        else if (j == pcols-1)
            part->pos = ROW_EAST;
        else
            part->pos = ROW_CENTER;
    } else if (pcols == 1 && prows > 1) {
        if (i == 0)
            part->pos = COL_SOUTH;
        else if (i == prows-1)
            part->pos = COL_NORTH;
        else
            part->pos = COL_CENTER;
    } else {
        part->pos = NWES;
    }
} /*}}}*/

// Logical-to-physical mapping assumes point (0,0) is in bottom left corner.
static inline void get_pid(size_t lid, size_t ncols, size_t *row, size_t *col)
{/*{{{*/
    *row = lid / ncols;
    *col = lid - (*row * ncols);
}/*}}}*/

static inline size_t get_lid(size_t row, size_t col, size_t ncols)
{/*{{{*/
    return (row * ncols) + col;
}/*}}}*/

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
        part->nrows += 2;  // Padding for boundary and ghost points
        part->ncols += 2;  // Padding for boundary and ghost points

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
                part->stages[0][pi][pj] = INTERNAL;
                part->stages[1][pi][pj] = INTERNAL;
            }
        }
    }

    // Setup boundary
    {
        const position_t pos = part->pos;
        const size_t nrows = part->nrows;
        const size_t ncols = part->ncols;

        if (!GHOST_SOUTH(pos))
            for (int j = 1; j < ncols-1; j++) {
                part->stages[0][0][j] = BOUNDARY;
                part->stages[1][0][j] = BOUNDARY;
            }
        if (!GHOST_WEST(pos))
            for (int i = 1; i < nrows-1; i++) {
                part->stages[0][i][0] = BOUNDARY;
                part->stages[1][i][0] = BOUNDARY;
            }
        if (!GHOST_EAST(pos))
            for (int i = 1; i < nrows-1; i++) {
                part->stages[0][i][ncols-1] = BOUNDARY;
                part->stages[1][i][ncols-1] = BOUNDARY;
            }
        if (!GHOST_NORTH(pos))
            for (int j = 1; j < ncols-1; j++) {
                part->stages[0][nrows-1][j] = BOUNDARY;
                part->stages[1][nrows-1][j] = BOUNDARY;
            }
    }

    // Setup ghostzones and edges:
    // 1) empty ghost points for both stages
    // 2) empty edge points only in second stage
    {
        const position_t pos = part->pos;
        const size_t num_rows = part->nrows;
        const size_t num_cols = part->ncols;
    
        if (GHOST_SOUTH(pos)) {
            const size_t i = 0;
            for (int j = 1; j < num_cols-1; j++) {
                // Ghost points
                part->stages[0][i][j] = GHOST_POINT;
                part->stages[1][i][j] = GHOST_POINT;
                qthread_empty(&part->stages[0][i][j]);
                qthread_empty(&part->stages[1][i][j]);
                // Edge points
                part->stages[0][i+1][j] = EDGE_POINT;
                part->stages[1][i+1][j] = EDGE_POINT;
                qthread_empty(&part->stages[1][i+1][j]);
            }
        }
        if (GHOST_NORTH(pos)) {
            const size_t i = num_rows - 1;
            for (int j = 1; j < num_cols-1; j++) {
                // Ghost points
                part->stages[0][i][j] = GHOST_POINT;
                part->stages[1][i][j] = GHOST_POINT;
                qthread_empty(&part->stages[0][i][j]);
                qthread_empty(&part->stages[1][i][j]);
                // Edge points
                part->stages[0][i-1][j] = EDGE_POINT;
                part->stages[1][i-1][j] = EDGE_POINT;
                qthread_empty(&part->stages[1][i-1][j]);
            }
        }
        if (GHOST_WEST(pos)) {
            const size_t j = 0;
            for (int i = 1; i < num_rows-1; i++) {
                // Ghost points
                part->stages[0][i][j] = GHOST_POINT;
                part->stages[1][i][j] = GHOST_POINT;
                qthread_empty(&part->stages[0][i][j]);
                qthread_empty(&part->stages[1][i][j]);
                // Edge points
                part->stages[0][i][j+1] = EDGE_POINT;
                part->stages[1][i][j+1] = EDGE_POINT;
                qthread_empty(&part->stages[1][i][j+1]);
            }
        }
        if (GHOST_EAST(pos)) {
            const size_t j = num_cols - 1;
            for (int i = 1; i < num_rows-1; i++) {
                // Ghost points
                part->stages[0][i][j] = GHOST_POINT;
                part->stages[1][i][j] = GHOST_POINT;
                qthread_empty(&part->stages[0][i][j]);
                qthread_empty(&part->stages[1][i][j]);
                // Edge points
                part->stages[0][i][j-1] = EDGE_POINT;
                part->stages[1][i][j-1] = EDGE_POINT;
                qthread_empty(&part->stages[1][i][j-1]);
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
typedef struct up_args_s {
    aligned_t ***stages;
    size_t       now;
    size_t       i;
    size_t       j;
    size_t       k;       // Upper bound of row
} up_args_t;

typedef struct upil_args_s {
    size_t num_rows;
    size_t num_cols;
    position_t pos;
    up_args_t *up_args;
} upil_args_t;

typedef struct us_args_s {
    size_t part_lid;
    size_t timestep;
    partition_t *part;
    const stencil_t *points;
} us_args_t;

typedef struct su_args_s {
    size_t part_lid;
    size_t stage;
    const stencil_t *points;
} su_args_t;

// Update internal point
static void update_point_internal(const size_t start, const size_t stop,
                                  void *arg_)
{
    const up_args_t *arg = (up_args_t *)arg_;
    aligned_t ***stages = arg->stages;
    const size_t now    = arg->now;
    const size_t i      = start;
    const size_t j      = arg->j;

    aligned_t **S = stages[prev_stage(now)];

    const aligned_t sum = S[i][j] + NORTH_OF(S,i,j) + SOUTH_OF(S,i,j) + 
                          EAST_OF(S,i,j) + WEST_OF(S,i,j);

    stages[now][i][j] = sum/NUM_NEIGHBORS;
}

// Spawn internal point tasks over columns
static void update_point_internal_loop(const size_t start, const size_t stop,
                                       void *arg_)
{
    const upil_args_t *arg = (upil_args_t *)arg_;
    const size_t num_rows = arg->num_rows;
    const position_t pos  = arg->pos;

    const size_t col_start = GHOST_SOUTH(pos) ? 2 : 1;
    const size_t col_stop  = GHOST_NORTH(pos) ? num_rows-2 : num_rows-1;

    up_args_t up_args = {arg->up_args->stages, arg->up_args->now, 0, start, 0};
    qt_loop(col_start, col_stop, update_point_internal, &up_args);
}

// Update NW corner point
static aligned_t update_point_nw(void *arg_)
{
    const up_args_t *arg = (up_args_t *)arg_;
    aligned_t ***stages = arg->stages;
    const size_t now    = arg->now;
    const size_t i      = arg->i;
    const size_t j      = arg->j;

    aligned_t **S = stages[prev_stage(now)];
    aligned_t value = 0;
    aligned_t sum = S[i][j];

    // Read and empty the ghost point
    qthread_readFE(&value, &NORTH_OF(S,i,j));
    sum += value;
    qthread_readFE(&value, &WEST_OF(S,i,j));
    sum += value;
    sum += EAST_OF(S,i,j) + SOUTH_OF(S,i,j);

    qthread_writeEF_const(&stages[now][i][j], sum/NUM_NEIGHBORS);

    return 0;
}

// Update NE corner point
static aligned_t update_point_ne(void *arg_)
{
    const up_args_t *arg = (up_args_t *)arg_;
    aligned_t ***stages = arg->stages;
    const size_t now    = arg->now;
    const size_t i      = arg->i;
    const size_t j      = arg->j;

    aligned_t **S = stages[prev_stage(now)];
    aligned_t value = 0;
    aligned_t sum = S[i][j];

    qthread_readFE(&value, &NORTH_OF(S,i,j));
    sum += value;
    qthread_readFE(&value, &EAST_OF(S,i,j));
    sum += value;
    sum += WEST_OF(S,i,j) + SOUTH_OF(S,i,j);

    qthread_writeEF_const(&stages[now][i][j], sum/NUM_NEIGHBORS);

    return 0;
}

// Update SW corner point
static aligned_t update_point_sw(void *arg_)
{
    const up_args_t *arg = (up_args_t *)arg_;
    aligned_t ***stages = arg->stages;
    const size_t now    = arg->now;
    const size_t i      = arg->i;
    const size_t j      = arg->j;

    aligned_t **S = stages[prev_stage(now)];
    aligned_t value = 0;
    aligned_t sum = S[i][j];

    qthread_readFE(&value, &SOUTH_OF(S,i,j));
    sum += value;
    qthread_readFE(&value, &WEST_OF(S,i,j));
    sum += value;
    sum += EAST_OF(S,i,j) + NORTH_OF(S,i,j);

    qthread_writeEF_const(&stages[now][i][j], sum/NUM_NEIGHBORS);

    return 0;
}

// Update SE corner point
static aligned_t update_point_se(void *arg_)
{
    const up_args_t *arg = (up_args_t *)arg_;
    aligned_t ***stages = arg->stages;
    const size_t now    = arg->now;
    const size_t i      = arg->i;
    const size_t j      = arg->j;

    aligned_t **S = stages[prev_stage(now)];
    aligned_t value = 0;
    aligned_t sum = S[i][j];

    qthread_readFE(&value, &SOUTH_OF(S,i,j));
    sum += value;
    qthread_readFE(&value, &EAST_OF(S,i,j));
    sum += value;
    sum += WEST_OF(S,i,j) + NORTH_OF(S,i,j);

    qthread_writeEF_const(&stages[now][i][j], sum/NUM_NEIGHBORS);

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
static void update_point_n(const size_t start, const size_t stop, void *arg_)
{
    const up_args_t *arg = (up_args_t *)arg_;
    aligned_t ***stages = arg->stages;
    const size_t now    = arg->now;
    const size_t i      = arg->i;
    const size_t j      = start;

    aligned_t **S = stages[prev_stage(now)];

    aligned_t sum = 0;
    qthread_readFE(&sum, &NORTH_OF(S,i,j));
    sum += S[i][j] + WEST_OF(S,i,j) + EAST_OF(S,i,j) + SOUTH_OF(S,i,j);

    qthread_writeEF_const(&stages[now][i][j], sum/NUM_NEIGHBORS);
}

static void update_point_s(const size_t start, const size_t stop, void *arg_)
{
    const up_args_t *arg = (up_args_t *)arg_;
    aligned_t ***stages = arg->stages;
    const size_t now    = arg->now;
    const size_t i      = arg->i;
    const size_t j      = start;

    aligned_t **S = stages[prev_stage(now)];

    aligned_t sum = 0;
    qthread_readFE(&sum, &SOUTH_OF(S,i,j));
    sum += S[i][j] + WEST_OF(S,i,j) + EAST_OF(S,i,j) + NORTH_OF(S,i,j);

    qthread_writeEF_const(&stages[now][i][j], sum/NUM_NEIGHBORS);
}

static void update_point_w(const size_t start, const size_t stop, void *arg_)
{
    const up_args_t *arg = (up_args_t *)arg_;
    aligned_t ***stages = arg->stages;
    const size_t now    = arg->now;
    const size_t i      = start;
    const size_t j      = arg->j;

    aligned_t **S = stages[prev_stage(now)];

    aligned_t sum = 0;
    qthread_readFE(&sum, &WEST_OF(S,i,j));
    sum += S[i][j] + NORTH_OF(S,i,j) + EAST_OF(S,i,j) + SOUTH_OF(S,i,j);

    qthread_writeEF_const(&stages[now][i][j], sum/NUM_NEIGHBORS);
}

static void update_point_e(const size_t start, const size_t stop, void *arg_)
{
    const up_args_t *arg = (up_args_t *)arg_;
    aligned_t ***stages = arg->stages;
    const size_t now    = arg->now;
    const size_t i      = start;
    const size_t j      = arg->j;

    aligned_t **S = stages[prev_stage(now)];

    aligned_t sum = 0;
    qthread_readFE(&sum, &EAST_OF(S,i,j));
    sum += S[i][j] + NORTH_OF(S,i,j) + WEST_OF(S,i,j) + SOUTH_OF(S,i,j);

    qthread_writeEF_const(&stages[now][i][j], sum/NUM_NEIGHBORS);
}

// Spawn tasks along edges
static aligned_t update_point_edge_n(void *arg_)
{
    up_args_t *arg = (up_args_t *)arg_;

    qt_loop(arg->j, arg->k, update_point_n, arg);

    return 0;
}

static aligned_t update_point_edge_s(void *arg_)
{
    up_args_t *arg = (up_args_t *)arg_;

    qt_loop(arg->j, arg->k, update_point_s, arg);

    return 0;
}

static aligned_t update_point_edge_w(void *arg_)
{
    up_args_t *arg = (up_args_t *)arg_;

    qt_loop(arg->i, arg->k, update_point_w, arg);

    return 0;
}

static aligned_t update_point_edge_e(void *arg_)
{
    up_args_t *arg = (up_args_t *)arg_;

    qt_loop(arg->i, arg->k, update_point_e, arg);

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
typedef struct sb_args_s {
    aligned_t *const target;
    const aligned_t *const source;
} sb_args_t;

typedef struct sbc_args_s {
    aligned_t *const target1;
    aligned_t *const target2;
    const aligned_t *const source;
} sbc_args_t;

/*
 * Pre: source is filled
 * Post: target is filled
 * 1) empty source
 * 2) fill target
 */
static aligned_t send_block(void *arg_) {
    const sb_args_t *arg = (sb_args_t *)arg_;

    aligned_t value;
    qthread_readFE(&value, arg->source);
    qthread_writeEF(arg->target, &value);

    return 0;
}

// Corner case is needed so we don't readFE() the same source twice
static aligned_t send_block_corner(void *arg_) {
    const sbc_args_t *arg = (sbc_args_t *)arg_;

    aligned_t value;
    qthread_readFE(&value, arg->source);
    qthread_writeEF(arg->target1, &value);
    qthread_writeEF(arg->target2, &value);

    return 0;
}

static aligned_t send_updates(void *arg_)
{
    const su_args_t *arg = (su_args_t *)arg_;
    const size_t src_lid    = arg->part_lid;
    const size_t stage      = arg->stage;
    const stencil_t *points = arg->points;

    const partition_t *src_part  = points->parts[src_lid];
    const position_t   src_pos   = src_part->pos;
    aligned_t  **const src_stage = src_part->stages[stage];
    const size_t       src_nrows = src_part->nrows;
    const size_t       src_ncols = src_part->ncols;

    if (GHOST_NORTH(src_pos)) {
        const size_t tgt_lid = 
            get_lid(src_part->row + 1, src_part->col, points->pcols);
        const partition_t *tgt_part   = points->parts[tgt_lid];
        aligned_t **const  tgt_stage  = tgt_part->stages[stage];

        const size_t i = src_nrows - 2;
        const size_t lb = GHOST_WEST(src_pos) ? 2 : 1;
        const size_t ub = GHOST_EAST(src_pos) ? src_ncols-2 : src_ncols-1;
        for (size_t j = lb; j < ub; j++) {
            const sb_args_t sb_args = {&tgt_stage[0][j], &src_stage[i][j]};
            qthread_fork_copyargs_precond(send_block, &sb_args, 
                sizeof(sb_args_t), NULL, 1, &src_stage[i][j]);
        }
    }
    if (GHOST_SOUTH(src_pos)) {
        const size_t tgt_lid = 
            get_lid(src_part->row - 1, src_part->col, points->pcols);
        const partition_t *tgt_part   = points->parts[tgt_lid];
        aligned_t  **const tgt_stage  = tgt_part->stages[stage];
        const size_t       tgt_nrows  = tgt_part->nrows;

        const size_t i = 1;
        const size_t lb = GHOST_WEST(src_pos) ? 2 : 1;
        const size_t ub = GHOST_EAST(src_pos) ? src_ncols-2 : src_ncols-1;
        for (size_t j = lb; j < ub; j++) {
            const sb_args_t sb_args = 
                {&tgt_stage[tgt_nrows-1][j], &src_stage[i][j]};
            qthread_fork_copyargs_precond(send_block, &sb_args, 
                sizeof(sb_args_t), NULL, 1, &src_stage[i][j]);
        }
    }
    if (GHOST_WEST(src_pos)) {
        const size_t tgt_lid = 
            get_lid(src_part->row, src_part->col-1, points->pcols);
        const partition_t *tgt_part   = points->parts[tgt_lid];
        aligned_t  **const tgt_stage  = tgt_part->stages[stage];
        const size_t       tgt_ncols  = tgt_part->ncols;

        const size_t j = 1;
        const size_t lb = GHOST_SOUTH(src_pos) ? 2 : 1;
        const size_t ub = GHOST_NORTH(src_pos) ? src_nrows-2 : src_nrows-1;
        for (size_t i = lb; i < ub; i++) {
            const sb_args_t sb_args = 
                {&tgt_stage[i][tgt_ncols-1], &src_stage[i][j]};
            qthread_fork_copyargs_precond(send_block, &sb_args, 
                sizeof(sb_args_t), NULL, 1, &src_stage[i][j]);
        }
    }

    if (GHOST_EAST(src_pos)) {
        const size_t tgt_lid = 
            get_lid(src_part->row, src_part->col+1, points->pcols);
        const partition_t *tgt_part   = points->parts[tgt_lid];
        aligned_t  **const tgt_stage  = tgt_part->stages[stage];

        const size_t j = src_ncols-2;
        const size_t lb = GHOST_SOUTH(src_pos) ? 2 : 1;
        const size_t ub = GHOST_NORTH(src_pos) ? src_nrows-2 : src_nrows-1;
        for (size_t i = lb; i < ub; i++) {
            const sb_args_t sb_args = 
                {&tgt_stage[i][0], &src_stage[i][j]};
            qthread_fork_copyargs_precond(send_block, &sb_args, 
                sizeof(sb_args_t), NULL, 1, &src_stage[i][j]);
        }
    }

    if (GHOST_WEST(src_pos) && GHOST_NORTH(src_pos)) {
        const size_t tgt1_lid = 
            get_lid(src_part->row, src_part->col-1, points->pcols);
        const partition_t *tgt1_part   = points->parts[tgt1_lid];
        aligned_t  **const tgt1_stage  = tgt1_part->stages[stage];
        const size_t       tgt1_ncols  = tgt1_part->ncols;

        const size_t tgt2_lid = 
            get_lid(src_part->row + 1, src_part->col, points->pcols);
        const partition_t *tgt2_part   = points->parts[tgt2_lid];
        aligned_t **const  tgt2_stage  = tgt2_part->stages[stage];

        const size_t i = src_nrows-2;
        const size_t j = 1;
        const sbc_args_t sbc_args = 
            {&tgt1_stage[i][tgt1_ncols-1], 
             &tgt2_stage[0][j],
             &src_stage[i][j]};
        qthread_fork_copyargs_precond(send_block_corner, &sbc_args, 
            sizeof(sbc_args_t), NULL, 1, &src_stage[i][j]);
    }

    if (GHOST_WEST(src_pos) && GHOST_SOUTH(src_pos)) {
        const size_t tgt1_lid = 
            get_lid(src_part->row, src_part->col-1, points->pcols);
        const partition_t *tgt1_part   = points->parts[tgt1_lid];
        aligned_t  **const tgt1_stage  = tgt1_part->stages[stage];
        const size_t       tgt1_ncols  = tgt1_part->ncols;

        const size_t tgt2_lid = 
            get_lid(src_part->row - 1, src_part->col, points->pcols);
        const partition_t *tgt2_part   = points->parts[tgt2_lid];
        aligned_t **const  tgt2_stage  = tgt2_part->stages[stage];
        const size_t       tgt2_nrows  = tgt2_part->nrows;

        const size_t i = 1;
        const size_t j = 1;
        const sbc_args_t sbc_args = 
            {&tgt1_stage[i][tgt1_ncols-1], 
             &tgt2_stage[tgt2_nrows-1][j],
             &src_stage[i][j]};
        qthread_fork_copyargs_precond(send_block_corner, &sbc_args, 
            sizeof(sbc_args_t), NULL, 1, &src_stage[i][j]);
    }

    if (GHOST_EAST(src_pos) && GHOST_NORTH(src_pos)) {
        const size_t tgt1_lid = 
            get_lid(src_part->row, src_part->col+1, points->pcols);
        const partition_t *tgt1_part   = points->parts[tgt1_lid];
        aligned_t  **const tgt1_stage  = tgt1_part->stages[stage];

        const size_t tgt2_lid = 
            get_lid(src_part->row + 1, src_part->col, points->pcols);
        const partition_t *tgt2_part   = points->parts[tgt2_lid];
        aligned_t  **const tgt2_stage  = tgt2_part->stages[stage];

        const size_t i = src_nrows-2;
        const size_t j = src_ncols-2;
        const sbc_args_t sbc_args = 
            {&tgt1_stage[i][0], 
             &tgt2_stage[0][j],
             &src_stage[i][j]};
        qthread_fork_copyargs_precond(send_block_corner, &sbc_args, 
            sizeof(sbc_args_t), NULL, 1, &src_stage[i][j]);
    }

    if (GHOST_EAST(src_pos) && GHOST_SOUTH(src_pos)) {
        const size_t tgt1_lid = 
            get_lid(src_part->row, src_part->col+1, points->pcols);
        const partition_t *tgt1_part   = points->parts[tgt1_lid];
        aligned_t  **const tgt1_stage  = tgt1_part->stages[stage];

        const size_t tgt2_lid = 
            get_lid(src_part->row - 1, src_part->col, points->pcols);
        const partition_t *tgt2_part   = points->parts[tgt2_lid];
        aligned_t  **const tgt2_stage  = tgt2_part->stages[stage];
        const size_t       tgt2_nrows  = tgt2_part->nrows;

        const size_t j = src_ncols-2;
        const size_t i = 1;
        const sbc_args_t sbc_args = 
            {&tgt1_stage[i][0], 
             &tgt2_stage[tgt2_nrows-1][j], 
             &src_stage[i][j]};
        qthread_fork_copyargs_precond(send_block_corner, &sbc_args, 
            sizeof(sbc_args_t), NULL, 1, &src_stage[i][j]);
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
/*
* - tasks for updating all points are spawned when called
* - this task does not return until all 
*/
static aligned_t update_stage(void *arg_)
{
    const us_args_t *arg = (us_args_t *)arg_;
    const position_t pos = arg->part->pos;
    const size_t num_rows = arg->part->nrows;
    const size_t num_cols = arg->part->ncols;
    const size_t now      = arg->timestep % 2;

    aligned_t rets[8];
    up_args_t up_args = {arg->part->stages, now, 0, 0, 0};
    const size_t args_size = sizeof(up_args_t);

    // Process corner cases
    {
        if (GHOST_NORTH(pos) && GHOST_WEST(pos)) {
            up_args.i = num_rows - 2;
            up_args.j = 1;
            qthread_fork_copyargs(update_point_nw,&up_args,args_size,&rets[0]);
        }
        if (GHOST_NORTH(pos) && GHOST_EAST(pos)) { 
            up_args.i = num_rows - 2;
            up_args.j = num_cols - 2;;
            qthread_fork_copyargs(update_point_ne,&up_args,args_size,&rets[1]);
        }
        if (GHOST_SOUTH(pos) && GHOST_WEST(pos)) {
            up_args.i = 1;
            up_args.j = 1;
            qthread_fork_copyargs(update_point_sw,&up_args,args_size,&rets[2]);
        }
        if (GHOST_SOUTH(pos) && GHOST_EAST(pos)) {
            up_args.i = 1;
            up_args.j = num_cols - 2;
            qthread_fork_copyargs(update_point_se,&up_args,args_size,&rets[3]);
        }
    }

    // Process edge cases
    {
        if (GHOST_NORTH(pos)) {
            up_args.i = num_rows-2;
            up_args.j = GHOST_WEST(pos) ? 2 : 1;
            up_args.k = GHOST_EAST(pos) ? num_cols-2 : num_cols-1;
            qthread_fork_copyargs(update_point_edge_n, &up_args, args_size, 
                                  &rets[4]);
        }
        if (GHOST_SOUTH(pos)) {
            up_args.i = 1;
            up_args.j = GHOST_WEST(pos) ? 2 : 1;
            up_args.k = GHOST_EAST(pos) ? num_cols-2 : num_cols-1;
            qthread_fork_copyargs(update_point_edge_s, &up_args, args_size,
                                  &rets[5]);
        }
        if (GHOST_WEST(pos)) {
            up_args.i = GHOST_SOUTH(pos) ? 2 : 1;
            up_args.j = 1;
            up_args.k = GHOST_NORTH(pos) ? num_rows-2 : num_rows-1;
            qthread_fork_copyargs(update_point_edge_w, &up_args, args_size,
                                  &rets[6]);
        }
        if (GHOST_EAST(pos)) {
            up_args.i = GHOST_SOUTH(pos) ? 2 : 1;
            up_args.j = num_cols-2;
            up_args.k = GHOST_NORTH(pos) ? num_rows-2 : num_rows-1;
            qthread_fork_copyargs(update_point_edge_e, &up_args, args_size,
                                  &rets[7]);
        }
    }

    // Process internal points
    {
        upil_args_t upil_args = {num_rows, num_cols, pos, &up_args};

        const size_t start = GHOST_WEST(pos) ? 2 : 1;
        const size_t stop  = GHOST_EAST(pos) ? num_cols-2 : num_cols-1;
        qt_loop(start, stop, update_point_internal_loop, &upil_args);
    }

    // Wait for corner and edge cases to finish
    for (int i = 0; i < 8; i++)
        qthread_readFF(&rets[i], &rets[i]);

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
/*
 *  -   Starts off local processing for a partition
 */
static void begin(const size_t start, const size_t stop, void *arg_)
{
    const stencil_t *arg = (stencil_t *)arg_;

    const size_t part_lid = start;

    for (size_t t = 1; t <= num_timesteps; t++) {
        // Send outgoing values to neighboring ghost cells
        const su_args_t su_args = {part_lid, (t-1)%2, arg};
        qthread_fork(send_updates, &su_args, NULL);

        // Compute a step
        const us_args_t us_args = {part_lid, t, arg->parts[part_lid], arg};
        aligned_t up_ret;
        qthread_fork(update_stage, &us_args, &up_ret);
        qthread_readFF(&up_ret, &up_ret);
    }
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

    const size_t num_parts = prows * pcols;

    // Check sanity
    assert(nrows > 0 && ncols > 0 && num_timesteps > 0);
    assert(nrows % prows == 0 && ncols % pcols == 0);
    assert((nrows/prows) % brows == 0 && (ncols/pcols) % bcols == 0);
    assert(nrows/prows > 1 && ncols/pcols > 1);

    // Initialize Qthreads
    assert(qthread_initialize() == 0);

    qtimer_t setup_timer = qtimer_create();
    qtimer_t exec_timer = qtimer_create();

    // Setup stencil and partitions
    stencil_t points = {nrows, ncols, prows, pcols, brows, bcols, NULL};
    {
        qtimer_start(setup_timer);
        points.parts = malloc(num_parts * sizeof(partition_t*));
        assert(points.parts);

        qt_loop(0, num_parts, setup_stencil, &points);
        qtimer_stop(setup_timer);
    }

    // Start off computation on each partition
    {
        qtimer_start(exec_timer);
        qt_loop(0, num_parts, begin, &points);
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
        print_stencil(&points, num_timesteps % 2);

    qtimer_destroy(setup_timer);
    qtimer_destroy(exec_timer);

    // Free allocated memory
    destroy_stencil(&points);

    return 0;
}

/* vim:set expandtab */
