cmake_minimum_required(VERSION 3.15)
project(brain_core CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_executable(brain
  src/main.cpp
  src/neurons.cpp
  src/net.cpp
  src/io_logger.cpp
  src/hormones.cpp
)

target_include_directories(brain PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
