cmake_minimum_required(VERSION 3.0)
project(ReactorX)

# cmake => makefile   make
# mymuduo最终编译成so动态库，设置动态库的路径，放在根目录的lib文件夹下面
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# 设置库文件的路径
set(LIB_DIR ${PROJECT_SOURCE_DIR}/lib)

# 设置调试信息 以及 启动C++11语言标准
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/include/threadpool)

add_subdirectory(src)

add_subdirectory(example)