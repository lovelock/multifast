cmake_minimum_required(VERSION 3.20)

project("multifast_examples" VERSION 0.1.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(MF_VERSION_NUMBER "Version number of multifast" "2.0.0")

set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DMF_VERSION_NUMBER=${MF_VERSION_NUMBER}")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DMF_VERSION_NUMBER=${MF_VERSION_NUMBER}")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_DEBUG}")
else (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_RELEASE}")
endif()

find_package(Threads)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ahocorasick)

set(MULTIFAST_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/multifast)

add_executable(multifast ${MULTIFAST_SOURCE_DIR}/multifast.c ${MULTIFAST_SOURCE_DIR}/pattern.c
               ${MULTIFAST_SOURCE_DIR}/reader.c ${MULTIFAST_SOURCE_DIR}/strmm.c ${MULTIFAST_SOURCE_DIR}/walker.c
)



add_executable(example0 examples/example0/example0.c)
add_executable(example1 examples/example1/example1.c)
add_executable(example2 examples/example2/example2.c)
add_executable(example3 examples/example3/example3.cpp examples/example3/AhoCorasickPlus.cpp examples/example3/AhoCorasickPlus.h)
add_executable(example4 examples/example4/example4.c)
add_executable(example5 examples/example5/example5.c)
add_executable(example6 examples/example6/example6.c)
add_executable(example7 examples/example7/example7.c)

target_link_libraries(multifast ahocorasick)

target_link_libraries(example0 ahocorasick)
target_link_libraries(example1 ahocorasick)
target_link_libraries(example2 ahocorasick)
target_link_libraries(example3 ahocorasick)
target_link_libraries(example4 ahocorasick)
target_link_libraries(example5 ahocorasick Threads::Threads)
target_link_libraries(example6 ahocorasick Threads::Threads)
target_link_libraries(example7 ahocorasick)

if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tester)
    enable_testing()
    add_subdirectory(tester)
    include(CTest)
endif ()
