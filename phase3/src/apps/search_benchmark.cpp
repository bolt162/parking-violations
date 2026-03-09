#include <iostream>
#include <iomanip>
#include "parking.hpp"
#include "record.hpp"
#include "benchmark_harness.hpp"

using namespace parking;
using namespace parking::benchmark;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <csv_file> [iterations]" << std::endl;
        return 1;
    }

    std::string filepath = argv[1];
    int iterations = (argc >= 3) ? std::atoi(argv[2]) : 12;

    std::cout << "Phase 3 SoA Search Benchmark" << std::endl;
    std::cout << "============================" << std::endl;

    ParkingAPI engine;
    engine.load(filepath);

    std::cout << "\nRecords: " << engine.record_count() << std::endl;
    std::cout << "Iterations per query: " << iterations << std::endl;
    std::cout << "\nRunning benchmarks..." << std::endl;

    // B1: Date narrow
    {
        auto times = run_benchmark([&]() {
            engine.find_by_date_range(20240101, 20240131);
        }, iterations);
        auto stats = compute_stats(times);
        auto res = engine.find_by_date_range(20240101, 20240131);
        std::cout << "B1 date_narrow: " << res.count() << " matches, ";
        print_stats("", stats);
    }

    // B2: Date wide
    {
        auto times = run_benchmark([&]() {
            engine.find_by_date_range(20240101, 20241231);
        }, iterations);
        auto stats = compute_stats(times);
        auto res = engine.find_by_date_range(20240101, 20241231);
        std::cout << "B2 date_wide: " << res.count() << " matches, ";
        print_stats("", stats);
    }

    // B3: Code common
    {
        auto times = run_benchmark([&]() {
            engine.find_by_violation_code(46);
        }, iterations);
        auto stats = compute_stats(times);
        auto res = engine.find_by_violation_code(46);
        std::cout << "B3 code_46: " << res.count() << " matches, ";
        print_stats("", stats);
    }

    // B4: Code rare
    {
        auto times = run_benchmark([&]() {
            engine.find_by_violation_code(99);
        }, iterations);
        auto stats = compute_stats(times);
        auto res = engine.find_by_violation_code(99);
        std::cout << "B4 code_99: " << res.count() << " matches, ";
        print_stats("", stats);
    }

    // B6: State common
    {
        uint8_t ny = state_to_enum("NY", 2);
        auto times = run_benchmark([&]() {
            engine.find_by_state(ny);
        }, iterations);
        auto stats = compute_stats(times);
        auto res = engine.find_by_state(ny);
        std::cout << "B6 state_NY: " << res.count() << " matches, ";
        print_stats("", stats);
    }

    // B7: State rare
    {
        uint8_t fl = state_to_enum("FL", 2);
        auto times = run_benchmark([&]() {
            engine.find_by_state(fl);
        }, iterations);
        auto stats = compute_stats(times);
        auto res = engine.find_by_state(fl);
        std::cout << "B7 state_FL: " << res.count() << " matches, ";
        print_stats("", stats);
    }

    // B8: County
    {
        uint8_t bk = county_to_enum("BK", 2);
        auto times = run_benchmark([&]() {
            engine.find_by_county(bk);
        }, iterations);
        auto stats = compute_stats(times);
        auto res = engine.find_by_county(bk);
        std::cout << "B8 county_BK: " << res.count() << " matches, ";
        print_stats("", stats);
    }

    // B9: Precinct aggregation
    {
        auto times = run_benchmark([&]() {
            engine.count_by_precinct();
        }, iterations);
        auto stats = compute_stats(times);
        auto res = engine.count_by_precinct();
        std::cout << "B9 precinct_agg: " << res.counts.size() << " precincts, ";
        print_stats("", stats);
    }

    // B10: Fiscal year aggregation
    {
        auto times = run_benchmark([&]() {
            engine.count_by_fiscal_year();
        }, iterations);
        auto stats = compute_stats(times);
        auto res = engine.count_by_fiscal_year();
        std::cout << "B10 fy_agg: " << res.counts.size() << " years, ";
        print_stats("", stats);
    }

    std::cout << "\nPeak RSS: " << get_peak_rss_mb() << " MB" << std::endl;
    std::cout << "Done." << std::endl;

    return 0;
}
