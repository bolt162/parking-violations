#include <iostream>
#include "soa_store.hpp"
#include "soa_search.hpp"

using namespace parking;

int main() {
    std::cout << "Testing SoA search..." << std::endl;

    // Create small test data
    SoADataStore store;
    store.resize(5);

    store.issue_dates[0] = 20240101;
    store.issue_dates[1] = 20240115;
    store.issue_dates[2] = 20240201;
    store.issue_dates[3] = 20240115;
    store.issue_dates[4] = 20240301;

    store.violation_codes[0] = 46;
    store.violation_codes[1] = 46;
    store.violation_codes[2] = 99;
    store.violation_codes[3] = 46;
    store.violation_codes[4] = 21;

    store.registration_states[0] = 1;  // NY
    store.registration_states[1] = 1;
    store.registration_states[2] = 2;  // NJ
    store.registration_states[3] = 1;
    store.registration_states[4] = 1;

    store.record_count = 5;

    // Test searches
    auto r1 = search_by_date_range(store, 20240101, 20240131);
    std::cout << "Date range 2024-01: " << r1.count() << " matches (expected 3)" << std::endl;

    auto r2 = search_by_violation_code(store, 46);
    std::cout << "Code 46: " << r2.count() << " matches (expected 3)" << std::endl;

    auto r3 = search_by_state(store, 1);
    std::cout << "State NY: " << r3.count() << " matches (expected 4)" << std::endl;

    bool pass = (r1.count() == 3 && r2.count() == 3 && r3.count() == 4);
    std::cout << (pass ? "\nAll tests passed." : "\nSome tests failed!") << std::endl;

    return pass ? 0 : 1;
}
