#include<qthread/performance.h>
#include<qthread/logging.h>
#include<qthread/qthread.h>
#include<string.h>
#include<strings.h>
#include<stdlib.h>
#include<time.h>

#ifdef QTHREAD_PERFORMANCE

qtperf_group_list_t* _groups=NULL;
qtperf_group_list_t** _next_group=NULL;
size_t _num_groups=0;
size_t _num_counters=0;
bool _collecting=0;
bool qtperf_should_instrument_workers = 0;
bool qtperf_should_instrument_qthreads = 0;
qtstategroup_t* qtperf_workers_group = NULL;
qtstategroup_t* qtperf_qthreads_group = NULL;
  
bool incr_counter(qtperf_iterator_t**);
bool incr_group(qtperf_iterator_t**);
void qtperf_free_state_group_internals(qtstategroup_t*);
void qtperf_free_group_list(void);
void qtperf_free_perfdata_internals(qtperfdata_t*);
void qtperf_free_perf_list(qtstategroup_t*,qtperf_perf_list_t*);


qtstategroup_t* qtperf_create_state_group(size_t num_states, const char* name, const char** state_names) {
  qtperf_group_list_t* current = NULL;
  if(_next_group == NULL){
    _next_group = &_groups;
  }
  *_next_group = malloc(sizeof(qtperf_group_list_t));
  qtlogargs(PERFDBG, "create group %p", *_next_group);
  current = *_next_group;
  _next_group = &current->next;
  current->next = NULL;
  current->group.num_states = num_states;
  if(name != NULL){
    current->group.name = strdup(name);
  } else {
    current->group.name = strdup("Unnamed group");
  }
  current->group.state_names = NULL;
  current->group.num_counters = 0;
  current->group.next_counter = NULL;
  current->group.counters = NULL;
  if(state_names != NULL){
    size_t i=0; 
    current->group.state_names = calloc(num_states,sizeof(char*));
    for(i=0; i<num_states; i++){
      current->group.state_names[i] = strdup(state_names[i]);
    }
  }
  _num_groups++;
  return &current->group;
}

/* INTERNAL - free allocated memory from a stategroup structure */
void qtperf_free_state_group_internals(qtstategroup_t* group) {
  free(group->name);
  qtperf_free_perf_list(group,group->counters);
  if(group->state_names != NULL){
    size_t i=0;
    for(i=0; i<group->num_states; i++){
      free(group->state_names[i]);
    }
    bzero(group->state_names, sizeof(char*)*group->num_states);
    free(group->state_names);
    group->name = NULL;
    group->state_names = NULL;
    group->counters = NULL;
    group->next_counter = NULL;
    QTPERF_ASSERT(group->num_counters == 0);
  }
}
/* Free the entire list of state groups */
void qtperf_free_group_list(){
  qtperf_group_list_t* next=NULL;
  qtlogargs(PERFDBG, "num_groups = %lu", _num_groups);
  while(_groups != NULL){
    next = _groups->next;
    qtperf_free_state_group_internals(&_groups->group);
    _groups->next = NULL;
    qtlogargs(PERFDBG, "free group %p", _groups);
    free(_groups);
    _groups = next;
    _num_groups--;
  }
  QTPERF_ASSERT(_num_groups == 0);
}

qtperfdata_t* qtperf_create_perfdata(qtstategroup_t* state_group) {
  qtperf_perf_list_t* current=NULL;
  if(state_group->next_counter == NULL){
    state_group->next_counter = &state_group->counters;
  }
  *state_group->next_counter = malloc(sizeof(qtperf_perf_list_t));
  current = *state_group->next_counter;
  bzero(current, sizeof(qtperf_perf_list_t));
  state_group->next_counter = &current->next;
  current->next = NULL;
  current->performance_data.perf_counters = calloc(state_group->num_states,sizeof(qtperfcounter_t));
  bzero(current->performance_data.perf_counters, sizeof(qtperfcounter_t)*state_group->num_states);
  current->performance_data.current_state=QTPERF_INVALID_STATE;
  current->performance_data.time_entered=0;
  current->performance_data.state_group = state_group;
  current->performance_data.num_states = state_group->num_states;
  state_group->num_counters++;
  return &current->performance_data;
}

