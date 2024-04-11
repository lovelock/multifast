project(multifast_tests)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

add_executable(tstSearch ${CMAKE_CURRENT_SOURCE_DIR}/tstSearch.cpp ${CMAKE_CURRENT_SOURCE_DIR}/RandomString.cpp ${CMAKE_CURRENT_SOURCE_DIR}/SearchResult.cpp)
add_executable(tstChunks ${CMAKE_CURRENT_SOURCE_DIR}/tstChunks.cpp ${CMAKE_CURRENT_SOURCE_DIR}/RandomString.cpp ${CMAKE_CURRENT_SOURCE_DIR}/SearchResult.cpp)
add_executable(tstHugeData ${CMAKE_CURRENT_SOURCE_DIR}/tstHugeData.cpp ${CMAKE_CURRENT_SOURCE_DIR}/RandomString.cpp)

target_link_libraries(tstSearch ahocorasick)
target_link_libraries(tstChunks ahocorasick)
target_link_libraries(tstHugeData ahocorasick)

add_test(NAME tstSearch COMMAND tstSearch)
add_test(NAME tstChunks COMMAND tstChunks)
add_test(NAME tstHugeData COMMAND tstHugeData)