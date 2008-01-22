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

aligned_t qmain(qthread_t *qthr, void *arg) {
  main_args_s *a = (main_args_s*)arg;
  int argc = a->argc;
  char **argv = a->argv;
  future_init(FUTURE_PER_LOC());

  my_main();

  pthread_mutex_unlock(&all_done);

  return 0;
}

int main (int argc, char **argv) {
  qthread_init(NUM_LOCS());
  main_args_s a;
  a.argc = argc;
  a.argv = argv;
  
  pthread_mutex_lock(&all_done);
  qthread_fork(qmain, &a, NULL);
  pthread_mutex_lock(&all_done);
  qthread_finalize();
}

void hello (int i, const char* msg, char c) {
  printf ("%s %3d %3c\n", msg, i, c);
}

void incr(int& i) {
  qthread_t *me = qthread_self();
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
int genericArraySet (ArrayT& arr, int size, char* name) {
  printf (">>>>>>  Array setting %s <<<<<<<\n", name);
  mt_loop<Iterator, ArrayPtr, mt_loop_traits::Par> (set, 0, arr, 0, size);
  return 1;
};

template <class ArrayT>
void genericArrayPrint (ArrayT& arr, int size, char *name) {
  printf (">>>>>>  Array printing %s <<<<<<<\n", name);
  mt_loop<ArrayPtr, mt_loop_traits::Par> (output, arr, 0, size);
  printf (">>>>>>  Array printing double by value %s <<<<<<<\n", name);
  mt_loop<ArrayPtr, mt_loop_traits::Par> (output_double, arr, 0, size);
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
  mt_loop_returns <ArrayPtr, ArrayPtr, mt_loop_traits::Par>
    (new_arr, assign, arr, 0, size);
  genericArrayPrint(new_arr, size, "Array Copy");

  int sum = 0;
  printf (">>>>> Adding Array <<<<<\n");
  mt_loop_returns <Collect<mt_loop_traits::Add>, ArrayPtr, mt_loop_traits::Par>
    (sum, assign, arr, 0, size);
  printf ("Sum = %d\n", sum);

  sum = 0;
  printf (">>>>> Subbing Array <<<<<\n");
  mt_loop_returns <Collect<mt_loop_traits::Sub>, ArrayPtr, mt_loop_traits::Par>
    (sum, assign, arr, 0, size);
  printf ("Diff = %d\n", sum);

  double product = 1;
  printf (">>>>> Multiplying first 10 in Array <<<<<\n");
  mt_loop_returns <Collect<mt_loop_traits::Mult>, ArrayPtr, mt_loop_traits::Par>
    (product, assign, arr, 0, 10);
  printf ("Product = %f\n", product);

  product = 1;
  printf (">>>>> Dividing by first 5 in Array <<<<<\n");
  mt_loop_returns <Collect<mt_loop_traits::Div>, ArrayPtr, mt_loop_traits::Par>
    (product, assign, arr, 0, 5);
  printf ("Quotient = %.8f\n", product);
}


void message_stuff() {
  const char *msg = "Hello Thread!";

  printf (">>>>>>  Msg printing <<<<<<<\n");
  mt_loop<Iterator, Val, ArrayPtr, mt_loop_traits::Par> 
    (hello, 0, msg, msg, 0, strlen(msg) - 1);
};

void vanilla_stuff () {
  int i = 7;
  printf ("i (%p) = %d\n", &i, i);
  printf (">>>>>>  Incrementing i <<<<<<<\n");
  mt_loop<Ref, mt_loop_traits::Par>  (incr, i, 0, 5);
  printf ("i (%p) = %d\n", &i, i);

  printf (">>>>>>  Setting i <<<<<<<\n");
  mt_loop<Iterator, Ref, mt_loop_traits::Par>  (set, 0, i, 0, 5);
  printf ("i (%p) = %d\n", &i, i);

  //This will not compile
  //mt_loop<Iterator, Val, mt_loop_traits::Par>  (set, 0, i, 0, 5);

  printf (">>>>>>  Printing constant int <<<<<<<\n");
  mt_loop<Val, mt_loop_traits::Par> (output, 3, 0, 3);

  //This will not compile
  //mt_loop<Val, mt_loop_traits::Par> (ref, 3, 0, 3);

  printf (">>>>>>  Printing Iterator <<<<<<<\n");
  mt_loop<Iterator, mt_loop_traits::Par> (output, 0, 0, 3);

  //This will not compile
  //mt_loop<Iterator, mt_loop_traits::Par> (ref, 0, 0, 3);
}

void big_data_stuff() {
  const int N = 100;
  printf (">>>>> Pass Big Data by Value %d iterations <<<<\n", N);
  BigData bd;
  bd.copy_count = 0;
  mt_loop<Val, mt_loop_traits::Par> (recvData<BigData>, bd, 0, N);
  printf ("Made %d copies for %d iterations\n", bd.copy_count, N);

  printf (">>>>> Pass Big Data by Reference %d iterations <<<<\n", N);
  bd.copy_count = 0;
  mt_loop<Ref, mt_loop_traits::Par> (recvData<BigData>, bd, 0, N);
  printf ("Made %d copies for %d iterations\n", bd.copy_count, N);
}

class add {
  int a_;
public:
  add(int a) : a_(a) {;}
  int operator()(int b) { return b + a_; }
};

class sub {
  int s_;
public:
  sub(int s) : s_(s) {;}
  int operator()(int b) { return b - s_; }
};

template <class OpT>
void class_stuff (int value, OpT op, int times) {
  int results[3];
  mt_mfun_loop_returns<ArrayPtr, Val, mt_loop_traits::Par>
    (&op, (int*)results, &OpT::operator(), value, 0, 3);

  for (int i = 0; i < 3; i++) {
    printf ("[testq] Result = %d\n", results[i]);
  }

  int sum = 0;
  mt_mfun_loop_returns<Collect<mt_loop_traits::Add>, Val, mt_loop_traits::Par>
    (&op, sum, &OpT::operator(), value, 0, times);

  printf ("[testq] Sum of Result (%d times) %d\n", times, sum);
}

void my_main() {
  printf ("[testq] Hello main\n");

  //array_stuff();
  //message_stuff();
  //vanilla_stuff();
  //big_data_stuff();
  class_stuff<add>(10, 4, 3);
  class_stuff<sub>(10, 4, 7);
}
