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
class TextPool {
public:
    TextPool() = default;

    void reserve(size_t bytes) {
        pool_.reserve(bytes);
    }

    uint32_t append(const char* data, int length) {
        uint32_t offset = static_cast<uint32_t>(pool_.size());
        if (length > 0 && data != nullptr) {
            pool_.append(data, length);
        }
        return offset;
    }

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

struct SearchResult {
    std::vector<size_t> indices;
    size_t total_scanned = 0;
    double elapsed_ms = 0.0;
    size_t count() const { return indices.size(); }
};

struct AggregateResult {
    std::vector<std::pair<uint16_t, size_t>> counts;
    size_t total_scanned = 0;
    double elapsed_ms = 0.0;
};

class SearchEngine;

class ParkingAPI {
public:
    explicit ParkingAPI(bool use_indexed = false);
    ~ParkingAPI();
    /// Load a CSV file into the engine.
    size_t load(const std::string& filepath);

    SearchResult find_by_date_range(uint32_t start_date, uint32_t end_date);
    SearchResult find_by_violation_code(uint16_t code);
    SearchResult find_by_plate(const char* plate_id);
    SearchResult find_by_state(uint8_t state_enum);
    SearchResult find_by_county(uint8_t county_enum);


    AggregateResult count_by_precinct();
    AggregateResult count_by_fiscal_year();
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

}

#endif
