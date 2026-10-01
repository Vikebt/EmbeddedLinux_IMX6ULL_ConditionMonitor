add_executable(test-condition-monitor test_condition_monitor.cpp)
target_link_libraries(test-condition-monitor PRIVATE condition)
add_test(NAME condition-monitor COMMAND test-condition-monitor)

add_test(NAME replay-fixture
  COMMAND condition-replay ${CMAKE_CURRENT_SOURCE_DIR}/data/normal_then_impact.csv)


