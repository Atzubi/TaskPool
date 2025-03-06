add_library(TaskPool STATIC "TaskPool.cpp")
target_include_directories(TaskPool PRIVATE "../include")
file(GLOB_RECURSE LIBRARY_HEADERS "../include/*.h")
target_sources(TaskPool PRIVATE  ${LIBRARY_HEADERS} )