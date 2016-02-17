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
void qtperf_free_state_group(qtstategroup_t* group) {
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
    qtperf_free_state_group(&_groups->group);
    _groups->next = NULL;
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
  _next_counter = &current->next;
  current->next = NULL;
  current->performance_data.perf_counters = malloc(sizeof(qtperfcounter_t)*state_group->num_states);
  bzero(current->performance_data.perf_counters, sizeof(qtperfcounter_t)*state_group->num_states);
  current->performance_data.current_state=QTPERF_INVALID_STATE;
  current->performance_data.time_entered=0;
  return &current->performance_data;
}

void qtperf_free_perfdata(qtperfdata_t* perfdata) {
  free(&perfdata->perf_counters);
  perfdata->perf_counters = NULL;
  perfdata->state_group = NULL;
}

void qtperf_free_perf_list(){
  qtperf_perf_list_t* next = NULL;
  while(_counters != NULL){
    next = _counters->next;
    qtperf_free_perfdata(&next->performance_data);
    _counters->next = NULL;
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
}

const char* qtperf_state_name(qtstategroup_t* group, qtperfid_t state){
  if(state >= group->num_states){
    logargs(LOGERR, "ERROR - STATE NUMBER %lu OUT OF BOUNDS FOR GROUP", state);
    return "ERROR";
  } else if(group->state_names == NULL){
    log(LOGERR, "ERROR - GROUP DOES NOT HAVE STATE NAMES DEFINED");
    return "ERROR";
  }
  return group->state_names[state];
}

void qtperf_enter_state(qtperfdata_t* data, qtperfid_t state){
  qttimestamp_t now = qtperf_now();
  if(state >= data->state_group->num_states) {
    logargs(LOGERR,"State number %lu is out of bounds!", state);
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

#endif // ifdef QTHREAD_PERFORMANCE
