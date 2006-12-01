#include "futurelib.h"
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

void hello (int i, const char* msg, char c) {
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
  i = val + 1; 
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
  printf ("Got data reference @ %p\n", &t);
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
int genericArraySet (ArrayT& arr, int size, char* name) {
  printf (">>>>>>  Array setting %s <<<<<<<\n", name);
  ParVoidLoop<Iterator, ArrayPtr, loop::Par> (set, 0, arr, 0, size);
  return 1;
};

template <class ArrayT>
void genericArrayPrint (ArrayT& arr, int size, char *name) {
  printf (">>>>>>  Array printing %s <<<<<<<\n", name);
  ParVoidLoop<ArrayPtr, loop::Par> (output, arr, 0, size);
  printf (">>>>>>  Array printing double by value %s <<<<<<<\n", name);
  ParVoidLoop<ArrayPtr, loop::Par> (output_double, arr, 0, size);
};

double assign (double val) {
  return val;
}


void array_stuff() {
  const int size = 100;
  int *arr = new int[size];
  genericArraySet(arr, size, "Plain old Pointer");
  genericArrayPrint(arr, size, "Plain old Pointer");

  //This compiles under some compilers - depends on support for typeof operator
  //UserArray usr_arr(size);
  //genericArraySet(usr_arr, size, "User Array Class");
  //genericArrayPrint(usr_arr, size, "User Array Class");

  printf (">>>>> Copy Array <<<<<\n");
  int *new_arr = new int[size];
  ParLoop <ArrayPtr, ArrayPtr, loop::Par>
    (new_arr, assign, arr, 0, size);
  genericArrayPrint(new_arr, size, "Array Copy");

  int sum = 0;
  printf (">>>>> Adding Array <<<<<\n");
  ParLoop <Collect<loop::Add>, ArrayPtr, loop::Par>
    (sum, assign, arr, 0, size);
  printf ("Sum = %d\n", sum);

  sum = 0;
  printf (">>>>> Subbing Array <<<<<\n");
  ParLoop <Collect<loop::Sub>, ArrayPtr, loop::Par>
    (sum, assign, arr, 0, size);
  printf ("Diff = %d\n", sum);

  double product = 1;
  printf (">>>>> Multiplying first 10 in Array <<<<<\n");
  ParLoop <Collect<loop::Mult>, ArrayPtr, loop::Par>
    (product, assign, arr, 0, 10);
  printf ("Product = %f\n", product);

  product = 1;
  printf (">>>>> Dividing by first 5 in Array <<<<<\n");
  ParLoop <Collect<loop::Div>, ArrayPtr, loop::Par>
    (product, assign, arr, 0, 5);
  printf ("Quotient = %.8f\n", product);
}


void message_stuff() {
  const char *msg = "Hello Thread!";

  printf (">>>>>>  Msg printing <<<<<<<\n");
  ParVoidLoop<Iterator, Val, ArrayPtr, loop::Par> 
    (hello, 0, msg, msg, 0, strlen(msg) - 1);
};

void vanilla_stuff () {
  int i = 7;
  printf ("i (%p) = %d\n", &i, i);
  printf (">>>>>>  Incrementing i <<<<<<<\n");
  ParVoidLoop<Ref, loop::Par>  (incr, i, 0, 5);
  printf ("i (%p) = %d\n", &i, i);

  printf (">>>>>>  Setting i <<<<<<<\n");
  ParVoidLoop<Iterator, Ref, loop::Par>  (set, 0, i, 0, 5);
  printf ("i (%p) = %d\n", &i, i);

  //This will not compile
  //ParVoidLoop<Iterator, int, loop::Par>  (set, 0, i, 0, 5);

  printf (">>>>>>  Printing constant int <<<<<<<\n");
  ParVoidLoop<Val, loop::Par> (output, 3, 0, 3);

  //This will not compile
  //ParVoidLoop<int, loop::Par> (ref, 3, 0, 3);

  printf (">>>>>>  Printing Iterator <<<<<<<\n");
  ParVoidLoop<Iterator, loop::Par> (output, 0, 0, 3);

  //This will not compile
  //ParVoidLoop<Iterator, loop::Par> (ref, 0, 0, 3);
}

void big_data_stuff() {
  const int N = 100;
  printf (">>>>> Pass Big Data by Value %d iterations <<<<\n", N);
  BigData bd;
  bd.copy_count = 0;
  ParVoidLoop<Val, loop::Par> (recvData<BigData>, bd, 0, N);
  printf ("Made %d copies for %d iterations\n", bd.copy_count, N);

  printf (">>>>> Pass Big Data by Reference %d iterations <<<<\n", N);
  bd.copy_count = 0;
  ParVoidLoop<Ref, loop::Par> (recvData<BigData>, bd, 0, N);
  printf ("Made %d copies for %d iterations\n", bd.copy_count, N);
}

void my_main() {
  printf ("Hello main\n");

  array_stuff();
  message_stuff();
  vanilla_stuff();
  big_data_stuff();
}
