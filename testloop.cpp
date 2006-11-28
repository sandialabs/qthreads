#include <qthread/futurelib.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define NUM_LOCS() 3
#define FUTURE_PER_LOC() 128

static pthread_mutex_t all_done = PTHREAD_MUTEX_INITIALIZER;

struct main_args_s {
  int argc;
  char **argv;
};

void my_main();

void qmain(qthread_t *qthr, void *arg) {
  main_args_s *a = (main_args_s*)arg;
  int argc = a->argc;
  char **argv = a->argv;
  future_init(qthr, FUTURE_PER_LOC(), NUM_LOCS());

  my_main();

  pthread_mutex_unlock(&all_done);
}

int main (int argc, char **argv) {
  qthread_init(NUM_LOCS());
  main_args_s a;
  a.argc = argc;
  a.argv = argv;
  
  pthread_mutex_lock(&all_done);
  qthread_fork(qmain, &a);
  pthread_mutex_lock(&all_done);
  qthread_finalize();
}

void hello (int i, char* msg, char c) {
  printf ("%s %3d %3c\n", msg, i, c);
}

void incr(int& i) {
  qthread_t *me;
  qthread_lock(me, &i);
  i++;
  printf ("incr i (%p) = %d\n", &i, i);
  qthread_unlock(me, &i);
}

void set(int val, int& i) { 
  i = val; 
  printf ("set i (%p) = %d\n", &i, i);
}

void output(const int& i) { 
  printf ("output i (%p) = %d\n", &i, i); 
}

void output_double(double i) { 
  printf ("output double i (%p) = %.4f\n", &i, i); 
}

void ref(int& i) { 
  printf ("ref i (%p) = %d\n", &i, i); 
}

template <class T>
void recvData( const T& t ) {
  //printf ("Got data reference @ %p\n", &t);
}

class BigData {
  char buf[512];
public:
  static int copy_count;
  BigData() { 
    printf ("Make new BigData @ %p\n", this);
  }
  BigData(const BigData& cp) {
    copy_count++;
    printf ("Copy big data %p into new data @ %p\n", &cp, this);
  }
};

class UserArray {
  int *ptr;
public:
  UserArray (int size) { ptr = new int[size]; }
  int& operator[](int index) { return ptr[index]; }
};

int BigData::copy_count = 0;

#define ALIGN_ATTR __attribute__ ((aligned (8)))

template <class ArrayT>
void genericArraySet (ArrayT& arr, int size, char* name) {
  printf (">>>>>>  Array setting %s <<<<<<<\n", name);
  ParVoidLoop<Iterator, ArrayPtr<ArrayT>, loop::Par> (set, NULL, arr, 0, size);
};

template <class ArrayT>
void genericArrayPrint (ArrayT& arr, int size, char *name) {
  printf (">>>>>>  Array printing %s <<<<<<<\n", name);
  ParVoidLoop<ArrayPtr<ArrayT>, loop::Par> (output, arr, 0, size);
  printf (">>>>>>  Array printing double by value %s <<<<<<<\n", name);
  ParVoidLoop<ArrayPtr<ArrayT>, loop::Par> (output_double, arr, 0, size);
};

void my_main() {
  printf ("Hello main\n");

  char *msg = "Hello Thread!";

  const int size = 20;
  int *arr = new int[size];
  genericArraySet(arr, size, "Plain old Pointer");
  genericArrayPrint(arr, size, "Plain old Pointer");

  UserArray usr_arr(size);
  genericArraySet(usr_arr, size, "User Array Class");
  genericArrayPrint(usr_arr, size, "User Array Class");

  printf (">>>>>>  Msg printing <<<<<<<\n");
  ParVoidLoop<Iterator, char*, ArrayPtr<char*>, loop::Par> 
    (hello, NULL, msg, msg, 0, strlen(msg) - 1);


  int i = 7;
  printf ("i (%p) = %d\n", &i, i);
  printf (">>>>>>  Incrementing i <<<<<<<\n");
  ParVoidLoop<Ref<int>, loop::Par>  (incr, i, 0, 5);
  printf ("i (%p) = %d\n", &i, i);

  printf (">>>>>>  Setting i <<<<<<<\n");
  ParVoidLoop<Iterator, Ref<int>, loop::Par>  (set, NULL, i, 0, 5);
  printf ("i (%p) = %d\n", &i, i);

  //This will not compile
  //ParVoidLoop<Iterator, int, loop::Par>  (set, NULL, i, 0, 5);

  printf (">>>>>>  Printing constant int <<<<<<<\n");
  ParVoidLoop<int, loop::Par> (output, 3, 0, 3);

  //This will not compile
  //ParVoidLoop<int, loop::Par> (ref, 3, 0, 3);

  printf (">>>>>>  Printing Iterator <<<<<<<\n");
  ParVoidLoop<Iterator, loop::Par> (output, NULL, 0, 3);

  //This will not compile
  //ParVoidLoop<Iterator, loop::Par> (ref, NULL, 0, 3);


  const int N = 100;
  printf (">>>>> Pass Big Data by Value %d iterations <<<<\n", N);
  BigData bd;
  bd.copy_count = 0;
  ParVoidLoop<BigData, loop::Par> (recvData<BigData>, bd, 0, N);
  printf ("Made %d copies for %d iterations\n", bd.copy_count, N);

  printf (">>>>> Pass Big Data by Reference %d iterations <<<<\n", N);
  bd.copy_count = 0;
  ParVoidLoop<Ref<BigData>, loop::Par> (recvData<BigData>, bd, 0, N);
  printf ("Made %d copies for %d iterations\n", bd.copy_count, N);
}
