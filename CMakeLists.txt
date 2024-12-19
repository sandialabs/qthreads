cmake_minimum_required(VERSION 3.30)

set(CMAKE_C_STANDARD 11)

project(qthreads
  VERSION 1.22
  DESCRIPTION "A user-level threading library"
  LANGUAGES C ASM)

include(GNUInstallDirs)

add_subdirectory(src)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  include(CTest)
  add_subdirectory(test)
endif()

