#include <iostream>
#include <iomanip>
#include <chrono>
#include <sys/resource.h>

#include "api/parking_api.hpp"
#include "core/violation_record.hpp"
#include "core/field_types.hpp"
#include "core/text_ref.hpp"
#include "data/text_pool.hpp"

using namespace parking;

/// Get peak resident set size in MB
static double get_peak_rss_mb() {
    struct rusage r;
    getrusage(RUSAGE_SELF, &r);
#ifdef __APPLE__
    return r.ru_maxrss / (1024.0 * 1024.0);
#else
    return r.ru_maxrss / 1024.0;
#endif
}

static void print_pool_field(const core::ViolationRecord& r,
                              const data::TextPool& pool,
                              int field_idx) {
    uint8_t len = r.str_lengths[field_idx];
    if (len > 0) {
        const char* p = pool.get(r.str_offsets[field_idx]);
        std::cout.write(p, len);
    }
}

static void print_record(size_t idx, const core::ViolationRecord& r,
                          const data::TextPool& pool) {
    std::cout << "  [" << idx << "] summons=" << r.summons_number
              << " plate=";
    print_pool_field(r, pool, core::SF_PLATE_ID);
    std::cout << " state=" << (int)r.registration_state
              << " date=" << r.issue_date
              << " code=" << r.violation_code
              << " county=" << (int)r.violation_county
              << " make=";
    print_pool_field(r, pool, core::SF_VEHICLE_MAKE);
    std::cout << " color=";
    print_pool_field(r, pool, core::SF_VEHICLE_COLOR);
    std::cout << " fy=" << r.fiscal_year
              << std::endl;
}

