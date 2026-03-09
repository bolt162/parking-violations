// Basic usage example for the Parking Violations Library
//
// Build: Link against parking_soa_lib
// Run:   ./basic_usage <csv_file>

#include <iostream>
#include "parking.hpp"
#include "record.hpp"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <csv_file>" << std::endl;
        return 1;
    }

    // Create the parking violations engine
    parking::ParkingAPI engine;

    // Load CSV data
    std::cout << "Loading data..." << std::endl;
    size_t count = engine.load(argv[1]);
    std::cout << "Loaded " << count << " records" << std::endl;

    // Example 1: Search by date range
    std::cout << "\n--- Date Range Search ---" << std::endl;
    auto jan_2024 = engine.find_by_date_range(20240101, 20240131);
    std::cout << "Violations in Jan 2024: " << jan_2024.count() << std::endl;

    // Example 2: Search by violation code
    std::cout << "\n--- Violation Code Search ---" << std::endl;
    auto code_46 = engine.find_by_violation_code(46);
    std::cout << "Code 46 violations: " << code_46.count() << std::endl;

    // Example 3: Search by state
    std::cout << "\n--- State Search ---" << std::endl;
    uint8_t ny = parking::state_to_enum("NY", 2);
    auto ny_violations = engine.find_by_state(ny);
    std::cout << "NY registered vehicles: " << ny_violations.count() << std::endl;

    // Example 4: Search by county
    std::cout << "\n--- County Search ---" << std::endl;
    uint8_t bk = parking::county_to_enum("BK", 2);
    auto brooklyn = engine.find_by_county(bk);
    std::cout << "Brooklyn violations: " << brooklyn.count() << std::endl;

    // Example 5: Aggregation by precinct
    std::cout << "\n--- Top 5 Precincts ---" << std::endl;
    auto by_precinct = engine.count_by_precinct();
    int shown = 0;
    for (auto& entry : by_precinct.counts) {
        if (shown++ >= 5) break;
        std::cout << "  Precinct " << entry.first << ": " << entry.second << std::endl;
    }

    // Example 6: Aggregation by fiscal year
    std::cout << "\n--- Violations by Fiscal Year ---" << std::endl;
    auto by_year = engine.count_by_fiscal_year();
    for (auto& entry : by_year.counts) {
        std::cout << "  FY " << entry.first << ": " << entry.second << std::endl;
    }

    return 0;
}
