set(QTHREADS_SCHEDULER nemesis CACHE STRING "Which scheduler to use for qthreads. Valid options are nemesis, sherwood, and distrib.")
set(QTHREADS_TOPOLOGY no CACHE STRING "Which topology detection/management system to use for qthreads. Valid options are no, hwloc, and binders.")
set(QTHREADS_BARRIER feb CACHE STRING "Which barrier implementation to use for qthreads. Valid options are feb, sinc, array, and log.")
set(QTHREADS_SINC donecount CACHE STRING "Which sinc implementation to use for qthreads. Valid options are donecount, donecoutn_cas, snzi, and original.")
set(QTHREADS_ALLOC base CACHE STRING "Wich allocation implementation to use for qthreads. Valid options are base, and chapel.")
set(QTHREADS_CACHELINE_SIZE_ESTIMATE 64 CACHE STRING "Estimate of the cacheline size of the target machine (used for optimizing data structure layouts).")
set(QTHREADS_DEFAULT_STACK_SIZE 32768 CACHE STRING "Default qthread stack size.")
set(QTHREADS_HASHMAP hashmap CACHE STRING "Which hashmap implementation to use. Valid values are \"hashmap\" and \"lf_hashmap\".")
set(QTHREADS_DICT_TYPE shavit CACHE STRING "Which dictionary implementation to use. Valid values are \"shavit\", \"trie\", and \"simple\".")
set(QTHREADS_TIMER_TYPE gettimeofday CACHE STRING "Which timer implementation to use. Valid values are \"clock_gettime\", \"mach\", \"gettimeofday\", and \"gethrtime\".")

set(QTHREADS_SOURCES
  cacheline.c
  envariables.c
  feb.c
  hazardptrs.c
  io.c
  locks.c
  qalloc.c
  qloop.c
  queue.c
  barrier/${QTHREADS_BARRIER}.c
  qutil.c
  syncvar.c
  qthread.c
  mpool.c
  shepherds.c
  workers.c
  threadqueues/${QTHREADS_SCHEDULER}_threadqueues.c
  sincs/${QTHREADS_SINC}.c
  alloc/${QTHREADS_ALLOC}.c
  affinity/common.c
  affinity/${QTHREADS_TOPOLOGY}.c
  touch.c
  tls.c
  teams.c
  fastcontext/asm.S
  fastcontext/context.c
  ${QTHREADS_HASHMAP}.c
  ds/qarray.c
  ds/qdqueue.c
  ds/qlfqueue.c
  ds/qpool.c
  ds/qswsrqueue.c
  ds/dictionary/hash.c
  ds/dictionary/dictionary_${QTHREADS_DICT_TYPE}.c
  qtimer/${QTHREADS_TIMER_TYPE}.c
  syscalls/accept.c
  syscalls/connect.c
  syscalls/poll.c
  syscalls/pread.c
  syscalls/pwrite.c
  syscalls/read.c
  syscalls/select.c
  syscalls/system.c
  syscalls/user_defined.c
  syscalls/wait4.c
  syscalls/write.c
  patterns/allpairs.c
  patterns/wavefront.c
)

# TODO: switch/checks necessary to include the correct
# fastcontext version when we need the fallback.

add_library(qthread SHARED ${QTHREADS_SOURCES})
target_include_directories(qthread
  PRIVATE "../include"
)
set_target_properties(qthread PROPERTIES C_VISIBILITY_PRESET hidden)
target_link_libraries(qthread PUBLIC Threads::Threads)
target_include_directories(qthread
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/qthread>
)
# TODO: move these into a configure header instead of piping them through the flags.
target_compile_definitions(qthread
  PRIVATE CACHELINE_WIDTH=${QTHREADS_CACHELINE_SIZE_ESTIMATE}
  PRIVATE QTHREAD_DEFAULT_STACK_SIZE=${QTHREADS_DEFAULT_STACK_SIZE}
)

#file(GLOB QTHREADS_INTERNAL_HEADERS "../include/*.h")
file(GLOB QTHREADS_PUBLIC_HEADERS "../include/qthread/*.h*")

target_sources(qthread
  PUBLIC
    FILE_SET qthreads_public_headers
    TYPE HEADERS
    BASE_DIRS "../include"
    FILES ${QTHREADS_PUBLIC_HEADERS}
)

install(
  TARGETS qthread
  EXPORT qthread
  LIBRARY
    DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    COMPONENT shlib
  FILE_SET qthreads_public_headers
)
install(EXPORT qthread DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/qthread")
