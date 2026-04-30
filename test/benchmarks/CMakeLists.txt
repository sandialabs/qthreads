# Allow benchmarks to access qthreads internal headers.
# In particular, qt_atomic_wait.h provides platform-generic wrappers around futexes.
include_directories("../../include")

set(CMAKE_CXX_STANDARD 26)

qthreads_test_cpp(atomic_accumulate)
qthreads_test_cpp(atomic_fetch_min)
qthreads_test_cpp(atomic_fetch_sub)
qthreads_test_cpp(function_call)
qthreads_test_cpp(function_call_unpredicted)
qthreads_test_cpp(nemesis_many_to_one)
qthreads_test_cpp(os_thread_fork_join)
qthreads_test_cpp(os_thread_futex_handoff)
qthreads_test(swapcontext)
qthreads_test(syscall)
