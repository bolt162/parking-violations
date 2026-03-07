#ifndef PARKING_API_HPP
#define PARKING_API_HPP

#include <string>
#include <memory>
#include <cstdint>

#include "data/data_store.hpp"
#include "io/file_loader.hpp"
#include "search/search_engine.hpp"
#include "search/search_result.hpp"
#include "search/linear_search.hpp"
#include "search/indexed_search.hpp"
#include "search/parallel_search.hpp"

namespace parking::api {

/// Top-level Facade for the parking violations engine.
///
/// Hides all internal subsystems (DataStore, FileLoader, SearchEngine)
/// behind a simple public API. Users only interact with this class.
///
/// Usage:
///   ParkingAPI api;
///   api.load("parking_violations_merged.csv");
///   auto results = api.find_by_violation_code(46);
///   std::cout << results.count() << " matches\n";
class ParkingAPI {
public:
    /// Construct with choice of search engine.
    /// @param use_indexed   If true, builds sorted indices after loading
    ///                      for O(log n) queries.
    /// @param use_parallel  If true, uses OpenMP-parallelized linear scan.
    ///                      Ignored if use_indexed is true.
    explicit ParkingAPI(bool use_indexed = false, bool use_parallel = false);

    virtual ~ParkingAPI() = default;

    // ── Data loading ────────────────────────────────────────────────────

    /// Load a CSV file into the engine.
    /// @param filepath  Path to the merged CSV file
    /// @return          Number of records loaded
    size_t load(const std::string& filepath);

    // ── Filter queries ──────────────────────────────────────────────────

    /// Find records where issue_date is in [start_date, end_date].
    search::SearchResult find_by_date_range(uint32_t start_date,
                                            uint32_t end_date);

    /// Find records matching an exact violation_code.
    search::SearchResult find_by_violation_code(uint16_t code);

    /// Find records matching a plate_id (string).
    search::SearchResult find_by_plate(const char* plate_id);

    /// Find records matching a registration_state enum value.
    search::SearchResult find_by_state(uint8_t state_enum);

    /// Find records matching a violation_county enum value.
    search::SearchResult find_by_county(uint8_t county_enum);

    // ── Aggregation queries ─────────────────────────────────────────────

    /// Count violations per precinct.
    search::AggregateResult count_by_precinct();

    /// Count violations per fiscal year.
    search::AggregateResult count_by_fiscal_year();

    // ── Accessors ───────────────────────────────────────────────────────

    /// Total number of loaded records.
    size_t record_count() const;

    /// Access the underlying DataStore (for benchmarks/tests).
    const data::DataStore& store() const;

    /// Access a specific record by index.
    const core::ViolationRecord& record(size_t index) const;

    /// Access the shared TextPool (for printing string fields).
    const data::TextPool& text_pool() const;

    /// Name of the active search engine.
    const char* search_engine_name() const;

private:
    data::DataStore store_;
    io::FileLoader loader_;
    std::unique_ptr<search::SearchEngine> search_engine_;
    bool use_indexed_;
};

} // namespace parking::api

#endif // PARKING_API_HPP