void qtperf_free_perfdata_internals(qtperfdata_t* perfdata) {
  free(perfdata->perf_counters);
  perfdata->perf_counters = NULL;
  perfdata->state_group = NULL;
}

void qtperf_free_perf_list(qtstategroup_t* group, qtperf_perf_list_t* counters){
  qtperf_perf_list_t* next = NULL;
  while(counters != NULL){
    next = counters->next;
    qtperf_free_perfdata_internals(&counters->performance_data);
    counters->next = NULL;
    free(counters);
    counters = next;
    group->num_counters--;
  }
  group->next_counter = &group->counters;
}

qttimestamp_t qtperf_now(){
  qttimestamp_t time=0;
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  time = ts.tv_sec * 1000000 + ts.tv_nsec/1000;
  return time;
}

void qtperf_start(){
  qtperfcounter_t now = qtperf_now();
  qtperf_iterator_t iter_box;
  qtperf_iterator_t* iter=&iter_box;
  qtperfdata_t* data = NULL;
  _collecting = 1;
  // Now I need to resume the counters, but pretend that any elapsed
  // time did not actually happen (set time_entered to now)
  // TODO XXX This needs to be locked! RACE CONDITION
  qtperf_iter_begin(&iter);
  for(data = qtperf_iter_next(&iter); data != NULL; data = qtperf_iter_next(&iter)){
    if(data->current_state != QTPERF_INVALID_STATE){
      data->time_entered = now;
    }
  }
}

void qtperf_stop() {
  qtperfcounter_t now = qtperf_now();
  qtperf_iterator_t iter_box;
  qtperf_iterator_t* iter=&iter_box;
  qtperfdata_t* data = NULL;
  _collecting = 0;
  // Now I need to record the time spent in the current state for all
  // active records
  // TODO XXX This needs to be locked! RACE CONDITION
  qtperf_iter_begin(&iter);
  for(data = qtperf_iter_next(&iter); data != NULL; data = qtperf_iter_next(&iter)){
    if(data->current_state != QTPERF_INVALID_STATE){
      data->perf_counters[data->current_state] += now - data->time_entered;
      data->time_entered = now;
    }
  }
}

void qtperf_free_data(){
  // Stop all threads, otherwise we'll have a crash
  qthread_finalize();
  qtperf_free_group_list();
  _groups = NULL;
  _next_group = NULL;
  _collecting = 0;
}

const char* qtperf_state_name(qtstategroup_t* group, qtperfid_t state){
  if(state >= group->num_states){
    qtlogargs(LOGERR, "ERROR - STATE NUMBER %lu OUT OF BOUNDS FOR GROUP", state);
    return "ERROR";
  } else if(group->state_names == NULL){
    qtlog(LOGERR, "ERROR - GROUP DOES NOT HAVE STATE NAMES DEFINED");
    return "ERROR";
  }
  return group->state_names[state];
}

// This function will still change states even if _collecting is
// false, but the timing data will not be recorded.
void qtperf_enter_state(qtperfdata_t* data, qtperfid_t state){
  qttimestamp_t now = qtperf_now();
  //qtlogargs(1, "data %p", data);
  if(state >= data->state_group->num_states) {
    qtlogargs(LOGERR,"State number %lu is out of bounds!", state);
    return;
  }
  if(_collecting && data->current_state != QTPERF_INVALID_STATE) {
    data->perf_counters[data->current_state] += now - data->time_entered;
  }
  if(_collecting){
    data->time_entered= now;
  }
  data->current_state = state;
}

void qtperf_iter_begin(qtperf_iterator_t**iter){
  (*iter)->current_group = _groups;
  if((*iter)->current_group){
    (*iter)->current = _groups->group.counters;
  } else {
    (*iter)->current = NULL;
  }
}

qtperfdata_t* qtperf_iter_deref(qtperf_iterator_t* iter){
  if(iter == qtperf_iter_end()){
    return NULL;
  }
  return &iter->current->performance_data;
}

