aux_source_directory(. SRC_LIST)

add_executable(benchmarkclient ${SRC_LIST})

target_link_libraries(benchmarkclient pthread)