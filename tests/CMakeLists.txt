project(tests)

add_executable(Tests "test_TaskPool.cpp")
target_link_libraries(Tests PRIVATE TaskPool Catch2)