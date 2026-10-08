add_executable(test-condition-monitor test_condition_monitor.cpp)
target_link_libraries(test-condition-monitor PRIVATE condition)
add_test(NAME condition-monitor COMMAND test-condition-monitor)

add_test(NAME replay-fixture
  COMMAND condition-replay ${CMAKE_CURRENT_SOURCE_DIR}/data/normal_then_impact.csv)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(test-scan-assembler test_scan_assembler.cpp)
  target_link_libraries(test-scan-assembler PRIVATE iio_adapter)
  add_test(NAME scan-assembler COMMAND test-scan-assembler)
  add_executable(test-iio-sysfs test_iio_sysfs.cpp)
  target_compile_features(test-iio-sysfs PRIVATE cxx_std_17)
  target_link_libraries(test-iio-sysfs PRIVATE iio_adapter)
  add_test(NAME iio-sysfs COMMAND test-iio-sysfs)
  add_test(NAME conditiond-simulated COMMAND conditiond --simulate --samples 30)
  if(NOT CMAKE_CROSSCOMPILING)
    find_package(PythonInterp 3 REQUIRED)
    add_test(NAME conditiond-process
      COMMAND ${PYTHON_EXECUTABLE}
              ${CMAKE_CURRENT_SOURCE_DIR}/test_conditiond_process.py
              $<TARGET_FILE:conditiond>)
  endif()
endif()

