#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

#include <qthread/qthread.h>
#include <qthread/qloop.h>
#include <qthread/qtimer.h>

#include "argparsing.h"

// Configuration constants
/*{{{*/
#define NUM_NEIGHBORS 5
#define NUM_STAGES 2
#define BOUNDARY 42
/*}}}*/

static int num_timesteps;

// Initial settings of point values and statuses
static const syncvar_t boundary_value = 
    { .u.s = { .data = BOUNDARY, .state = 0, .lock = 0 } }; // Full
#define INTERNAL_POINT    SYNCVAR_INITIALIZER
#define GHOST_POINT_EMPTY SYNCVAR_EMPTY_INITIALIZER
#define GHOST_POINT_FULL  SYNCVAR_INITIALIZER
#define EDGE_POINT_EMPTY  SYNCVAR_EMPTY_INITIALIZER
#define EDGE_POINT_FULL   SYNCVAR_INITIALIZER

// Stencil and partition and position data structures
/*{{{*/
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
    Q_ALIGNED(8) position_t pos;    // Position of this partition in matrix
    size_t row;                     // Row index in partition matrix
    size_t col;                     // Col index in partition matrix
    size_t nrows;                   // Number of rows in this partition
    size_t ncols;                   // Number of cols in this partition
    size_t brows;                   // Number of row blocks
    size_t bcols;                   // Number of col blocks
    syncvar_t **stages[NUM_STAGES]; // The local stencil points
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
/*}}}*/

////////////////////////////////////////////////////////////////////////////////
// Helpers

// Position-related helper stuff
/*{{{*/
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

#define HAS_NW_CORNER(pos) (CENTER == pos || \
                           EAST == pos || SOUTH == pos || SE == pos)
#define HAS_SW_CORNER(pos) (CENTER == pos || \
                           NORTH == pos || NE == pos || EAST == pos)
#define HAS_NE_CORNER(pos) (CENTER == pos || \
                           WEST == pos || SW == pos || SOUTH == pos)
#define HAS_SE_CORNER(pos) (CENTER == pos || \
                           NW == pos || NORTH == pos || WEST == pos)
/*}}}*/

// Syncvar-related stuff
#define SYNCVAR_EVAL(x) INT60TOINT64(x.u.s.data)
#define SYNCVAR_BIND(x,v) x.u.s.data = INT64TOINT60(v)

static inline void print_stencil(stencil_t *stencil, size_t step)
{/*{{{*/
    fprintf(stderr, "Stencil:\n");
    fprintf(stderr, "\tpoints:     %lu x %lu\n",stencil->nrows,stencil->ncols);
    fprintf(stderr, "\tpartitions: %lu x %lu\n",stencil->prows,stencil->pcols);

    const size_t num_parts = stencil->prows * stencil->pcols;
    for (int pi = 0; pi < num_parts; pi++) {
        const partition_t *part = stencil->parts[pi];
        fprintf(stderr, "\tPartition: (%lu,%lu) %s, %lu x %lu\n",
            part->row, part->col, pos_strs[part->pos], part->nrows,part->ncols);
        for (int i = part->nrows-1; i >= 0; i--) {
            fprintf(stderr, "\t\t%02lu", 
                SYNCVAR_EVAL(part->stages[step][i][0]));
            for (int j = 1; j < part->ncols; j++) {
                fprintf(stderr, " %02lu", 
                    SYNCVAR_EVAL(part->stages[step][i][j]));
            }
            fprintf(stderr, "\n");
        }
    }
}/*}}}*/

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
// Stencil point update code

typedef struct upx_args_s {
    syncvar_t            ***stages;
    size_t                  now;
    Q_ALIGNED(8) position_t dir;
    Q_ALIGNED(8) position_t pos;
    size_t                  num_rows;
    size_t                  num_cols;
} upx_args_t;

typedef struct upi_args_s {
    syncvar_t ***stages;
    size_t       now;
    size_t       col;
} upi_args_t;

