# Qthread Task Yield

A task can relinquish control of an execution context by *yielding*.
When doing so, the runtime puts the task back into a ready queue.
This document covers the different states of yielding and how those effect where the task is enqueued.

## Qthread task yield states:

1. *Near*: set the task state to indicate it was yielded near.
   If using spawn cache, try to enqueue in local spawn cache.
   If not using spawn cache, or local enqueue is not supported by scheduler, swap this task with first task in queue.
   Note this is done by dequeueing next task then enqueueing this task followed by next task, using `qt_threadqueue_enqueue()`.
2. *Far*: set the task state to indicate it was yielded far.
   Enqueue this task using `qt_threadqueue_enqueue_yielded()`, which will place the task "farthest" from the worker, when there is such a concept.
3. *Direct*: when not using spawn cache, does not alter task state.
   When using spawn cache, if there is no cached task, does not alter task state.
   If there is a simple or not-new task at the head of the cache, then just do a yield *far*.
   Otherwise, create new task data for cached task and directly swap into that task.
4. *Stealable*: set the task to be stealable
5. *Unstealable*: set the task to be unstealable

See `include/qt_yield.h` for definition of yield states.

## Internal API

The `qthread_yield_(flags)` function is the internal yield call.
See `src/qthread.c` for definition of `qthread_yield_()`.

The `qt_loop_spawner()` function uses `qthread_yield_()`.
That function is called internal to `qt_loop()`.
If the loop is a "simple" loop, then tasks are yielded *near*.
Otherwise, tasks are yielded *far*.

## External API

A call to `qthread_yield()` is equivalent to `qthread_yield_(QTHREAD_YIELD_FAR)`.
A call to `qthread_yield_near()` is equivalent to `qthread_yield_(QTHREAD_YIELD_NEAR)`.

