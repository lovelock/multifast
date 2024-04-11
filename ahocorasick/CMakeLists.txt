cmake_minimum_required(VERSION 3.20)
project("ahocorasick" VERSION 2.0.0)

set(SOURCE_FILES actypes.h ahocorasick.c ahocorasick.h mpool.c mpool.h node.c node.h replace.c replace.h)

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
