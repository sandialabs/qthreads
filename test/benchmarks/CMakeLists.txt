# Allow benchmarks to access qthreads internal headers.
# In particular, qt_atomic_wait.h provides platform-generic wrappers around futexes.
include_directories("../../include")

qthreads_test_cpp(os_thread_fork_join)
qthreads_test_cpp(os_thread_futex_handoff)
qthreads_test(swapcontext)
qthreads_test(syscall)
