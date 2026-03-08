#include "search.hpp"
#include <chrono>
#include <cstring>
#include <array>
#include <unordered_map>
#include <algorithm>
#include <numeric>
#include <iostream>

namespace parking {

// --- Timing helpers ---

using Clock = std::chrono::high_resolution_clock;

static inline auto now() { return Clock::now(); }

static inline double elapsed_ms(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

// --- scan_filter: eliminates boilerplate in linear filter queries ---
// Pred takes (const ViolationRecord&) and returns bool.

template <typename Pred>
static SearchResult scan_filter(const DataStore& store, Pred pred) {
    SearchResult result;
    auto t0 = now();

    const auto& recs = store.records();
    result.total_scanned = recs.size();

    for (size_t i = 0; i < recs.size(); ++i) {
        if (pred(recs[i])) {
            result.indices.push_back(i);
        }
    }

    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

// --- LinearSearch ---

SearchResult LinearSearch::search_by_date_range(
    const DataStore& store, uint32_t start_date, uint32_t end_date)
{
    return scan_filter(store, [=](const ViolationRecord& r) {
        return r.issue_date >= start_date && r.issue_date <= end_date;
    });
}

SearchResult LinearSearch::search_by_violation_code(
    const DataStore& store, uint16_t code)
{
    return scan_filter(store, [=](const ViolationRecord& r) {
        return r.violation_code == code;
    });
}

SearchResult LinearSearch::search_by_plate(
    const DataStore& store, const TextPool& pool,
    const char* plate_id, int plate_len)
{
    SearchResult result;
    auto t0 = now();

    const auto& recs = store.records();
    result.total_scanned = recs.size();

    for (size_t i = 0; i < recs.size(); ++i) {
        uint8_t len = recs[i].str_lengths[SF_PLATE_ID];
        if (len == plate_len) {
            const char* p = pool.get(recs[i].str_offsets[SF_PLATE_ID]);
            if (std::memcmp(p, plate_id, len) == 0) {
                result.indices.push_back(i);
            }
        }
    }

    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

SearchResult LinearSearch::search_by_state(
    const DataStore& store, uint8_t state_enum)
{
    return scan_filter(store, [=](const ViolationRecord& r) {
        return r.registration_state == state_enum;
    });
}

SearchResult LinearSearch::search_by_county(
    const DataStore& store, uint8_t county_enum)
{
    return scan_filter(store, [=](const ViolationRecord& r) {
        return r.violation_county == county_enum;
    });
}

AggregateResult LinearSearch::count_by_precinct(const DataStore& store) {
    AggregateResult result;
    auto t0 = now();

    const auto& recs = store.records();
    result.total_scanned = recs.size();

    std::array<size_t, 200> counts{};
    for (size_t i = 0; i < recs.size(); ++i) {
        uint16_t p = recs[i].violation_precinct;
        if (p < counts.size()) counts[p]++;
    }

    for (uint16_t p = 0; p < counts.size(); ++p) {
        if (counts[p] > 0) result.counts.emplace_back(p, counts[p]);
    }

    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

AggregateResult LinearSearch::count_by_fiscal_year(const DataStore& store) {
    AggregateResult result;
    auto t0 = now();

    const auto& recs = store.records();
    result.total_scanned = recs.size();

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

// --- IndexedSearch ---

void IndexedSearch::build_indices(const DataStore& store) {
    auto t0 = now();
    size_t n = store.size();
    const auto& recs = store.records();

    std::cout << "  Building sorted indices for " << n << " records..." << std::endl;

    auto make_index = [n]() -> std::vector<uint32_t> {
        std::vector<uint32_t> idx(n);
        std::iota(idx.begin(), idx.end(), 0);
        return idx;
    };

    {
        auto t = now();
        date_index_ = make_index();
        std::sort(date_index_.begin(), date_index_.end(),
                  [&recs](uint32_t a, uint32_t b) {
                      return recs[a].issue_date < recs[b].issue_date;
                  });
        std::cout << "    date_index:   " << elapsed_ms(t, now()) << " ms" << std::endl;
    }

    {
        auto t = now();
        code_index_ = make_index();
        std::sort(code_index_.begin(), code_index_.end(),
                  [&recs](uint32_t a, uint32_t b) {
                      return recs[a].violation_code < recs[b].violation_code;
                  });
        std::cout << "    code_index:   " << elapsed_ms(t, now()) << " ms" << std::endl;
    }

    {
        auto t = now();
        state_index_ = make_index();
        std::sort(state_index_.begin(), state_index_.end(),
                  [&recs](uint32_t a, uint32_t b) {
                      return recs[a].registration_state < recs[b].registration_state;
                  });
        std::cout << "    state_index:  " << elapsed_ms(t, now()) << " ms" << std::endl;
    }

    {
        auto t = now();
        county_index_ = make_index();
        std::sort(county_index_.begin(), county_index_.end(),
                  [&recs](uint32_t a, uint32_t b) {
                      return recs[a].violation_county < recs[b].violation_county;
                  });
        std::cout << "    county_index: " << elapsed_ms(t, now()) << " ms" << std::endl;
    }

    build_time_ms_ = elapsed_ms(t0, now());
    indices_built_ = true;

    double mem_mb = (4.0 * n * 4) / (1024.0 * 1024.0);
    std::cout << "  Indices built in " << (build_time_ms_ / 1000.0)
              << "s, memory: " << mem_mb << " MB" << std::endl;
}

SearchResult IndexedSearch::search_by_date_range(
    const DataStore& store, uint32_t start_date, uint32_t end_date)
{
    SearchResult result;
    auto t0 = now();

    const auto& recs = store.records();
    result.total_scanned = recs.size();

    auto lo = std::lower_bound(
        date_index_.begin(), date_index_.end(), start_date,
        [&recs](uint32_t idx, uint32_t val) {
            return recs[idx].issue_date < val;
        });

    auto hi = std::upper_bound(
        date_index_.begin(), date_index_.end(), end_date,
        [&recs](uint32_t val, uint32_t idx) {
            return val < recs[idx].issue_date;
        });

    result.indices.reserve(std::distance(lo, hi));
    for (auto it = lo; it != hi; ++it) {
        result.indices.push_back(*it);
    }

    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

SearchResult IndexedSearch::search_by_violation_code(
    const DataStore& store, uint16_t code)
{
    SearchResult result;
    auto t0 = now();

    const auto& recs = store.records();
    result.total_scanned = recs.size();

    auto lo = std::lower_bound(
        code_index_.begin(), code_index_.end(), code,
        [&recs](uint32_t idx, uint16_t val) {
            return recs[idx].violation_code < val;
        });

    auto hi = std::upper_bound(
        code_index_.begin(), code_index_.end(), code,
        [&recs](uint16_t val, uint32_t idx) {
            return val < recs[idx].violation_code;
        });

    result.indices.reserve(std::distance(lo, hi));
    for (auto it = lo; it != hi; ++it) {
        result.indices.push_back(*it);
    }

    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

SearchResult IndexedSearch::search_by_plate(
    const DataStore& store, const TextPool& pool,
    const char* plate_id, int plate_len)
{
    // Plate has too many unique values for a practical index, fall back to scan
    SearchResult result;
    auto t0 = now();

    const auto& recs = store.records();
    result.total_scanned = recs.size();

    for (size_t i = 0; i < recs.size(); ++i) {
        uint8_t len = recs[i].str_lengths[SF_PLATE_ID];
        if (len == plate_len) {
            const char* p = pool.get(recs[i].str_offsets[SF_PLATE_ID]);
            if (std::memcmp(p, plate_id, len) == 0) {
                result.indices.push_back(i);
            }
        }
    }

    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

SearchResult IndexedSearch::search_by_state(
    const DataStore& store, uint8_t state_enum)
{
    SearchResult result;
    auto t0 = now();

    const auto& recs = store.records();
    result.total_scanned = recs.size();

    auto lo = std::lower_bound(
        state_index_.begin(), state_index_.end(), state_enum,
        [&recs](uint32_t idx, uint8_t val) {
            return recs[idx].registration_state < val;
        });

    auto hi = std::upper_bound(
        state_index_.begin(), state_index_.end(), state_enum,
        [&recs](uint8_t val, uint32_t idx) {
            return val < recs[idx].registration_state;
        });

    result.indices.reserve(std::distance(lo, hi));
    for (auto it = lo; it != hi; ++it) {
        result.indices.push_back(*it);
    }

    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

SearchResult IndexedSearch::search_by_county(
    const DataStore& store, uint8_t county_enum)
{
    SearchResult result;
    auto t0 = now();

    const auto& recs = store.records();
    result.total_scanned = recs.size();

    auto lo = std::lower_bound(
        county_index_.begin(), county_index_.end(), county_enum,
        [&recs](uint32_t idx, uint8_t val) {
            return recs[idx].violation_county < val;
        });

    auto hi = std::upper_bound(
        county_index_.begin(), county_index_.end(), county_enum,
        [&recs](uint8_t val, uint32_t idx) {
            return val < recs[idx].violation_county;
        });

    result.indices.reserve(std::distance(lo, hi));
    for (auto it = lo; it != hi; ++it) {
        result.indices.push_back(*it);
    }

    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

// Aggregation queries (still full-scan even with indices)

AggregateResult IndexedSearch::count_by_precinct(const DataStore& store) {
    AggregateResult result;
    auto t0 = now();

    const auto& recs = store.records();
    result.total_scanned = recs.size();

    std::array<size_t, 200> counts{};
    for (size_t i = 0; i < recs.size(); ++i) {
        uint16_t p = recs[i].violation_precinct;
        if (p < counts.size()) counts[p]++;
    }

    for (uint16_t p = 0; p < counts.size(); ++p) {
        if (counts[p] > 0) result.counts.emplace_back(p, counts[p]);
    }

    result.elapsed_ms = elapsed_ms(t0, now());
    return result;
}

AggregateResult IndexedSearch::count_by_fiscal_year(const DataStore& store) {
    AggregateResult result;
    auto t0 = now();

    const auto& recs = store.records();
    result.total_scanned = recs.size();

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

} // namespace parking
