cmake_minimum_required(VERSION 3.21)

project(PGOV LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE
      Release
      CACHE STRING "Build type" FORCE)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Android")
  find_program(
    CLANG_TIDY_EXE
    NAMES clang-tidy clang-tidy.exe
    PATHS "${ANDROID_TOOLCHAIN_ROOT}/bin"
    NO_DEFAULT_PATH)

  if(CLANG_TIDY_EXE)
    set(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_EXE}")
    message(STATUS "Found clang-tidy: ${CLANG_TIDY_EXE}")
  endif()

endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT LTO_SUPPORTED OUTPUT LTO_ERROR)

file(GLOB_RECURSE PGOV_SOURCES CONFIGURE_DEPENDS "src/*.c")
list(REMOVE_ITEM PGOV_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c")

add_library(pgov STATIC ${PGOV_SOURCES})
target_include_directories(pgov PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_compile_definitions(pgov PUBLIC NDK_BUILD)

target_compile_options(
  pgov PUBLIC
  -Wall
  -Wextra
  -Werror)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  target_compile_options(
    pgov PUBLIC
    -O2
    -ffunction-sections
    -fdata-sections
    -fvisibility=hidden)

  if(LTO_SUPPORTED)
    set_property(TARGET pgov PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()

else()
  target_compile_definitions(pgov PUBLIC DEBUG)
endif()

add_executable(pgovd "src/main.c")

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  target_link_options(
    pgovd PRIVATE
    -Wl,--gc-sections
    -Wl,--icf=all
    -Wl,-s
    -Wl,--build-id=none
    -Wl,--as-needed
    -Wl,-z,max-page-size=16384)

  if(LTO_SUPPORTED)
    set_property(TARGET pgovd PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()
endif()

target_link_libraries(pgovd PRIVATE pgov log)
