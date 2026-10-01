add_library(condition STATIC src/condition_monitor.cpp)
target_include_directories(condition PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

