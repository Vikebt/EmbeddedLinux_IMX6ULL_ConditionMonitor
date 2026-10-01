cmake_minimum_required(VERSION 3.10)
project(imx6ull_condition_monitor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

include(CTest)
add_subdirectory(libcondition)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_subdirectory(libiio)
endif()
add_subdirectory(apps)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

