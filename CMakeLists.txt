cmake_minimum_required(VERSION 3.12...4.1)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_C_STANDARD 11)
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build using shared libraries.")

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.9.0")
  project(qthreads
    VERSION 1.23
    DESCRIPTION "A user-level threading library"
    LANGUAGES C CXX)
else()
  project(qthreads
    VERSION 1.23
    LANGUAGES C CXX)
endif()

# Have to detect the C Compiler first to determine which flavor of asm to use.
if(MSVC)
  # This is an imperfect way of getting insight into the target architecture,
  # but it's good enough for the case of distinguishing between AMD64 and ARM64 on Windows.
  if(CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64")
    enable_language(ASM_MASM)
  elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "ARM64")
    if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
      # There are currently compatibility issues between clang-cl and armasm64.
      # Similarly, llvm-ml is only on x86/x64.
      enable_language(ASM)
    else()
      enable_language(ASM_MARMASM)
    endif()
  else()
    message(FATAL_ERROR "Unknown Windows Architecture.")
  endif()
else()
  enable_language(ASM)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if(MSVC AND NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
  add_compile_options($<$<COMPILE_LANGUAGE:C>:/experimental:c11atomics>)
endif()

add_subdirectory(src)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  include(CTest)
  add_subdirectory(test)
endif()

set(QTHREADS_MINIMUM_PORTABLE OFF CACHE BOOL "Build a heavily restricted subset of Qthreads, useful debugging/developing support for new platforms.")
