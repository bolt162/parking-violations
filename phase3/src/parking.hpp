#ifndef PARKING_HPP
#define PARKING_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <utility>
#include "soa_store.hpp"

namespace parking {

// Search result: list of matching record indices
struct SearchResult {
    std::vector<size_t> indices;
    size_t total_scanned = 0;
    double elapsed_ms = 0.0;
    size_t count() const { return indices.size(); }
};

// Aggregation result: list of (category, count) pairs
struct AggregateResult {
    std::vector<std::pair<uint16_t, size_t>> counts;
    size_t total_scanned = 0;
    double elapsed_ms = 0.0;
};

// Count-only result (SIMD-friendly query)
struct CountResult {
    size_t count = 0;
    size_t total_scanned = 0;
    double elapsed_ms = 0.0;
};

// Date range result (SIMD-friendly query)
struct DateRangeResult {
    uint32_t min_date = 0;
    uint32_t max_date = 0;
    size_t total_scanned = 0;
    double elapsed_ms = 0.0;
};

// Main API for parking violations engine
class ParkingAPI {
public:
    ParkingAPI() = default;

    // Load CSV file
    size_t load(const std::string& filepath);

    // SIMD-friendly queries (pure reductions, can be auto-vectorized)
    CountResult count_in_date_range(uint32_t start_date, uint32_t end_date);
    DateRangeResult find_date_extremes();

    // Filter queries (conditional collection, cannot be vectorized)
    SearchResult find_by_date_range(uint32_t start_date, uint32_t end_date);
    SearchResult find_by_violation_code(uint16_t code);
    SearchResult find_by_plate(const char* plate_id);
    SearchResult find_by_state(uint8_t state_enum);
    SearchResult find_by_county(uint8_t county_enum);

    // Aggregation queries
    AggregateResult count_by_precinct();
    AggregateResult count_by_fiscal_year();

    // Accessors
    size_t record_count() const { return store_.size(); }
    const SoADataStore& store() const { return store_; }
    const TextPool& text_pool() const { return store_.text_pool; }

private:
    SoADataStore store_;
};

} // namespace parking

#endif
