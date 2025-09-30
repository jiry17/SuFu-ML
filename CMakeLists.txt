cmake_minimum_required(VERSION 3.13)
project(ISTool)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake/)
set(THREADS_PREFER_PTHREAD_FLAG ON)


SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++17 -O3")
#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++20 -O0")

INCLUDE(FindPkgConfig)
include_directories(include)
find_package(Threads)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_definitions(-DLINUX)

    #Z3
    set(Z3_PATH Z3PATH)
    link_directories(${Z3_PATH}/build)
    set(Z3_FILE libz3.so)
    include_directories(${Z3_PATH}/src/api/c++)
    include_directories(${Z3_PATH}/src/api)

    #jsoncpp
    find_package(Jsoncpp)
    include_directories(${Jsoncpp_INCLUDE_DIR})

    #gurobi
    set(GUROBI_PATH GUROBIPATH)
    include_directories(${GUROBI_PATH}/include)
    link_directories(${GUROBI_PATH}/lib)
    set(GUROBI_FILE libgurobi_g++5.2.a libgurobi91.so)

    set(THIRDPARTY_LIBS glog gflags ${Jsoncpp_LIBRARY} ${Z3_FILE} ${GUROBI_FILE} ${PROFILER_FILE} ${CMAKE_THREAD_LIBS_INIT})
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    #z3
    set(Z3_PATH /Users/pro/Desktop/work/2025S/z3)
    set(Z3_FILE /opt/homebrew/opt/z3/lib/libz3.dylib)
    include_directories(/Users/pro/Desktop/work/2025S/z3/include/)
    message(${Z3_FILE})

    #jsoncpp
    set(Jsoncpp_LIBRARY /opt/homebrew/Cellar/jsoncpp/1.9.6/lib/libjsoncpp.dylib)
    include_directories(/opt/homebrew/Cellar/jsoncpp/1.9.6/include)

    #glog
    include_directories(/opt/homebrew/Cellar/glog/0.6.0/include/)
    set(GLOG_FILE /opt/homebrew/Cellar/glog/0.6.0/lib/libglog.dylib)

    #gflags
    include_directories(/opt/homebrew/Cellar/gflags/2.2.2/include/)
    set(GFLAGS_FILE /opt/homebrew/Cellar/gflags/2.2.2/lib/libgflags.dylib)

    #gurobi
    include_directories(/Library/gurobi1201/macos_universal2/include)
    link_directories(/Library/gurobi1201/macos_universal2/lib)
    set(GUROBI_FILE libgurobi_c++.a libgurobi120.dylib)

    # libs
    set(THIRDPARTY_LIBS ${Jsoncpp_LIBRARY} ${Z3_FILE} ${GUROBI_FILE} ${GFLAGS_FILE} ${PROFILER_FILE} ${GLOG_FILE} ${CMAKE_THREAD_LIBS_INIT})
else()
    message(FATAL_ERROR "Unexceptional system ${CMAKE_SYSTEM_NAME}")
endif()

# subdic
add_subdirectory(basic)
add_subdirectory(ext)
add_subdirectory(sygus)
add_subdirectory(solver)
add_subdirectory(incre)

set(SRC_LIBS solver_lib  ${SyGuS_LIBS} ext_lib basic_lib incre_lib)

add_subdirectory(executor)
