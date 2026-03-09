#include <iostream>
#include <iomanip>
#include <sys/resource.h>
#include "parking.hpp"
#include "record.hpp"

using namespace parking;

static double get_peak_rss_mb() {
    struct rusage r;
    getrusage(RUSAGE_SELF, &r);
    return r.ru_maxrss / (1024.0 * 1024.0);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <csv_file>" << std::endl;
        return 1;
    }

    std::cout << "Phase 3 SoA Demo" << std::endl;
    std::cout << "================" << std::endl;

    ParkingAPI engine;
    engine.load(argv[1]);

    std::cout << "\nRecords: " << engine.record_count() << std::endl;
    std::cout << "Peak RSS: " << std::fixed << std::setprecision(1)
              << get_peak_rss_mb() << " MB" << std::endl;

    // Run sample queries
    std::cout << "\nSearch Results:" << std::endl;

    auto r1 = engine.find_by_date_range(20240101, 20240131);
    std::cout << "  Date 2024-01: " << r1.count() << " matches, "
              << std::setprecision(2) << r1.elapsed_ms << " ms" << std::endl;

    auto r2 = engine.find_by_violation_code(46);
    std::cout << "  Code 46: " << r2.count() << " matches, "
              << r2.elapsed_ms << " ms" << std::endl;

    uint8_t ny = state_to_enum("NY", 2);
    auto r3 = engine.find_by_state(ny);
    std::cout << "  State NY: " << r3.count() << " matches, "
              << r3.elapsed_ms << " ms" << std::endl;

    uint8_t bk = county_to_enum("BK", 2);
    auto r4 = engine.find_by_county(bk);
    std::cout << "  County BK: " << r4.count() << " matches, "
              << r4.elapsed_ms << " ms" << std::endl;

    auto r5 = engine.count_by_precinct();
    std::cout << "  Precincts: " << r5.counts.size() << " unique, "
              << r5.elapsed_ms << " ms" << std::endl;

    auto r6 = engine.count_by_fiscal_year();
    std::cout << "  Fiscal years: " << r6.counts.size() << " unique, "
              << r6.elapsed_ms << " ms" << std::endl;

    std::cout << "\nDone." << std::endl;
    return 0;
}
