#include<qthread/performance.h>
#include<qthread/logging.h>
#include<string.h>
#include<strings.h>
#include<stdlib.h>
#include<time.h>

#ifdef QTHREAD_PERFORMANCE

qtperf_perf_list_t* _counters=NULL;
qtperf_perf_list_t** _next_counter = NULL;
qtperf_group_list_t* _groups=NULL;
qtperf_group_list_t** _next_group=NULL;
bool _collecting=0;

void qtperf_free_state_group(qtstategroup_t*);
void qtperf_free_group_list(void);
void qtperf_free_perfdata(qtperfdata_t*);
void qtperf_free_perf_list(void);


qtstategroup_t* qtperf_create_state_group(size_t num_states, const char** state_names) {
  qtperf_group_list_t* current = NULL;
  if(_next_group == NULL){
    _next_group = &_groups;
  }
  *_next_group = malloc(sizeof(qtperf_group_list_t));
  current = *_next_group;
  _next_group = &current->next;
  current->next = NULL;
  current->group.num_states = num_states;
  current->group.state_names = NULL;
  if(state_names != NULL){
    size_t i=0; 
    current->group.state_names = malloc(sizeof(char*)*num_states);
    for(i=0; i<num_states; i++){
      current->group.state_names[i] = strdup(state_names[i]);
    }
  }
  return &current->group;
}

/* INTERNAL - free allocated memory from a stategroup structure */
void qtperf_free_state_group_internals(qtstategroup_t* group) {
  if(group->state_names != NULL){
    size_t i=0;
    for(i=0; i<group->num_states; i++){
      free(group->state_names[i]);
    }
    bzero(group->state_names, sizeof(char*)*group->num_states);
    free(group->state_names);
    group->state_names = NULL;
  }
}
/* Free the entire list of state groups */
void qtperf_free_group_list(){
  qtperf_group_list_t* next=NULL;
  while(_groups != NULL){
    next = _groups->next;
    qtperf_free_state_group_internals(&_groups->group);
    _groups->next = NULL;
    free(_groups);
    _groups = next;
  }
}



qtperfdata_t* qtperf_create_perfdata(qtstategroup_t* state_group) {
  qtperf_perf_list_t* current=NULL;
  if(_next_counter == NULL){
    _next_counter = &_counters;
  }
  *_next_counter = malloc(sizeof(qtperf_perf_list_t));
  current = *_next_counter;
  bzero(current, sizeof(qtperf_perf_list_t));
  _next_counter = &current->next;
  current->next = NULL;
  current->performance_data.perf_counters = malloc(sizeof(qtperfcounter_t)*state_group->num_states);
  bzero(current->performance_data.perf_counters, sizeof(qtperfcounter_t)*state_group->num_states);
  current->performance_data.current_state=QTPERF_INVALID_STATE;
  current->performance_data.time_entered=0;
  current->performance_data.state_group = state_group;
  current->performance_data.num_states = state_group->num_states;
  return &current->performance_data;
}

void qtperf_free_perfdata_internals(qtperfdata_t* perfdata) {
  free(perfdata->perf_counters);
  perfdata->perf_counters = NULL;
  perfdata->state_group = NULL;
}

void qtperf_free_perf_list(){
  qtperf_perf_list_t* next = NULL;
  while(_counters != NULL){
    next = _counters->next;
    qtperf_free_perfdata_internals(&_counters->performance_data);
    _counters->next = NULL;
    free(_counters);
    _counters = next;
  }
}

qttimestamp_t qtperf_now(){
  qttimestamp_t time=0;
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  time = ts.tv_sec * 1000000 + ts.tv_nsec/1000;
  return time;
}

void qtperf_start(){
  _collecting = 1;
}

void qtperf_stop() {
  _collecting = 0;
}

