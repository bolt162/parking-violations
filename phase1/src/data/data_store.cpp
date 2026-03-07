#include "data/data_store.hpp"
#include <utility>

namespace parking::data {

void DataStore::reserve(size_t count) {
    records_.reserve(count);
    // Estimate ~70 bytes of text content per record
    text_pool_.reserve(count * 70);
}

void DataStore::add_records(std::vector<core::ViolationRecord>&& records) {
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

const core::ViolationRecord& DataStore::operator[](size_t index) const {
    return records_[index];
}

core::ViolationRecord& DataStore::operator[](size_t index) {
    return records_[index];
}

const std::vector<core::ViolationRecord>& DataStore::records() const {
    return records_;
}

std::vector<core::ViolationRecord>& DataStore::records() {
    return records_;
}

const core::ViolationRecord* DataStore::data() const {
    return records_.data();
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

} // namespace parking::data
