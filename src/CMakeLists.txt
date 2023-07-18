# 定义参与编译的源代码文件 
aux_source_directory(. SRC_LIST)
# 编译生成动态库mymuduo
add_library(ReactorX SHARED ${SRC_LIST})
target_link_libraries(ReactorX ${LIB_DIR}/libThreadPool.so pthread)