// Update internal point
static void update_point_internal(const size_t start, const size_t stop,
                                  void *arg_)
{
    const upi_args_t *arg = (upi_args_t *)arg_;
    syncvar_t ***stages = arg->stages;
    const size_t now    = arg->now;
    const size_t i      = start;
    const size_t j      = arg->col;

    syncvar_t **S = stages[prev_stage(now)];

    const uint64_t sum = SYNCVAR_EVAL(S[i][j]) +
                         SYNCVAR_EVAL(NORTH_OF(S,i,j)) +
                         SYNCVAR_EVAL(SOUTH_OF(S,i,j)) +
                         SYNCVAR_EVAL(WEST_OF(S,i,j)) +
                         SYNCVAR_EVAL(EAST_OF(S,i,j));

    SYNCVAR_BIND(stages[now][i][j], sum/NUM_NEIGHBORS);
}

// Spawn internal point tasks over columns
static void update_point_internal_loop(const size_t start, const size_t stop,
                                       void *arg_)
{
    const upx_args_t *arg = (upx_args_t *)arg_;
    const size_t num_rows = arg->num_rows;
    const position_t pos  = arg->pos;

    const size_t col_start = GHOST_SOUTH(pos) ? 2 : 1;
    const size_t col_stop  = GHOST_NORTH(pos) ? num_rows-2 : num_rows-1;

    upi_args_t upi_args = {arg->stages, arg->now, start};
    qt_loop(col_start, col_stop, update_point_internal, &upi_args);
}

static aligned_t update_point_corner(void *arg_)
{
    const upx_args_t *arg = (upx_args_t *)arg_;
    syncvar_t ***stages   = arg->stages;
    const position_t dir  = arg->dir;
    const size_t now      = arg->now;
    const size_t num_rows = arg->num_rows;
    const size_t num_cols = arg->num_cols;

    syncvar_t **S  = stages[prev_stage(now)];
    size_t i       = 0;
    size_t j       = 0;
    uint64_t sum   = 0;
    uint64_t value = 0;

    switch (dir) {
        case NW: 
            i = num_rows - 2;
            j = 1;
            sum = SYNCVAR_EVAL(S[i][j]) +
                  SYNCVAR_EVAL(SOUTH_OF(S,i,j)) +
                  SYNCVAR_EVAL(EAST_OF(S,i,j));
            qthread_syncvar_readFE(&value, &NORTH_OF(S,i,j));
            sum += value;
            qthread_syncvar_readFE(&value, &WEST_OF(S,i,j));
            sum += value;
            break;
        case NE:
            i = num_rows - 2;
            j = num_cols - 2;
            sum = SYNCVAR_EVAL(S[i][j]) +
                  SYNCVAR_EVAL(SOUTH_OF(S,i,j)) +
                  SYNCVAR_EVAL(WEST_OF(S,i,j));
            qthread_syncvar_readFE(&value, &NORTH_OF(S,i,j));
            sum += value;
            qthread_syncvar_readFE(&value, &EAST_OF(S,i,j));
            sum += value;
            break;
        case SW:
            i = 1;
            j = 1;
            sum = SYNCVAR_EVAL(S[i][j]) +
                  SYNCVAR_EVAL(NORTH_OF(S,i,j)) +
                  SYNCVAR_EVAL(EAST_OF(S,i,j));
            qthread_syncvar_readFE(&value, &SOUTH_OF(S,i,j));
            sum += value;
            qthread_syncvar_readFE(&value, &WEST_OF(S,i,j));
            sum += value;
            break;
        case SE:
            i = 1;
            j = num_cols - 2;
            sum = SYNCVAR_EVAL(S[i][j]) +
                  SYNCVAR_EVAL(NORTH_OF(S,i,j)) +
                  SYNCVAR_EVAL(WEST_OF(S,i,j));
            qthread_syncvar_readFE(&value, &SOUTH_OF(S,i,j));
            sum += value;
            qthread_syncvar_readFE(&value, &EAST_OF(S,i,j));
            sum += value;
            break;
        default:
            abort();
            break;
    }

    qthread_syncvar_writeEF_const(&stages[now][i][j],sum/NUM_NEIGHBORS);

    return 0;
}

