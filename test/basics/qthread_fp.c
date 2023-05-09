#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <qthread/qthread.h>


// https://www.geeksforgeeks.org/comparison-float-value-c/
// https://dotnettutorials.net/lesson/taylor-series-using-recursion-in-c/
// https://www.studytonight.com/c/programs/important-concepts/sum-of-taylor-series

struct parts
{
  int length;
  float exp;
  float ans;
  aligned_t* cond;
};

// https://www.w3resource.com/c-programming-exercises/math/c-math-exercise-24.php
static float taylor_exponential_core(int n, float x)
{
  float exp_sum = 1;
  for (int i = n - 1; i > 0; --i)
  {
    exp_sum = 1 + x * exp_sum / i;
  }
  return exp_sum;
}

static aligned_t taylor_exponential(void *arg)
{
  struct parts* te = (struct parts*) arg;
  float exp = te->exp;
  float length = te->length;
  te->ans = taylor_exponential_core(te->length, te->exp);
  return 0;
}

static void checkFloat()
{
  void* cond = malloc(QTHREAD_SIZEOF_ALIGNED_T);
  struct parts teParts = {250,9.0f,0.0f, cond};
  int ret = -1;
  qthread_empty(teParts.cond);

  ret = qthread_fork(taylor_exponential, &teParts, teParts.cond);
  assert(ret == QTHREAD_SUCCESS);

  ret = qthread_readFF(NULL, teParts.cond);
  assert(ret == QTHREAD_SUCCESS);

  printf("value of ans = %f\n", teParts.ans);
}

int main(void)
{
  float ans = taylor_exponential_core(250, 9.0);
  printf("value of ans = %f\n", ans);
  assert(ans == 8103.083984f);

  int status = qthread_initialize();
  assert(status == QTHREAD_SUCCESS);

  checkFloat();
  return EXIT_SUCCESS;
}