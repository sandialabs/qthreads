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
  aligned_t cond;
};

// https://www.w3resource.com/c-programming-exercises/math/c-math-exercise-24.php
static float taylor_exponential_core(int n, float x)
{
  float exp_sum = 1;
  for (int i = n - 1; i > 0; --i)
  {
    exp_sum = 1 + x * exp_sum / i;
    qthread_yield();
  }
  return exp_sum;
}

static aligned_t taylor_exponential(void *arg)
{
  struct parts *te = (struct parts *)arg;
  float exp = te->exp;
  float length = te->length;
  te->ans = taylor_exponential_core(te->length, te->exp);
  return 0;
}

static aligned_t checkFloatAsQthreads()
{
  struct parts teParts = {250, 9.0f, 0.0f};
  qthread_empty(&teParts.cond);

  struct parts teParts1 = {50, 9.0f, 0.0f};
  qthread_empty(&teParts1.cond);
  
  struct parts teParts2 = {9, 9.0f, 0.0f};
  qthread_empty(&teParts2.cond);

  return 0;

}

static void checkFloatAsQthread()
{
  int ret = -1;
  struct parts teParts = {250, 9.0f, 0.0f};
  qthread_empty(&teParts.cond);

  ret = qthread_fork(taylor_exponential, &teParts, &teParts.cond);
  assert(ret == QTHREAD_SUCCESS);

  ret = qthread_readFF(NULL, &teParts.cond);
  assert(ret == QTHREAD_SUCCESS);

  assert(teParts.ans == 8103.083984f);
}

static void checkFloat()
{
  float ans = taylor_exponential_core(250, 9.0f);
  assert(ans == 8103.083984f);
}

int main(void)
{
  checkFloat();

  int status = qthread_initialize();
  assert(status == QTHREAD_SUCCESS);
  checkFloatAsQthread();
  
  return EXIT_SUCCESS;
}