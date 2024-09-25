[![Build Status](https://travis-ci.org/Qthreads/qthreads.svg?branch=master)](https://travis-ci.org/Qthreads/qthreads)

# WELCOME TO THE NEW HOME OF QTHREADS:
# https://github.com/sandialabs/qthreads

QTHREADS!
=========

The qthreads API is designed to make using large numbers of threads convenient
and easy. The API maps well to both MTA-style threading and PIM-style
threading, and is still quite useful in a standard SMP context. The qthreads
API also provides access to full/empty-bit (FEB) semantics, where every word of
memory can be marked either full or empty, and a thread can wait for any word
to attain either state.

The qthreads library on an SMP is essentially a library for spawning and
controlling coroutines: threads with small (4-8k) stacks. The threads are
entirely in user-space and use their locked/unlocked status as part of their
scheduling.

The library's metaphor is that there are many qthreads and several "shepherds".
Shepherds generally map to specific processors or memory regions, but this is
not an explicit part of the API. Qthreads are assigned to specific shepherds
and do not generally migrate.

The API includes utility functions for making threaded loops, sorting, and
similar operations convenient.

## Collaboration

Need help or interested in finding out more? Join us on our Slack channel: https://join.slack.com/t/qthreads/signup

## Performance

On a machine with approximately 2GB of RAM, this library was able to spawn and
handle 350,000 qthreads. With some modifications (mostly in stack-size), it was
able to handle 1,000,000 qthreads. It may be able to do more, but swapping will
become an issue, and you may start to run out of address space.

This library has been tested, and runs well, on a 64-bit machine. It is
occasionally tested on 32-bit machines, and has even been tested under Cygwin.

Currently, the only real limiting factor on the number of threads is the amount
of memory and address space you have available. For more than 2^32 threads, the
thread_id value will need to be made larger (or eliminated, as it is not
*required* for correct operation by the library itself).

For information on how to use qthread or qalloc, there is A LOT of information
in the header files (qthread.h and qalloc.h), but the primary documentation is
man pages.

