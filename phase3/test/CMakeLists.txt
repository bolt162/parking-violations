# Parser tests
add_executable(test_soa_parser test_soa_parser.cpp)
target_link_libraries(test_soa_parser parking_soa_lib)

# Search tests
add_executable(test_soa_search test_soa_search.cpp)
target_link_libraries(test_soa_search parking_soa_lib)
