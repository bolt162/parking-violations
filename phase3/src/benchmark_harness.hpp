#ifndef PARKING_BENCHMARK_HARNESS_HPP
#define PARKING_BENCHMARK_HARNESS_HPP

#include <chrono>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <iostream>
#include <iomanip>
#include <functional>
#include <sys/resource.h>

namespace parking::benchmark {

/// Statistics computed from multiple benchmark runs.
struct Stats {
    double mean     = 0.0;
    double stddev   = 0.0;
    double min_val  = 0.0;
    double max_val  = 0.0;
    double median   = 0.0;
    size_t n        = 0;
};

/// Compute mean, stddev, min, max, median from a vector of doubles.
inline Stats compute_stats(std::vector<double> values) {
    Stats s;
    s.n = values.size();
    if (s.n == 0) return s;

    std::sort(values.begin(), values.end());
    s.min_val = values.front();
    s.max_val = values.back();

    if (s.n % 2 == 0) {
        s.median = (values[s.n / 2 - 1] + values[s.n / 2]) / 2.0;
    } else {
        s.median = values[s.n / 2];
    }

    double sum = std::accumulate(values.begin(), values.end(), 0.0);
    s.mean = sum / s.n;

    double sq_sum = 0.0;
    for (double v : values) {
        sq_sum += (v - s.mean) * (v - s.mean);
    }
    s.stddev = (s.n > 1) ? std::sqrt(sq_sum / (s.n - 1)) : 0.0;

    return s;
}

/// Print a Stats object in a formatted table row.
inline void print_stats(const std::string& label, const Stats& s,
                        const std::string& unit = "ms") {
    std::cout << "  " << std::left << std::setw(22) << label
              << std::right << std::fixed
              << "  mean=" << std::setw(10) << std::setprecision(2) << s.mean
              << " " << unit
              << "  std=" << std::setw(8) << std::setprecision(2) << s.stddev
              << "  min=" << std::setw(10) << std::setprecision(2) << s.min_val
              << "  max=" << std::setw(10) << std::setprecision(2) << s.max_val
              << "  median=" << std::setw(10) << std::setprecision(2) << s.median
              << "  (n=" << s.n << ")"
              << std::endl;
}

/// Print stats as a CSV line (for machine-readable output).
inline void print_stats_csv(const std::string& label, const Stats& s,
                            std::ostream& out = std::cout) {
    out << label << ","
        << std::fixed << std::setprecision(4)
        << s.mean << "," << s.stddev << ","
        << s.min_val << "," << s.max_val << ","
        << s.median << "," << s.n
        << std::endl;
}

/// Run a callable `iterations` times, collecting elapsed time (ms) per run.
/// The first `warmup` runs are discarded.
/// Returns vector of measured times.
template <typename Func>
std::vector<double> run_benchmark(Func&& fn, size_t iterations,
                                  size_t warmup = 1) {
    // Warmup runs
    for (size_t i = 0; i < warmup; ++i) {
        fn();
    }

    std::vector<double> times;
    times.reserve(iterations);

    for (size_t i = 0; i < iterations; ++i) {
        auto t0 = std::chrono::high_resolution_clock::now();
        fn();
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        times.push_back(ms);
    }

    return times;
}

/// Get peak RSS in MB (cross-platform).
inline double get_peak_rss_mb() {
    struct rusage r;
    getrusage(RUSAGE_SELF, &r);
#ifdef __APPLE__
    return r.ru_maxrss / (1024.0 * 1024.0);
#else
    return r.ru_maxrss / 1024.0;
#endif
}

/// A named benchmark result (for CSV export).
struct BenchmarkEntry {
    std::string name;
    std::string engine;
    Stats       time_stats;       // elapsed ms
    size_t      records_scanned;
    size_t      records_matched;
    double      throughput;       // records/sec (mean)
};

/// Print CSV header for BenchmarkEntry.
inline void print_csv_header(std::ostream& out = std::cout) {
    out << "name,engine,mean_ms,stddev_ms,min_ms,max_ms,median_ms,"
        << "runs,records_scanned,records_matched,throughput_rec_per_sec"
        << std::endl;
}

/// Print a BenchmarkEntry as CSV.
inline void print_csv_row(const BenchmarkEntry& e,
                          std::ostream& out = std::cout) {
    out << e.name << "," << e.engine << ","
        << std::fixed << std::setprecision(4)
        << e.time_stats.mean << ","
        << e.time_stats.stddev << ","
        << e.time_stats.min_val << ","
        << e.time_stats.max_val << ","
        << e.time_stats.median << ","
        << e.time_stats.n << ","
        << e.records_scanned << ","
        << e.records_matched << ","
        << std::setprecision(0) << e.throughput
        << std::endl;
}

} // namespace parking::benchmark

#endif // PARKING_BENCHMARK_HARNESS_HPP
