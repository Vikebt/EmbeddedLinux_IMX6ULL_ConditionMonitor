add_executable(condition-sim condition_sim.cpp)
install(TARGETS condition-sim RUNTIME DESTINATION bin)

add_executable(condition-replay condition_replay.cpp)
target_link_libraries(condition-replay PRIVATE condition)
install(TARGETS condition-replay RUNTIME DESTINATION bin)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(conditiond conditiond.cpp)
  target_link_libraries(conditiond PRIVATE condition iio_adapter)
  install(TARGETS conditiond RUNTIME DESTINATION sbin)
endif()