bool incr_counter(qtperf_iterator_t** iter){
  if(*iter != qtperf_iter_end() && (*iter)->current != NULL){
    (*iter)->current = (*iter)->current->next;
  }
  return (*iter)->current != NULL;
}

bool incr_group(qtperf_iterator_t** iter){
  if(*iter != qtperf_iter_end() && (*iter)->current_group != NULL){
    (*iter)->current_group = (*iter)->current_group->next;
    if((*iter)->current_group != NULL){
      (*iter)->current = (*iter)->current_group->group.counters;
    }
  }
  return (*iter)->current_group != NULL;
}

qtperfdata_t* qtperf_iter_next(qtperf_iterator_t** iter){
  qtperfdata_t* data = qtperf_iter_deref(*iter);
  if(!incr_counter(iter) && !incr_group(iter)){
    (*iter) = qtperf_iter_end();
    return NULL;
  }
  return data;
}

qtperf_iterator_t* qtperf_iter_end(){
  return NULL;
}


/* This section is for the instrumentation of workers */

static const char* worker_state_names[]={
  "WKR_INIT",
  "WKR_QTHREAD_ACTIVE",
  "WKR_SHEPHERD",
  "WKR_IDLE",
  "WKR_BLOCKED"
};

// need to set a flag that's visible to other files that can be used
// to control whether samples are recorded for worker state
// transitions. For now I'll see if I can use an inline function
// definition from performance.h
void qtperf_set_instrument_workers(bool yes_no){
  qtperf_should_instrument_workers = yes_no;
  // TODO: add code to instrument existing workers. As it stands, this
  // only affects workers spawned *after* this call, which means that
  // you have to call this function before qthread_initialize();

  // initialize the state group for worker data collection
  if(yes_no && qtperf_workers_group == NULL){
    qtperf_workers_group = qtperf_create_state_group(WKR_NUM_STATES, "Workers Internal", worker_state_names);
  }
  QTPERF_ASSERT((qtperf_should_instrument_workers && (qtperf_workers_group != NULL)) ||
                (!qtperf_should_instrument_workers && (qtperf_workers_group == NULL)));
}


/* This section is for instrumentation of qthreads */
typedef enum{
 QTH_NUM_STATES
} qthread_state_t;
static const char* qthread_state_names[] = {
};

void qtperf_set_instrument_qthreads(bool yes_no) {
  qtperf_should_instrument_qthreads = yes_no;

  if(yes_no && qtperf_qthreads_group == NULL){
    qtperf_qthreads_group = qtperf_create_state_group(QTH_NUM_STATES, "Qthreads Internal", qthread_state_names);
  }
  QTPERF_ASSERT((qtperf_should_instrument_qthreads && (qtperf_qthreads_group != NULL)) ||
                (!qtperf_should_instrument_qthreads && (qtperf_qthreads_group == NULL)));
}


/* PRINTING RESULTS */

void qtperf_print_results(){
  qtperf_group_list_t* current = NULL;
  for(current = _groups; current != NULL; current = current->next){
    qtperf_print_group(&current->group);
  }
}

#ifndef PERF_SHOW_STATES_WITH_ZERO_TIME
#define PERF_SHOW_STATES_WITH_ZERO_TIME 1
#endif

void qtperf_print_group(qtstategroup_t* group){
  qtperf_perf_list_t* current = NULL;
  size_t i=0;
  printf("%s (%lu instances, %lu total states)\n", group->name, group->num_counters, group->num_states);
  for(current = group->counters,i=0; current != NULL; current = current->next,i++){
    printf("  Instance %lu:\n", i);
    qtperf_print_perfdata(&current->performance_data, PERF_SHOW_STATES_WITH_ZERO_TIME);
  }
  printf("------------------------------------------------\n");
}

void qtperf_print_perfdata(qtperfdata_t* perfdata, bool show_zeros){
  size_t i=0;
  const char** names = (const char**)perfdata->state_group->state_names;
  qtperfcounter_t* data = perfdata->perf_counters;
  for(i=0; i<perfdata->state_group->num_states; i++){
    if(show_zeros || (data[i] != 0)){
      if(names != NULL){
        printf("    %s: %llu\n", names[i], data[i]);
      }else{
        printf("    state %lu: %llu\n", i, data[i]);
      }
    }
  }
}

