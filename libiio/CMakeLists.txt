add_library(iio_adapter STATIC src/iio_sysfs.cpp)
target_include_directories(iio_adapter PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

