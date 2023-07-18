aux_source_directory(. SRC_LIST)

add_executable(testserver ${SRC_LIST})

target_link_libraries(testserver ${LIB_DIR}/libReactorX.so ${LIB_DIR}/libThreadPool.so)