/* Testing related functionality */

#ifdef QTPERF_TESTING

#define MAX_NAME_LENGTH 128
bool qtp_validate_names(const char** names, size_t count){
  size_t i=0;
  bool valid = 1;
  for(i=0; names != NULL && i<count; i++){
    size_t len=0;
    bool printable=1;
    for(len=0; names[i][len] != '\0' && len < MAX_NAME_LENGTH; len++){
      printable = printable && isprint(names[i][len]);
    }
    valid &= printable && len < MAX_NAME_LENGTH && len > 0 && names[i][len] == '\0';
    assert_true(printable && len < MAX_NAME_LENGTH && len > 0 && names[i][len] == '\0');
  }
  assert_true(names == NULL || valid);
  return valid;
}

bool qtp_validate_perfdata(qtperfdata_t* data){
  bool valid = 1;
  assert_true(data != NULL);
  assert_true(data->state_group != NULL);
  valid = data != NULL && data->state_group != NULL;
  valid = (data->num_states > 0);
  assert_true(data->num_states > 0);
  valid = valid && data->perf_counters != NULL;
  assert_true(data->perf_counters != NULL);
  valid = valid && ((data->current_state == QTPERF_INVALID_STATE && data->time_entered==0)
                    || (data->current_state != QTPERF_INVALID_STATE && data->time_entered > 0));
  qtlogargs(1,"data->current_state=%lu, data->time_entered=%lu", data->current_state, data->time_entered);
  assert_true((data->current_state == QTPERF_INVALID_STATE && data->time_entered==0)
              || (data->current_state != QTPERF_INVALID_STATE && data->time_entered > 0));
  return valid;
}

bool qtp_validate_perf_list(qtperf_perf_list_t* counters,
                            qtperf_perf_list_t** next_counter,
                            size_t num_counters){
  qtperf_perf_list_t* current = counters;
  bool valid=1;
  size_t found_counters=0;
  if(current == NULL){
    assert_true(next_counter == &counters || next_counter == NULL);
    valid = valid && (next_counter == &counters || next_counter == NULL);
  }
  while(current != NULL) {
    found_counters++;
    valid = valid && qtp_validate_perfdata(&current->performance_data);
    if(current->next == NULL){
      assert_true(next_counter == &current->next);
      valid = valid && next_counter == &current->next;
    }
    current = current->next;
  }
  assert_true(found_counters == num_counters);
  valid = valid && (found_counters == num_counters);
  return valid;
}

bool qtp_validate_state_group(qtstategroup_t* group){
  bool valid = group->num_states > 0;
  bool valid_names = 0;
  bool valid_counters = 0;
  valid_names = qtp_validate_names((const char**)group->state_names, group->num_states);
  valid_counters = qtp_validate_perf_list(group->counters, group->next_counter, group->num_counters);
  assert_true(valid_counters);
  assert_true(group->num_states > 0);
  valid = valid && ((group->state_names == NULL) || valid_names);
  assert_true((group->state_names == NULL) || valid_names);
  return valid;
}

bool qtp_validate_group_list(void){
  qtperf_group_list_t* current = _groups;
  size_t found_groups=0;
  bool valid=1;
  if(current == NULL){
    assert_true(_next_group == &_groups || _next_group == NULL);
    valid = valid && (_next_group==&_groups || _next_group == NULL);
  }
  while(current != NULL){
    found_groups++;
    valid = valid & qtp_validate_state_group(&current->group);
    if(current->next == NULL){
      assert_true(_next_group == &current->next);
      valid = valid && _next_group== &current->next;
    }
    current = current->next;
  }
  assert_true(found_groups == _num_groups);
  valid = valid && (found_groups == _num_groups);
  return valid;
}

bool qtperf_check_invariants(void) {
  assert_true(_collecting == 0 || _collecting == 1);
  return (_collecting == 0 || _collecting == 1) && qtp_validate_group_list();
}

#endif

#endif // ifdef QTHREAD_PERFORMANCE
