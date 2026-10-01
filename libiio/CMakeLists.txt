add_library(iio_adapter STATIC src/iio_sysfs.cpp)
target_include_directories(iio_adapter PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
  target_link_libraries(iio_adapter PUBLIC stdc++fs)
endif()

