#ifndef QT_PERFORMANCE_H
#define QT_PERFORMANCE_H
#include<stddef.h>

#ifndef PERFDBG
#  define PERFDBG 0
#endif

typedef unsigned char bool;
typedef size_t qtperfid_t;
typedef unsigned long long qtperfcounter_t;
typedef unsigned long qttimestamp_t;
static const qtperfid_t QTPERF_INVALID_STATE = (qtperfid_t)(-1);

struct qtperf_perf_list_s;
typedef struct qtstategroup_s {
  size_t num_states;
  char* name;
  char** state_names;
  size_t num_counters;
  struct qtperf_perf_list_s** next_counter;
  struct qtperf_perf_list_s* counters;
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
  unsigned long busy;// 1 if somebody is using this structure, else 0
} qtperfdata_t;

/* a linked list of performance trackers. */
typedef struct qtperf_perf_list_s {
  qtperfdata_t performance_data;
  struct qtperf_perf_list_s* next;
} qtperf_perf_list_t;

typedef struct qtperf_iterator_s {
  qtperf_group_list_t* current_group;
  qtperf_perf_list_t* current;
} qtperf_iterator_t;


//--------------- WORKER INSTRUMENTATION ---------------------------- 
typedef enum {
  WKR_INIT,
  WKR_QTHREAD_ACTIVE,
  WKR_SHEPHERD,
  WKR_IDLE,
  WKR_BLOCKED,
  WKR_NUM_STATES
} worker_state_t;

extern bool qtperf_should_instrument_workers;
extern qtstategroup_t* qtperf_workers_group;

//--------------- QTHREAD INSTRUMENTATION ---------------------------- 
extern bool qtperf_should_instrument_qthreads;
extern qtstategroup_t* qtperf_qthreads_group;


//---------------- PERFORMANCE API -----------------------------------
qtstategroup_t* qtperf_create_state_group(size_t num_states, const char* group_name, const char** state_names);
qtperfdata_t* qtperf_create_perfdata(qtstategroup_t* state_group);
qttimestamp_t qtperf_now(void);
void qtperf_start(void);
void qtperf_stop(void);
void qtperf_free_data(void);
const char* qtperf_state_name(qtstategroup_t* group, qtperfid_t state_id);
void qtperf_enter_state(qtperfdata_t* data, qtperfid_t state_id);
void qtperf_iter_begin(qtperf_iterator_t** iter);
qtperfdata_t* qtperf_iter_next(qtperf_iterator_t** iter);
qtperfdata_t* qtperf_iter_deref(qtperf_iterator_t* iter);
qtperf_iterator_t* qtperf_iter_end(void);
void qtperf_set_instrument_workers(bool);
void qtperf_set_instrument_qthreads(bool);
void qtperf_print_results(void);
void qtperf_print_group(qtstategroup_t* group);
void qtperf_print_perfdata(qtperfdata_t* perfdata, bool show_states_with_zero_time);

#ifdef QTPERF_TESTING
#include<stdarg.h>
#include<stddef.h>
#include<setjmp.h>
#include<string.h>
#include<cmocka.h>
#include<ctype.h>
#define QTPERF_ASSERT(...) assert_true(__VA_ARGS__)

bool qtp_validate_names(const char** names, size_t count);
bool qtp_validate_state_group(qtstategroup_t*);
bool qtp_validate_perfdata(qtperfdata_t*);
bool qtp_validate_perf_list(qtperf_perf_list_t* list, qtperf_perf_list_t** next, size_t expected_items);
bool qtp_validate_group_list(void);
// This function checks the data structures used by qtperf to ensure
// that they are consistent. You should be able to call it at any
// time, as long as another call into qtperf is not running
// concurrently.
bool qtperf_check_invariants(void);

#else // ifdef QTPERF_TESTING

// define away unless QTPERF_TESTING is defined, otherwise it's an
// alias for cmocka's assert_true
#define QTPERF_ASSERT(...)

#endif // ifdef QTPERF_TESTING


// Convenience macros to limit the prevalence of ifndef/endif blocks
// in the main code
#ifdef QTHREAD_PERFORMANCE
#define QTPERF_ENTER_STATE(...) qtperf_enter_state(__VA_ARGS__)
#define QTPERF_WORKER_ENTER_STATE(pdata,state) do{      \
    if(qtperf_should_instrument_workers){\
      QTPERF_ASSERT(pdata != NULL);\
      qtperf_enter_state(pdata, state);\
    } } while(0)
#define QTPERF_QTHREAD_ENTER_STATE(pdata,state) do{                     \
    if(qtperf_should_instrument_qthreads){                                     \
        QTPERF_ASSERT(pdata != NULL);\
        qtperf_enter_state(pdata, state);\
    }} while(0)

#else
# define QTPERF_ENTER_STATE(...)
#endif // ifdef QTHREAD_PERFORMANCE


#endif