void qtperf_free_data(){
  qtperf_free_perf_list();
  qtperf_free_group_list();
  _counters = NULL;
  _next_counter = NULL;
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

void qtperf_enter_state(qtperfdata_t* data, qtperfid_t state){
  qttimestamp_t now = qtperf_now();
  if(state >= data->state_group->num_states) {
    qtlogargs(LOGERR,"State number %lu is out of bounds!", state);
    return;
  }
  if(data->current_state != QTPERF_INVALID_STATE) {
    data->perf_counters[data->current_state] += now - data->time_entered;
  }
  data->time_entered= now;
  data->current_state = state;
}

void qtperf_iter_begin(qtperf_iterator_t**iter){
  (*iter)->current = _counters;
}

qtperfdata_t* qtperf_iter_deref(qtperf_iterator_t* iter){
  if(iter == qtperf_iter_end()){
    return NULL;
  }
  return &iter->current->performance_data;
}

qtperfdata_t* qtperf_iter_next(qtperf_iterator_t** iter){
  qtperfdata_t* data = qtperf_iter_deref(*iter);
  if(*iter != qtperf_iter_end()){
    (*iter)->current = (*iter)->current->next;
    if((*iter)->current == NULL){
      *iter = qtperf_iter_end();
    }
  }
  return data;
}

qtperf_iterator_t* qtperf_iter_end(){
  return NULL;
}

#ifdef QTPERF_TESTING

#define MAX_NAME_LENGTH 128
bool qtp_validate_names(const char** names, size_t count){
  size_t i=0;
  bool valid = 1;
  for(i=0; i<count; i++){
    size_t len=0;
    bool printable=1;
    for(len=0; names[i][len] != '\0' && len < MAX_NAME_LENGTH; len++){
      printable = printable && isprint(names[i][len]);
    }
    valid &= printable && len < MAX_NAME_LENGTH && len > 0 && names[i][len] == '\0';
    assert_true(printable && len < MAX_NAME_LENGTH && len > 0 && names[i][len] == '\0');
  }
  
  return valid;
}

bool qtp_validate_state_group(qtstategroup_t* group){
  bool valid = group->num_states > 0;
  bool valid_names = 1;
  valid_names = qtp_validate_names((const char**)group->state_names, group->num_states);
  assert_true(group->num_states > 0);
  valid = valid && ((group->state_names == NULL) || valid_names);
  assert_true((group->state_names == NULL) || valid_names);
  return valid;
}

bool qtp_validate_perfdata(qtperfdata_t* data){
  bool valid = 1;
  assert_true(data != NULL);
  assert_true(data->state_group != NULL);
  valid = data != NULL && data->state_group != NULL;
  valid = qtp_validate_state_group(data->state_group);
  valid = valid && (data->num_states > 0);
  assert_true(data->num_states > 0);
  valid = valid && data->perf_counters != NULL;
  assert_true(data->perf_counters != NULL);
  valid = valid && ((data->current_state == QTPERF_INVALID_STATE && data->time_entered==0)
                    || (data->current_state != QTPERF_INVALID_STATE && data->time_entered > 0));
  assert_true((data->current_state == QTPERF_INVALID_STATE && data->time_entered==0)
              || (data->current_state != QTPERF_INVALID_STATE && data->time_entered > 0));
  return valid;
}

bool qtp_validate_perf_list(void){
  qtperf_perf_list_t* current = _counters;
  bool valid=1;
  if(current == NULL){
    assert_true(_next_counter == &_counters || _next_counter == NULL);
    valid = valid && (_next_counter == &_counters || _next_counter == NULL);
  }
  while(current != NULL) {
    valid = valid && qtp_validate_perfdata(&current->performance_data);
    if(current->next == NULL){
      assert_true(_next_counter == &current->next);
      valid = valid && _next_counter == &current->next;
    }
    current = current->next;
  }
  return valid;
}
//qtperf_perf_list_t* _counters=NULL;
//qtperf_perf_list_t** _next_counter = NULL;
//qtperf_group_list_t* _groups=NULL;
//qtperf_group_list_t** _next_group=NULL;
//bool _collecting=0;

bool qtp_validate_group_list(void){
  qtperf_group_list_t* current = _groups;
  bool valid=1;
  if(current == NULL){
    assert_true(_next_group == &_groups || _next_group == NULL);
    valid = valid && (_next_group==&_groups || _next_group == NULL);
  }
  while(current != NULL){
    valid = valid & qtp_validate_state_group(&current->group);
    if(current->next == NULL){
      assert_true(_next_group == &current->next);
      valid = valid && _next_group== &current->next;
    }
    current = current->next;
  }
  return valid;
}

bool qtperf_check_invariants(void) {
  assert_true(_collecting == 0 || _collecting == 1);
  return (_collecting == 0 || _collecting == 1) && qtp_validate_perf_list() && qtp_validate_group_list();
}

#endif

#endif // ifdef QTHREAD_PERFORMANCE
