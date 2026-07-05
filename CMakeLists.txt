cmake_minimum_required(VERSION 3.21)

project(
  PGOV
  VERSION 1.0
  LANGUAGES C)

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

file(GLOB_RECURSE PG_SOURCES CONFIGURE_DEPENDS "src/*.c")
list(REMOVE_ITEM PG_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c")

add_library(pgov STATIC ${PG_SOURCES})
target_include_directories(pgov PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_compile_options(pgov PRIVATE -Wall -Wextra -Werror)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  target_compile_options(
    pgov
    PRIVATE -O2
            -ffunction-sections
            -fdata-sections
            -fvisibility=hidden
            -fno-math-errno
            -fno-signed-zeros
            -fno-trapping-math)

  if(LTO_SUPPORTED)
    set_property(TARGET pgov PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()

else()
  target_compile_definitions(pgov PRIVATE DEBUG)
endif()

add_executable(pgovd "src/main.c")

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  target_link_options(
    pgovd
    PRIVATE
    -Wl,--gc-sections
    -Wl,--icf=all
    -Wl,-s
    -Wl,--build-id=none
    -Wl,-O2
    -Wl,--as-needed)

  if(LTO_SUPPORTED)
    set_property(TARGET pgovd PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()
endif()

target_link_libraries(pgovd PRIVATE pgov android log m)
