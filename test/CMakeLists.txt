set(CMAKE_CXX_STANDARD 11)

set(QTHREADS_BUILD_TESTS ON CACHE BOOL "Whether or not to build the qthreads tests.")

if (${QTHREADS_BUILD_TESTS})
  include_directories("." "utils/rng")

  add_subdirectory(utils/rng)

  function(qthreads_test name)
    add_executable(${name} "${name}.c")
    target_link_libraries(${name} qthread)
    target_link_libraries(${name} qthreads_test_rng)
    if(NOT MSVC)
      target_link_libraries(${name} m)
    endif()
    add_test(NAME ${name} COMMAND ${name} ${ARGN})
    set_property(TEST ${name} PROPERTY ENVIRONMENT "QT_NUM_SHEPHERDS=2;QT_NUM_WORKERS_PER_SHEPHERD=1")
    if(WIN32)
      set_property(TEST ${name} PROPERTY ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:qthread>"
        "PATH=path_list_append:$<TARGET_FILE_DIR:qthreads_test_rng>")
    else()
      # Prioritize the built libqthread.so for testing over any previous installed one.
      set_property(TEST ${name} PROPERTY ENVIRONMENT_MODIVICATION
        "LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:qthread>")
    endif()
    set_property(TEST ${name} PROPERTY C_STANDARD "C11")
  endfunction()

  function(qthreads_test_cpp name)
    add_executable(${name} "${name}.cpp")
    target_link_libraries(${name} qthread)
    target_link_libraries(${name} qthreads_test_rng)
    if(NOT MSVC)
      target_link_libraries(${name} m)
    endif()
    add_test(NAME ${name} COMMAND ${name} ${ARGN})
    set_property(TEST ${name} PROPERTY ENVIRONMENT "QT_NUM_SHEPHERDS=2;QT_NUM_WORKERS_PER_SHEPHERD=1")
    if(WIN32)
      set_property(TEST ${name} PROPERTY ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:qthread>"
        "PATH=path_list_append:$<TARGET_FILE_DIR:qthreads_test_rng>")
    else()
      # Prioritize the built libqthread.so for testing over any previous installed one.
      set_property(TEST ${name} PROPERTY ENVIRONMENT_MODIVICATION
        "LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:qthread>")
    endif()
    set_property(TEST ${name} PROPERTY CXX_STANDARD "17")
  endfunction()

  add_subdirectory(basics)
  add_subdirectory(features)
  add_subdirectory(internal)
  add_subdirectory(stress)
endif()
