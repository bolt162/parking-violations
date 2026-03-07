#include "api/parking_api.hpp"
#include <iostream>
#include <chrono>

namespace parking::api {

ParkingAPI::ParkingAPI(bool use_indexed, bool use_parallel)
    : use_indexed_(use_indexed)
{
    if (use_indexed) {
        search_engine_ = std::make_unique<search::IndexedSearch>();
    } else if (use_parallel) {
        search_engine_ = std::make_unique<search::ParallelSearch>();
    } else {
        search_engine_ = std::make_unique<search::LinearSearch>();
    }
}

// ── Data loading ────────────────────────────────────────────────────────────

size_t ParkingAPI::load(const std::string& filepath) {
    size_t count = loader_.load(filepath, store_);

    // If using indexed search, build the indices now
    if (use_indexed_ && count > 0) {
        auto* indexed = dynamic_cast<search::IndexedSearch*>(search_engine_.get());
        if (indexed) {
            auto t0 = std::chrono::high_resolution_clock::now();
            indexed->build_indices(store_);
            auto t1 = std::chrono::high_resolution_clock::now();
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            std::cout << "Index build time: " << (ms / 1000.0) << "s" << std::endl;
        }
    }

    return count;
}

// ── Filter queries ──────────────────────────────────────────────────────────

search::SearchResult ParkingAPI::find_by_date_range(uint32_t start_date,
                                                     uint32_t end_date) {
    return search_engine_->search_by_date_range(store_, start_date, end_date);
}

search::SearchResult ParkingAPI::find_by_violation_code(uint16_t code) {
    return search_engine_->search_by_violation_code(store_, code);
}

search::SearchResult ParkingAPI::find_by_plate(const char* plate_id) {
    int len = 0;
    while (plate_id[len] != '\0') len++;
    return search_engine_->search_by_plate(store_, store_.text_pool(),
                                            plate_id, len);
}

search::SearchResult ParkingAPI::find_by_state(uint8_t state_enum) {
    return search_engine_->search_by_state(store_, state_enum);
}

search::SearchResult ParkingAPI::find_by_county(uint8_t county_enum) {
    return search_engine_->search_by_county(store_, county_enum);
}

// ── Aggregation queries ─────────────────────────────────────────────────────

search::AggregateResult ParkingAPI::count_by_precinct() {
    return search_engine_->count_by_precinct(store_);
}

search::AggregateResult ParkingAPI::count_by_fiscal_year() {
    return search_engine_->count_by_fiscal_year(store_);
}

// ── Accessors ───────────────────────────────────────────────────────────────

size_t ParkingAPI::record_count() const {
    return store_.size();
}

const data::DataStore& ParkingAPI::store() const {
    return store_;
}

const core::ViolationRecord& ParkingAPI::record(size_t index) const {
    return store_[index];
}

const data::TextPool& ParkingAPI::text_pool() const {
    return store_.text_pool();
}

const char* ParkingAPI::search_engine_name() const {
    return search_engine_->name();
}

} // namespace parking::api
