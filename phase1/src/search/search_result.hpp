#ifndef PARKING_SEARCH_RESULT_HPP
#define PARKING_SEARCH_RESULT_HPP

#include <vector>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace parking::search {

/// Lightweight result from a search query.
///
/// Stores indices into the DataStore rather than copies of records.
/// This works with both AoS (Phase 1/2) and SoA (Phase 3) layouts.
struct SearchResult {
    /// Indices of matching records in the DataStore.
    std::vector<size_t> indices;

    /// Total number of records scanned during the search.
    size_t total_scanned = 0;

    /// Wall-clock time for the search in milliseconds.
    double elapsed_ms = 0.0;

    /// Number of matching records.
    size_t count() const { return indices.size(); }
};

/// Result from an aggregation query (count by category).
struct AggregateResult {
    /// Pairs of (category_value, count).
    std::vector<std::pair<uint16_t, size_t>> counts;

    /// Total records scanned.
    size_t total_scanned = 0;

    /// Wall-clock time in milliseconds.
    double elapsed_ms = 0.0;
};

} // namespace parking::search

#endif // PARKING_SEARCH_RESULT_HPP
