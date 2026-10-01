add_executable(condition-sim condition_sim.cpp)

add_executable(condition-replay condition_replay.cpp)
target_link_libraries(condition-replay PRIVATE condition)


