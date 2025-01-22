cmake_minimum_required(VERSION 3.2)

set(CMAKE_C_STANDARD 11)

if(CMAKE_VERSION VERSION_LESS "3.9.0")
  project(qthreads
    VERSION 1.22
    DESCRIPTION "A user-level threading library"
    LANGUAGES C ASM CXX)
else()
  project(qthreads
    VERSION 1.22
    LANGUAGES C ASM CXX)
endif()

include(GNUInstallDirs)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_subdirectory(src)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  include(CTest)
  add_subdirectory(test)
endif()

