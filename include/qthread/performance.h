#ifndef QT_PERFORMANCE_H
#define QT_PERFORMANCE_H
#include<stddef.h>

typedef unsigned char bool;
typedef size_t qtperfid_t;
typedef unsigned long long qtperfcounter_t;
typedef unsigned long qttimestamp_t;
static const qtperfid_t QTPERF_INVALID_STATE = (qtperfid_t)(-1);

typedef struct qtstategroup_s {
  size_t num_states;
  char** state_names;
} qtstategroup_t;

typedef struct qtperf_group_list_s {
  qtstategroup_t group;
  struct qtperf_group_list_s* next;
} qtperf_group_list_t;
/**
   This is the main performance data structure where timing data is
   stored. Data is indexed by (unit,stategroup) tuples.
*/
typedef struct qtperfdata_s {
  qtstategroup_t* state_group;
  size_t num_states;
  qtperfcounter_t* perf_counters;
  qtperfid_t current_state;
  qttimestamp_t time_entered;
} qtperfdata_t;

/* a linked list of performance trackers. */
typedef struct qtperf_perf_list_s {
  qtperfdata_t performance_data;
  struct qtperf_perf_list_s* next;
} qtperf_perf_list_t;

typedef struct qtperf_iterator_s {
  qtperf_perf_list_t* current;
} qtperf_iterator_t;

qtstategroup_t* qtperf_create_state_group(size_t num_states, const char** state_names);
qtperfdata_t* qtperf_create_perfdata(qtstategroup_t* state_group);
void qtperf_start(void);
void qtperf_stop(void);
void qtperf_free_data(void);
const char* qtperf_state_name(qtstategroup_t* group, qtperfid_t state_id);
void qtperf_enter_state(qtperfdata_t* data, qtperfid_t state_id);
void qtperf_iter_begin(qtperf_iterator_t** iter);
qtperfdata_t* qtperf_iter_next(qtperf_iterator_t** iter);
qtperfdata_t* qtperf_iter_deref(qtperf_iterator_t* iter);
qtperf_iterator_t* qtperf_iter_end(void);

#endif
