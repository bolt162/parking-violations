enable_testing(true)

include_directories(../src)

add_executable(test_parser test_parser.cpp)
target_link_libraries(test_parser parking_lib)

add_executable(test_search test_search.cpp)
target_link_libraries(test_search parking_lib)