static void update_point_edge(const size_t start, const size_t stop, void *arg_)
{
    const upx_args_t *arg = (upx_args_t *)arg_;
    syncvar_t ***stages = arg->stages;
    const size_t now    = arg->now;
    const position_t dir = arg->dir;
    const size_t num_rows = arg->num_rows;
    const size_t num_cols = arg->num_cols;

    syncvar_t **S = stages[prev_stage(now)];
    size_t i       = 0;
    size_t j       = 0;
    uint64_t sum   = 0;

    switch (dir) {
        case NORTH:
            i = num_rows-2;
            j = start;
            qthread_syncvar_readFE(&sum, &NORTH_OF(S,i,j));
            sum += SYNCVAR_EVAL(S[i][j]) +
                   SYNCVAR_EVAL(SOUTH_OF(S,i,j)) +
                   SYNCVAR_EVAL(WEST_OF(S,i,j)) +
                   SYNCVAR_EVAL(EAST_OF(S,i,j));
            break;
        case SOUTH:
            i = 1;
            j = start;
            qthread_syncvar_readFE(&sum, &SOUTH_OF(S,i,j));
            sum += SYNCVAR_EVAL(S[i][j]) +
                   SYNCVAR_EVAL(NORTH_OF(S,i,j)) +
                   SYNCVAR_EVAL(WEST_OF(S,i,j)) +
                   SYNCVAR_EVAL(EAST_OF(S,i,j));
            break;
        case WEST:
            i = start;
            j = 1;
            qthread_syncvar_readFE(&sum, &WEST_OF(S,i,j));
            sum += SYNCVAR_EVAL(S[i][j]) +
                   SYNCVAR_EVAL(NORTH_OF(S,i,j)) +
                   SYNCVAR_EVAL(SOUTH_OF(S,i,j)) +
                   SYNCVAR_EVAL(EAST_OF(S,i,j));
            break;
        case EAST:
            i = start;
            j = num_cols - 2;
            qthread_syncvar_readFE(&sum, &EAST_OF(S,i,j));
            sum += SYNCVAR_EVAL(S[i][j]) +
                   SYNCVAR_EVAL(NORTH_OF(S,i,j)) +
                   SYNCVAR_EVAL(SOUTH_OF(S,i,j)) +
                   SYNCVAR_EVAL(WEST_OF(S,i,j));
            break;
        default:
            abort();
            break;
    }
    
    qthread_syncvar_writeEF_const(&stages[now][i][j],sum/NUM_NEIGHBORS);
}

// Spawn tasks along edges
static aligned_t update_point_edge_loop(void *arg_)
{
    upx_args_t *arg = (upx_args_t *)arg_;
    const position_t dir = arg->dir;
    const position_t pos = arg->pos;
    const size_t num_rows = arg->num_rows;
    const size_t num_cols = arg->num_cols;

    switch (dir) {
        case NORTH:
        {
            const size_t lb = GHOST_WEST(pos) ? 2 : 1;
            const size_t ub = GHOST_EAST(pos) ? num_cols-2 : num_cols-1;

            qt_loop(lb, ub, update_point_edge, arg);
            break;
        }
        case SOUTH:
        {
            const size_t lb = GHOST_WEST(pos) ? 2 : 1;
            const size_t ub = GHOST_EAST(pos) ? num_cols-2 : num_cols-1;

            qt_loop(lb, ub, update_point_edge, arg);
            break;
        }
        case WEST:
        {
            const size_t lb = GHOST_SOUTH(pos) ? 2 : 1;
            const size_t ub = GHOST_NORTH(pos) ? num_rows-2 : num_rows-1;

            qt_loop(lb, ub, update_point_edge, arg);
            break;
        }
        case EAST:
        {
            const size_t lb = GHOST_SOUTH(pos) ? 2 : 1;
            const size_t ub = GHOST_NORTH(pos) ? num_rows-2 : num_rows-1;

            qt_loop(lb, ub, update_point_edge, arg);
            break;
        }
        default:
            abort();
    }

    return 0;
}

