#include "search/linear_search.hpp"
#include "core/text_ref.hpp"
#include "data/text_pool.hpp"
#include <chrono>
#include <cstring>
#include <array>
#include <unordered_map>

namespace parking::search {

// ── Helper: start/stop timer ────────────────────────────────────────────────

using Clock = std::chrono::high_resolution_clock;

static inline auto now() { return Clock::now(); }

static inline double elapsed_ms(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

// ── Filter queries ──────────────────────────────────────────────────────────

SearchResult LinearSearch::search_by_date_range(
    const data::DataStore& store,
    uint32_t start_date, uint32_t end_date)
{
    SearchResult result;
    auto t0 = now();

    const auto& recs = store.records();
    result.total_scanned = recs.size();

    for (size_t i = 0; i < recs.size(); ++i) {
        uint32_t d = recs[i].issue_date;
        if (d >= start_date && d <= end_date) {
            result.indices.push_back(i);
        }
    }

    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

SearchResult LinearSearch::search_by_violation_code(
    const data::DataStore& store,
    uint16_t code)
{
    SearchResult result;
    auto t0 = now();

    const auto& recs = store.records();
    result.total_scanned = recs.size();

    for (size_t i = 0; i < recs.size(); ++i) {
        if (recs[i].violation_code == code) {
            result.indices.push_back(i);
        }
    }

    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

SearchResult LinearSearch::search_by_plate(
    const data::DataStore& store,
    const data::TextPool& pool,
    const char* plate_id, int plate_len)
{
    SearchResult result;
    auto t0 = now();

    const auto& recs = store.records();
    result.total_scanned = recs.size();

    for (size_t i = 0; i < recs.size(); ++i) {
        uint8_t len = recs[i].str_lengths[core::SF_PLATE_ID];
        if (len == plate_len) {
            const char* p = pool.get(recs[i].str_offsets[core::SF_PLATE_ID]);
            if (std::memcmp(p, plate_id, len) == 0) {
                result.indices.push_back(i);
            }
        }
    }

    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

SearchResult LinearSearch::search_by_state(
    const data::DataStore& store,
    uint8_t state_enum)
{
    SearchResult result;
    auto t0 = now();

    const auto& recs = store.records();
    result.total_scanned = recs.size();

    for (size_t i = 0; i < recs.size(); ++i) {
        if (recs[i].registration_state == state_enum) {
            result.indices.push_back(i);
        }
    }

    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

SearchResult LinearSearch::search_by_county(
    const data::DataStore& store,
    uint8_t county_enum)
{
    SearchResult result;
    auto t0 = now();

    const auto& recs = store.records();
    result.total_scanned = recs.size();

    for (size_t i = 0; i < recs.size(); ++i) {
        if (recs[i].violation_county == county_enum) {
            result.indices.push_back(i);
        }
    }

    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

// ── Aggregation queries ─────────────────────────────────────────────────────

AggregateResult LinearSearch::count_by_precinct(
    const data::DataStore& store)
{
    AggregateResult result;
    auto t0 = now();

    const auto& recs = store.records();
    result.total_scanned = recs.size();

    // Precincts are small numbers (0-123 typical), use a fixed array
    std::array<size_t, 200> counts{};

    for (size_t i = 0; i < recs.size(); ++i) {
        uint16_t p = recs[i].violation_precinct;
        if (p < counts.size()) {
            counts[p]++;
        }
    }

    // Collect non-zero entries
    for (uint16_t p = 0; p < counts.size(); ++p) {
        if (counts[p] > 0) {
            result.counts.emplace_back(p, counts[p]);
        }
    }

    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

AggregateResult LinearSearch::count_by_fiscal_year(
    const data::DataStore& store)
{
    AggregateResult result;
    auto t0 = now();

    const auto& recs = store.records();
    result.total_scanned = recs.size();

    // Only 3 fiscal years expected
    std::unordered_map<uint16_t, size_t> counts;
    counts.reserve(4);

    for (size_t i = 0; i < recs.size(); ++i) {
        counts[recs[i].fiscal_year]++;
    }

    for (auto& [fy, cnt] : counts) {
        result.counts.emplace_back(fy, cnt);
    }

    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

} // namespace parking::search
