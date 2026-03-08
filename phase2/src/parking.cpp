#include "parking.hpp"
#include "csv_reader.hpp"
#include "search.hpp"
#include <iostream>
#include <chrono>
#include <utility>

namespace parking {

// --- DataStore ---

void DataStore::reserve(size_t count) {
    records_.reserve(count);
    // Estimate ~70 bytes of text content per record
    text_pool_.reserve(count * 70);
}

void DataStore::add_records(std::vector<ViolationRecord>&& records) {
    if (records_.empty()) {
        records_ = std::move(records);
    } else {
        records_.insert(records_.end(),
                        std::make_move_iterator(records.begin()),
                        std::make_move_iterator(records.end()));
    }
}

size_t DataStore::size() const {
    return records_.size();
}

const ViolationRecord& DataStore::operator[](size_t index) const {
    return records_[index];
}

ViolationRecord& DataStore::operator[](size_t index) {
    return records_[index];
}

const std::vector<ViolationRecord>& DataStore::records() const {
    return records_;
}

std::vector<ViolationRecord>& DataStore::records() {
    return records_;
}

size_t DataStore::capacity() const {
    return records_.capacity();
}

const TextPool& DataStore::text_pool() const {
    return text_pool_;
}

TextPool& DataStore::text_pool() {
    return text_pool_;
}

// --- ParkingAPI ---

ParkingAPI::ParkingAPI(bool use_indexed)
    : use_indexed_(use_indexed)
{
    if (use_indexed) {
        search_engine_ = std::make_unique<IndexedSearch>();
    } else {
        search_engine_ = std::make_unique<ParallelSearch>();
    }
}

// Out-of-line destructor so unique_ptr<SearchEngine> works with forward decl
ParkingAPI::~ParkingAPI() = default;

size_t ParkingAPI::load(const std::string& filepath) {
    size_t count = load_csv(filepath, store_);

    if (use_indexed_ && count > 0) {
        auto* indexed = dynamic_cast<IndexedSearch*>(search_engine_.get());
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

// Filter queries

SearchResult ParkingAPI::find_by_date_range(uint32_t start_date,
                                             uint32_t end_date) {
    return search_engine_->search_by_date_range(store_, start_date, end_date);
}

SearchResult ParkingAPI::find_by_violation_code(uint16_t code) {
    return search_engine_->search_by_violation_code(store_, code);
}

SearchResult ParkingAPI::find_by_plate(const char* plate_id) {
    int len = 0;
    while (plate_id[len] != '\0') len++;
    return search_engine_->search_by_plate(store_, store_.text_pool(),
                                            plate_id, len);
}

SearchResult ParkingAPI::find_by_state(uint8_t state_enum) {
    return search_engine_->search_by_state(store_, state_enum);
}

SearchResult ParkingAPI::find_by_county(uint8_t county_enum) {
    return search_engine_->search_by_county(store_, county_enum);
}

// Aggregation queries

AggregateResult ParkingAPI::count_by_precinct() {
    return search_engine_->count_by_precinct(store_);
}

AggregateResult ParkingAPI::count_by_fiscal_year() {
    return search_engine_->count_by_fiscal_year(store_);
}

// Accessors

size_t ParkingAPI::record_count() const {
    return store_.size();
}

const DataStore& ParkingAPI::store() const {
    return store_;
}

const ViolationRecord& ParkingAPI::record(size_t index) const {
    return store_[index];
}

const TextPool& ParkingAPI::text_pool() const {
    return store_.text_pool();
}

const char* ParkingAPI::search_engine_name() const {
    return search_engine_->name();
}

} // namespace parking