typedef struct us_args_s {
    size_t part_lid;
    size_t timestep;
    partition_t *part;
    const stencil_t *points;
} us_args_t;

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

    syncvar_t ***const stages = arg->part->stages;

    syncvar_t rets[8];
    for (int i = 0; i < 8; i++)
        rets[i] = SYNCVAR_INITIALIZER;

    upx_args_t upx_args = {stages, now, 0, pos, num_rows, num_cols};

    // Process (up to) four corner cases
    if (HAS_NW_CORNER(pos)) {
        upx_args.dir = NW;
        qthread_fork_syncvar_copyargs(update_point_corner, 
            &upx_args, sizeof(upx_args), &rets[0]);
    }
    if (HAS_NE_CORNER(pos)) {
        upx_args.dir = NE;
        qthread_fork_syncvar_copyargs(update_point_corner, 
            &upx_args, sizeof(upx_args), &rets[1]);
    }
    if (HAS_SW_CORNER(pos)) {
        upx_args.dir = SW;
        qthread_fork_syncvar_copyargs(update_point_corner, 
            &upx_args, sizeof(upx_args), &rets[2]);
    }
    if (HAS_SE_CORNER(pos)) {
        upx_args.dir = SE;
        qthread_fork_syncvar_copyargs(update_point_corner, 
            &upx_args, sizeof(upx_args), &rets[3]);
    }

    // Process edge cases
    if (GHOST_NORTH(pos)) {
        upx_args.dir = NORTH;
        qthread_fork_syncvar_copyargs(
			update_point_edge_loop, &upx_args, sizeof(upx_args_t), &rets[4]);
    }
    if (GHOST_SOUTH(pos)) {
        upx_args.dir = SOUTH;
        qthread_fork_syncvar_copyargs(
			update_point_edge_loop, &upx_args, sizeof(upx_args_t), &rets[5]);
    }
    if (GHOST_WEST(pos)) {
        upx_args.dir = WEST;
        qthread_fork_syncvar_copyargs(
			update_point_edge_loop, &upx_args, sizeof(upx_args_t), &rets[6]);
    }
    if (GHOST_EAST(pos)) {
        upx_args.dir = EAST;
        qthread_fork_syncvar_copyargs(
			update_point_edge_loop, &upx_args, sizeof(upx_args_t), &rets[7]);
    }

    // Process internal points
    //up_args_t up_args = {arg->part->stages, now, 0, 0, 0};
    //upil_args_t upil_args = {num_rows, num_cols, pos, &up_args};
    upx_args.dir = 0;
    upx_args.pos = pos;

    const size_t start = GHOST_WEST(pos) ? 2 : 1;
    const size_t stop  = GHOST_EAST(pos) ? num_cols-2 : num_cols-1;
    qt_loop(start, stop, update_point_internal_loop, &upx_args);

    // Wait for corner and edge cases to finish
    for (int i = 0; i < 8; i++)
        qthread_syncvar_readFF(NULL, &rets[i]);

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Halo exchange

typedef struct sb_args_s {
    syncvar_t *const target;
    syncvar_t *const source;
} sb_args_t;

typedef struct sbc_args_s {
    syncvar_t *const target1;
    syncvar_t *const target2;
    syncvar_t *const source;
} sbc_args_t;

typedef struct su_args_s {
    size_t part_lid;
    size_t stage;
    const stencil_t *points;
} su_args_t;

/*
 * Pre: source is filled
 * Post: target is filled
 * 1) empty source
 * 2) fill target
 */
static aligned_t send_block(void *arg_) {
    const sb_args_t *arg = (sb_args_t *)arg_;

    uint64_t value;
    qthread_syncvar_readFE(&value, arg->source);
    qthread_syncvar_writeEF(arg->target, &value);

    return 0;
}

