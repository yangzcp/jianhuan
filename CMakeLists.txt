cmake_minimum_required(VERSION 3.25)
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(jianhuan VERSION 0.1.0 LANGUAGES C CXX)

if(WIN32)
  add_compile_definitions(WINVER=0x0A00 _WIN32_WINNT=0x0A00 NTDDI_VERSION=0x0A000000)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(JIANHUAN_BUILD_TESTS "Build core tests" ON)
option(JIANHUAN_BUILD_CONSOLE "Build console demo" ON)
option(JIANHUAN_BUILD_WINDOWS_TSF "Build Windows TSF text service" ${WIN32})

set(JIANHUAN_LIBRIME_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/librime" CACHE PATH "librime source directory")
set(JIANHUAN_DEPS_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/.deps/vcpkg/x64-windows-static" CACHE PATH "Installed dependency prefix")
list(APPEND CMAKE_PREFIX_PATH "${JIANHUAN_DEPS_PREFIX}")

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC ON CACHE BOOL "" FORCE)
set(BUILD_TEST OFF CACHE BOOL "" FORCE)
set(BUILD_DATA OFF CACHE BOOL "" FORCE)
set(BUILD_SAMPLE OFF CACHE BOOL "" FORCE)
set(BUILD_SEPARATE_LIBS OFF CACHE BOOL "" FORCE)
set(ENABLE_LOGGING ON CACHE BOOL "" FORCE)
set(ENABLE_EXTERNAL_PLUGINS OFF CACHE BOOL "" FORCE)
set(ENABLE_THREADING ON CACHE BOOL "" FORCE)

add_subdirectory("${JIANHUAN_LIBRIME_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/librime" EXCLUDE_FROM_ALL)

add_library(jianhuan_core STATIC
  src/core/engine.cpp
  src/core/settings.cpp
)
target_include_directories(jianhuan_core PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src/core/include"
  "${JIANHUAN_LIBRIME_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_BINARY_DIR}/librime/src"
)
if(MSVC)
  target_compile_options(jianhuan_core PRIVATE /utf-8)
endif()
if(TARGET rime-static)
  target_link_libraries(jianhuan_core PUBLIC rime-static)
else()
  target_link_libraries(jianhuan_core PUBLIC rime)
endif()
if(WIN32)
  target_link_libraries(jianhuan_core PUBLIC bcrypt)
endif()
target_compile_definitions(jianhuan_core PRIVATE JIANHUAN_VERSION="${PROJECT_VERSION}")

if(JIANHUAN_BUILD_CONSOLE)
  add_executable(jianhuan_console tools/console_demo.cpp)
  target_link_libraries(jianhuan_console PRIVATE jianhuan_core)
  if(MSVC)
    target_compile_options(jianhuan_console PRIVATE /utf-8)
  endif()
  add_executable(jianhuan_bench tools/bench.cpp)
  target_link_libraries(jianhuan_bench PRIVATE jianhuan_core)
  if(WIN32)
    target_link_libraries(jianhuan_bench PRIVATE psapi)
  endif()
  if(MSVC)
    target_compile_options(jianhuan_bench PRIVATE /utf-8)
  endif()
endif()

if(JIANHUAN_BUILD_TESTS)
  enable_testing()
  add_executable(jianhuan_core_test tests/core_test.cpp)
  target_link_libraries(jianhuan_core_test PRIVATE jianhuan_core)
  if(MSVC)
    target_compile_options(jianhuan_core_test PRIVATE /utf-8)
  endif()
  add_test(NAME jianhuan_core_test COMMAND jianhuan_core_test "${CMAKE_CURRENT_SOURCE_DIR}/data")
endif()

if(JIANHUAN_BUILD_WINDOWS_TSF AND WIN32)
  add_subdirectory(src/platform/windows)
endif()

install(DIRECTORY data/ DESTINATION share/jianhuan)