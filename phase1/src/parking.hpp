#ifndef PARKING_HPP
#define PARKING_HPP

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <cstddef>
#include <utility>
#include "record.hpp"

namespace parking {

// Contiguous pool for all text (string) field data.
// Variable-length string values are appended here instead of stored
// as fixed-size char arrays. Each record stores (offset, length) pairs
// that point into this pool.
class TextPool {
public:
    TextPool() = default;

    // Pre-allocate capacity for estimated total text bytes.
    void reserve(size_t bytes) {
        pool_.reserve(bytes);
    }

    // Append a string value. Returns the byte offset where it was stored.
    uint32_t append(const char* data, int length) {
        uint32_t offset = static_cast<uint32_t>(pool_.size());
        if (length > 0 && data != nullptr) {
            pool_.append(data, length);
        }
        return offset;
    }

    // Retrieve pointer to text at given offset (NOT null-terminated).
    const char* get(uint32_t offset) const {
        return pool_.data() + offset;
    }

    size_t size() const { return pool_.size(); }
    size_t capacity() const { return pool_.capacity(); }

    double size_mb() const {
        return pool_.size() / (1024.0 * 1024.0);
    }

    double size_gb() const {
        return pool_.size() / (1024.0 * 1024.0 * 1024.0);
    }

private:
    std::string pool_;
};

// Central data container owning all loaded ViolationRecords
// and the shared TextPool for string field data.
class DataStore {
public:
    DataStore() = default;
    ~DataStore() = default;

    void reserve(size_t count);
    void add_records(std::vector<ViolationRecord>&& records);

    size_t size() const;

    const ViolationRecord& operator[](size_t index) const;
    ViolationRecord& operator[](size_t index);

    const std::vector<ViolationRecord>& records() const;
    std::vector<ViolationRecord>& records();

    size_t capacity() const;

    const TextPool& text_pool() const;
    TextPool& text_pool();

private:
    std::vector<ViolationRecord> records_;
    TextPool text_pool_;
};

// --- Search result types ---

// Lightweight result from a filter query.
// Stores indices into the DataStore rather than copies of records.
struct SearchResult {
    std::vector<size_t> indices;
    size_t total_scanned = 0;
    double elapsed_ms = 0.0;
    size_t count() const { return indices.size(); }
};

// Result from an aggregation query (count by category).
struct AggregateResult {
    std::vector<std::pair<uint16_t, size_t>> counts;
    size_t total_scanned = 0;
    double elapsed_ms = 0.0;
};

// Forward declaration -- defined in search.hpp
class SearchEngine;

// Top-level facade for the parking violations engine.
// Hides DataStore, CSV loading, and SearchEngine behind a simple API.
//
// Usage:
//   ParkingAPI api;
//   api.load("parking_violations_merged.csv");
//   auto results = api.find_by_violation_code(46);
//   std::cout << results.count() << " matches\n";
class ParkingAPI {
public:
    /// If use_indexed is true, builds sorted indices for O(log n) queries.
    /// Otherwise uses serial linear scan.
    explicit ParkingAPI(bool use_indexed = false);
    ~ParkingAPI();

    // --- Data loading ---

    /// Load a CSV file into the engine.
    size_t load(const std::string& filepath);

    // --- Filter queries ---

    SearchResult find_by_date_range(uint32_t start_date, uint32_t end_date);
    SearchResult find_by_violation_code(uint16_t code);
    SearchResult find_by_plate(const char* plate_id);
    SearchResult find_by_state(uint8_t state_enum);
    SearchResult find_by_county(uint8_t county_enum);

    // --- Aggregation queries ---

    AggregateResult count_by_precinct();
    AggregateResult count_by_fiscal_year();

    // --- Accessors ---

    size_t record_count() const;
    const DataStore& store() const;
    const ViolationRecord& record(size_t index) const;
    const TextPool& text_pool() const;
    const char* search_engine_name() const;

private:
    DataStore store_;
    std::unique_ptr<SearchEngine> search_engine_;
    bool use_indexed_;
};

} // namespace parking

#endif // PARKING_HPP
