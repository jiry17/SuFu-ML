cmake_minimum_required(VERSION 3.5)
project(ISToolExecutor)
cmake_policy(SET CMP0079 NEW)
aux_source_directory(invoker INVOKER_SRC)
add_library(invoker_lib STATIC ${INVOKER_SRC} invoker/invoker.cpp)
set(ALL_LIBS ${SRC_LIBS} invoker_lib)

add_executable(run run_incre_label.cpp)
target_link_libraries(run ${ALL_LIBS} ${ALL_LIBS} ${THIRDPARTY_LIBS})

add_executable(eval eval_program.cpp)
target_link_libraries(eval ${ALL_LIBS} ${aLL_LIBS} ${THIRDPARTY_LIBS})

add_executable(run_rust rust_trans.cpp)
target_link_libraries(run_rust ${ALL_LIBS} ${aLL_LIBS} ${THIRDPARTY_LIBS})