static void print_search_result(const char* label,
                                const search::SearchResult& res) {
    std::cout << "  " << label << ": "
              << res.count() << " matches / "
              << res.total_scanned << " scanned in "
              << std::fixed << std::setprecision(2) << res.elapsed_ms << " ms"
              << " (" << std::setprecision(0)
              << (res.total_scanned / (res.elapsed_ms / 1000.0))
              << " rec/s)" << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <csv_file> [--indexed]"
                  << std::endl;
        std::cerr << "  --indexed  Use IndexedSearch (sorted indices + binary search)"
                  << std::endl;
        std::cerr << "  default    Use LinearSearch (serial scan)" << std::endl;
        return 1;
    }

    bool use_indexed = false;
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--indexed") {
            use_indexed = true;
        }
    }

    std::cout << "============================================" << std::endl;
    std::cout << "  Parking Violations Demo" << std::endl;
    std::cout << "  sizeof(ViolationRecord) = "
              << sizeof(core::ViolationRecord) << " bytes" << std::endl;
    std::cout << "  Search engine: "
              << (use_indexed ? "IndexedSearch" : "LinearSearch") << std::endl;
    std::cout << "============================================\n" << std::endl;

    // ── Create API and load data ──────────────────────────────────────────

    api::ParkingAPI engine(use_indexed);

    auto t0 = std::chrono::high_resolution_clock::now();
    size_t count = engine.load(argv[1]);
    auto t1 = std::chrono::high_resolution_clock::now();

    double load_sec = std::chrono::duration<double>(t1 - t0).count();

    std::cout << "\n── Load Summary ──" << std::endl;
    std::cout << "  Records loaded:  " << engine.record_count() << std::endl;
    std::cout << "  Load time:       " << std::fixed << std::setprecision(2)
              << load_sec << " s" << std::endl;
    std::cout << "  Throughput:      " << std::setprecision(0)
              << (engine.record_count() / load_sec) << " records/sec" << std::endl;
    std::cout << "  Peak RSS:        " << std::setprecision(1)
              << get_peak_rss_mb() << " MB" << std::endl;
    double rec_gb = engine.record_count() * sizeof(core::ViolationRecord)
                    / (1024.0 * 1024.0 * 1024.0);
    std::cout << "  In-memory size:  " << std::setprecision(2)
              << rec_gb << " GB structs + "
              << engine.text_pool().size_gb() << " GB text pool" << std::endl;
    std::cout << "  Engine:          " << engine.search_engine_name() << std::endl;
    std::cout << std::endl;

    if (engine.record_count() == 0) {
        std::cerr << "No records loaded — exiting." << std::endl;
        return 1;
    }

    // ── Sample records ───────────────────────────────────────────────────

    const auto& pool = engine.text_pool();

    std::cout << "── Sample Records ──" << std::endl;
    print_record(0, engine.record(0), pool);
    if (engine.record_count() > 1)
        print_record(1, engine.record(1), pool);
    if (engine.record_count() > 1000)
        print_record(1000, engine.record(1000), pool);
    if (engine.record_count() > 1000000)
        print_record(1000000, engine.record(1000000), pool);
    std::cout << std::endl;

    // ── Search queries ───────────────────────────────────────────────────

    std::cout << "── Search Queries ──" << std::endl;

    // B1: Date range (narrow) — January 2024
    {
        auto res = engine.find_by_date_range(20240101, 20240131);
        print_search_result("Date 2024-01", res);
    }

    // B2: Date range (wide) — all of 2024
    {
        auto res = engine.find_by_date_range(20240101, 20241231);
        print_search_result("Date 2024-all", res);
    }

    // B3: Violation code 46 (double parking — common)
    {
        auto res = engine.find_by_violation_code(46);
        print_search_result("Code 46", res);
    }

    // B4: Violation code 99 (rare)
    {
        auto res = engine.find_by_violation_code(99);
        print_search_result("Code 99", res);
    }

    // B5: Plate lookup (first record's plate)
    {
        const auto& r0 = engine.record(0);
        uint8_t plen = r0.str_lengths[core::SF_PLATE_ID];
        std::string sample_plate(pool.get(r0.str_offsets[core::SF_PLATE_ID]), plen);
        auto res = engine.find_by_plate(sample_plate.c_str());
        print_search_result("Plate lookup", res);
    }

    // B6: State = NY (enum 1, very common)
    {
        uint8_t ny = core::state_to_enum("NY", 2);
        auto res = engine.find_by_state(ny);
        print_search_result("State=NY", res);
    }

    // B7: State = FL (rare)
    {
        uint8_t fl = core::state_to_enum("FL", 2);
        auto res = engine.find_by_state(fl);
        print_search_result("State=FL", res);
    }

    // B8: County = BK (Brooklyn)
    {
        uint8_t bk = core::county_to_enum("BK", 2);
        auto res = engine.find_by_county(bk);
        print_search_result("County=BK", res);
    }

    std::cout << std::endl;

    // ── Aggregation queries ──────────────────────────────────────────────

    std::cout << "── Aggregation Queries ──" << std::endl;

    // B9: Count by precinct
    {
        auto res = engine.count_by_precinct();
        std::cout << "  Count by precinct: "
                  << res.counts.size() << " precincts in "
                  << std::fixed << std::setprecision(2)
                  << res.elapsed_ms << " ms" << std::endl;

        // Show top 5 precincts
        auto sorted = res.counts;
        std::sort(sorted.begin(), sorted.end(),
                  [](const auto& a, const auto& b) {
                      return a.second > b.second;
                  });
        std::cout << "    Top 5: ";
        for (size_t i = 0; i < std::min(sorted.size(), size_t(5)); ++i) {
            std::cout << "P" << sorted[i].first << "="
                      << sorted[i].second;
            if (i < 4 && i < sorted.size() - 1) std::cout << ", ";
        }
        std::cout << std::endl;
    }

    // B10: Count by fiscal year
    {
        auto res = engine.count_by_fiscal_year();
        std::cout << "  Count by fiscal year: "
                  << res.counts.size() << " years in "
                  << std::fixed << std::setprecision(2)
                  << res.elapsed_ms << " ms" << std::endl;

        auto sorted = res.counts;
        std::sort(sorted.begin(), sorted.end());
        for (const auto& [fy, cnt] : sorted) {
            std::cout << "    FY" << fy << ": " << cnt << std::endl;
        }
    }

    std::cout << "\n── Final Stats ──" << std::endl;
    std::cout << "  Peak RSS: " << std::setprecision(1) << get_peak_rss_mb()
              << " MB" << std::endl;
    std::cout << "\nDone." << std::endl;
    return 0;
}
