if(MSVC)
  # The rng code isn't set up with proper symbol visibility yet.
  # Just link it statically with MSVC for now.
  add_library(qthreads_test_rng STATIC brg_sha1.c rng_brg.c)
else()
  add_library(qthreads_test_rng SHARED brg_sha1.c rng_brg.c)
endif()
