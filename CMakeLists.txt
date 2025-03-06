cmake_minimum_required(VERSION 3.20)

project(TaskPool)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
    set(BUILD_SHARED_LIBS TRUE)
    string(REGEX REPLACE "/W[1-3]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
else ()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
endif ()

# external
add_subdirectory(external/Catch2)

# tests
option(ATZUBI_TASKPOOL_BUILD_TESTS "Build tests" 0)
if (ATZUBI_TASKPOOL_BUILD_TESTS)
    add_subdirectory(tests)
endif ()

# src
add_subdirectory(src include)

target_include_directories(TaskPool INTERFACE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        )