// Corner case is needed so we don't readFE() the same source twice
static aligned_t send_block_corner(void *arg_) 
{
    const sbc_args_t *arg = (sbc_args_t *)arg_;

    uint64_t value;
    qthread_syncvar_readFE(&value, arg->source);
    qthread_syncvar_writeEF(arg->target1, &value);
    qthread_syncvar_writeEF(arg->target2, &value);

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
    syncvar_t **const  src_stage = src_part->stages[stage];
    const size_t       src_nrows = src_part->nrows;
    const size_t       src_ncols = src_part->ncols;

    // Send updates along edges
    if (GHOST_NORTH(src_pos)) {
        const size_t tgt_lid = 
            get_lid(src_part->row + 1, src_part->col, points->pcols);
        const partition_t *tgt_part   = points->parts[tgt_lid];
        syncvar_t **const  tgt_stage  = tgt_part->stages[stage];

        const size_t i = src_nrows - 2;
        const size_t lb = GHOST_WEST(src_pos) ? 2 : 1;
        const size_t ub = GHOST_EAST(src_pos) ? src_ncols-2 : src_ncols-1;
        for (size_t j = lb; j < ub; j++) {
            const sb_args_t sb_args = {&tgt_stage[0][j], &src_stage[i][j]};
            qthread_fork_copyargs(send_block, &sb_args, 
                sizeof(sb_args_t), NULL);
        }
    }
    if (GHOST_SOUTH(src_pos)) {
        const size_t tgt_lid = 
            get_lid(src_part->row - 1, src_part->col, points->pcols);
        const partition_t *tgt_part   = points->parts[tgt_lid];
        syncvar_t  **const tgt_stage  = tgt_part->stages[stage];
        const size_t       tgt_nrows  = tgt_part->nrows;

        const size_t i = 1;
        const size_t lb = GHOST_WEST(src_pos) ? 2 : 1;
        const size_t ub = GHOST_EAST(src_pos) ? src_ncols-2 : src_ncols-1;
        for (size_t j = lb; j < ub; j++) {
            const sb_args_t sb_args = 
                {&tgt_stage[tgt_nrows-1][j], &src_stage[i][j]};
            qthread_fork_copyargs(send_block, &sb_args, 
                sizeof(sb_args_t), NULL);
        }
    }
    if (GHOST_WEST(src_pos)) {
        const size_t tgt_lid = 
            get_lid(src_part->row, src_part->col-1, points->pcols);
        const partition_t *tgt_part   = points->parts[tgt_lid];
        syncvar_t  **const tgt_stage  = tgt_part->stages[stage];
        const size_t       tgt_ncols  = tgt_part->ncols;

        const size_t j = 1;
        const size_t lb = GHOST_SOUTH(src_pos) ? 2 : 1;
        const size_t ub = GHOST_NORTH(src_pos) ? src_nrows-2 : src_nrows-1;
        for (size_t i = lb; i < ub; i++) {
            const sb_args_t sb_args = 
                {&tgt_stage[i][tgt_ncols-1], &src_stage[i][j]};
            qthread_fork_copyargs(send_block, &sb_args, 
                sizeof(sb_args_t), NULL);
        }
    }

    if (GHOST_EAST(src_pos)) {
        const size_t tgt_lid = 
            get_lid(src_part->row, src_part->col+1, points->pcols);
        const partition_t *tgt_part   = points->parts[tgt_lid];
        syncvar_t  **const tgt_stage  = tgt_part->stages[stage];

        const size_t j = src_ncols-2;
        const size_t lb = GHOST_SOUTH(src_pos) ? 2 : 1;
        const size_t ub = GHOST_NORTH(src_pos) ? src_nrows-2 : src_nrows-1;
        for (size_t i = lb; i < ub; i++) {
            const sb_args_t sb_args = 
                {&tgt_stage[i][0], &src_stage[i][j]};
            qthread_fork_copyargs(send_block, &sb_args, 
                sizeof(sb_args_t), NULL);
        }
    }

    // Send updates at corners
    if (GHOST_WEST(src_pos) && GHOST_NORTH(src_pos)) {
        const size_t tgt1_lid = 
            get_lid(src_part->row, src_part->col-1, points->pcols);
        const partition_t *tgt1_part   = points->parts[tgt1_lid];
        syncvar_t  **const tgt1_stage  = tgt1_part->stages[stage];
        const size_t       tgt1_ncols  = tgt1_part->ncols;

        const size_t tgt2_lid = 
            get_lid(src_part->row + 1, src_part->col, points->pcols);
        const partition_t *tgt2_part   = points->parts[tgt2_lid];
        syncvar_t **const  tgt2_stage  = tgt2_part->stages[stage];

        const size_t i = src_nrows-2;
        const size_t j = 1;
        const sbc_args_t sbc_args = 
            {&tgt1_stage[i][tgt1_ncols-1], 
             &tgt2_stage[0][j],
             &src_stage[i][j]};
        qthread_fork_copyargs(send_block_corner, &sbc_args, 
            sizeof(sbc_args_t), NULL);
    }

    if (GHOST_WEST(src_pos) && GHOST_SOUTH(src_pos)) {
        const size_t tgt1_lid = 
            get_lid(src_part->row, src_part->col-1, points->pcols);
        const partition_t *tgt1_part   = points->parts[tgt1_lid];
        syncvar_t  **const tgt1_stage  = tgt1_part->stages[stage];
        const size_t       tgt1_ncols  = tgt1_part->ncols;

        const size_t tgt2_lid = 
            get_lid(src_part->row - 1, src_part->col, points->pcols);
        const partition_t *tgt2_part   = points->parts[tgt2_lid];
        syncvar_t **const  tgt2_stage  = tgt2_part->stages[stage];
        const size_t       tgt2_nrows  = tgt2_part->nrows;

        const size_t i = 1;
        const size_t j = 1;
        const sbc_args_t sbc_args = 
            {&tgt1_stage[i][tgt1_ncols-1], 
             &tgt2_stage[tgt2_nrows-1][j],
             &src_stage[i][j]};
        qthread_fork_copyargs(send_block_corner, &sbc_args, 
            sizeof(sbc_args_t), NULL);
    }

    if (GHOST_EAST(src_pos) && GHOST_NORTH(src_pos)) {
        const size_t tgt1_lid = 
            get_lid(src_part->row, src_part->col+1, points->pcols);
        const partition_t *tgt1_part   = points->parts[tgt1_lid];
        syncvar_t  **const tgt1_stage  = tgt1_part->stages[stage];

        const size_t tgt2_lid = 
            get_lid(src_part->row + 1, src_part->col, points->pcols);
        const partition_t *tgt2_part   = points->parts[tgt2_lid];
        syncvar_t  **const tgt2_stage  = tgt2_part->stages[stage];

        const size_t i = src_nrows-2;
        const size_t j = src_ncols-2;
        const sbc_args_t sbc_args = 
            {&tgt1_stage[i][0], 
             &tgt2_stage[0][j],
             &src_stage[i][j]};
        qthread_fork_copyargs(send_block_corner, &sbc_args, 
            sizeof(sbc_args_t), NULL);
    }

    if (GHOST_EAST(src_pos) && GHOST_SOUTH(src_pos)) {
        const size_t tgt1_lid = 
            get_lid(src_part->row, src_part->col+1, points->pcols);
        const partition_t *tgt1_part   = points->parts[tgt1_lid];
        syncvar_t  **const tgt1_stage  = tgt1_part->stages[stage];

        const size_t tgt2_lid = 
            get_lid(src_part->row - 1, src_part->col, points->pcols);
        const partition_t *tgt2_part   = points->parts[tgt2_lid];
        syncvar_t  **const tgt2_stage  = tgt2_part->stages[stage];
        const size_t       tgt2_nrows  = tgt2_part->nrows;

        const size_t j = src_ncols-2;
        const size_t i = 1;
        const sbc_args_t sbc_args = 
            {&tgt1_stage[i][0], 
             &tgt2_stage[tgt2_nrows-1][j], 
             &src_stage[i][j]};
        qthread_fork_copyargs(send_block_corner, &sbc_args, 
            sizeof(sbc_args_t), NULL);
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Local work
/*
 *  -   Starts off local processing for a partition
 */
static void begin(const size_t start, const size_t stop, void *arg_)
{/*{{{*/
    const stencil_t *arg = (stencil_t *)arg_;

    const size_t part_lid = start;

    for (size_t t = 1; t <= num_timesteps; t++) {
        // Send outgoing values to neighboring ghost cells
        const su_args_t su_args = {part_lid, (t-1)%2, arg};
        qthread_fork(send_updates, &su_args, NULL);

        // Compute a step
        const us_args_t us_args = {part_lid, t, arg->parts[part_lid], arg};
        syncvar_t up_ret = SYNCVAR_EMPTY_INITIALIZER;
        qthread_fork_syncvar(update_stage, &us_args, &up_ret);
        qthread_syncvar_readFF(NULL, &up_ret);
    }
}/*}}}*/

////////////////////////////////////////////////////////////////////////////////
// Stencil setup and tear down

static void setup_stencil(const size_t start, const size_t stop, void *arg)
{/*{{{*/
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

        part->stages[0] = malloc(part->nrows * sizeof(syncvar_t *));
        assert(part->stages[0]);
        part->stages[1] = malloc(part->nrows * sizeof(syncvar_t *));
        assert(part->stages[1]);
        for (int pi = 0; pi < part->nrows; pi++) {
            part->stages[0][pi] = malloc(part->ncols * sizeof(syncvar_t));
            assert(part->stages[0][pi]);
            part->stages[1][pi] = malloc(part->ncols * sizeof(syncvar_t));
            assert(part->stages[1][pi]);
            for (int pj = 0; pj < part->ncols; pj++) {
                part->stages[0][pi][pj] = INTERNAL_POINT;
                part->stages[1][pi][pj] = INTERNAL_POINT;
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
                part->stages[0][0][j] = boundary_value;
                part->stages[1][0][j] = boundary_value;
            }
        if (!GHOST_WEST(pos))
            for (int i = 1; i < nrows-1; i++) {
                part->stages[0][i][0] = boundary_value;
                part->stages[1][i][0] = boundary_value;
            }
        if (!GHOST_EAST(pos))
            for (int i = 1; i < nrows-1; i++) {
                part->stages[0][i][ncols-1] = boundary_value;
                part->stages[1][i][ncols-1] = boundary_value;
            }
        if (!GHOST_NORTH(pos))
            for (int j = 1; j < ncols-1; j++) {
                part->stages[0][nrows-1][j] = boundary_value;
                part->stages[1][nrows-1][j] = boundary_value;
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
                part->stages[0][i][j] = GHOST_POINT_EMPTY;
                part->stages[1][i][j] = GHOST_POINT_EMPTY;
                // Edge points
                part->stages[0][i+1][j] = EDGE_POINT_FULL;
                part->stages[1][i+1][j] = EDGE_POINT_EMPTY;
            }
        }
        if (GHOST_NORTH(pos)) {
            const size_t i = num_rows - 1;
            for (int j = 1; j < num_cols-1; j++) {
                // Ghost points
                part->stages[0][i][j] = GHOST_POINT_EMPTY;
                part->stages[1][i][j] = GHOST_POINT_EMPTY;
                // Edge points
                part->stages[0][i-1][j] = EDGE_POINT_FULL;
                part->stages[1][i-1][j] = EDGE_POINT_EMPTY;
            }
        }
        if (GHOST_WEST(pos)) {
            const size_t j = 0;
            for (int i = 1; i < num_rows-1; i++) {
                // Ghost points
                part->stages[0][i][j] = GHOST_POINT_EMPTY;
                part->stages[1][i][j] = GHOST_POINT_EMPTY;
                // Edge points
                part->stages[0][i][j+1] = EDGE_POINT_FULL;
                part->stages[1][i][j+1] = EDGE_POINT_EMPTY;
            }
        }
        if (GHOST_EAST(pos)) {
            const size_t j = num_cols - 1;
            for (int i = 1; i < num_rows-1; i++) {
                // Ghost points
                part->stages[0][i][j] = GHOST_POINT_EMPTY;
                part->stages[1][i][j] = GHOST_POINT_EMPTY;
                // Edge points
                part->stages[0][i][j-1] = EDGE_POINT_FULL;
                part->stages[1][i][j-1] = EDGE_POINT_EMPTY;
            }
        }
    }
}/*}}}*/

static inline void destroy_stencil(stencil_t *points)
{/*{{{*/
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
}/*}}}*/

int main(int argc, char *argv[])
{/*{{{*/
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
}/*}}}*/

/* vim:set expandtab */
