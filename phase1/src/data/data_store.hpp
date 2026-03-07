#ifndef PARKING_DATA_STORE_HPP
#define PARKING_DATA_STORE_HPP

#include <vector>
#include <cstddef>
#include "core/violation_record.hpp"
#include "data/text_pool.hpp"

namespace parking::data {

/// Central data container that owns all loaded ViolationRecords
/// and the shared TextPool for string field data.
///
/// Design notes:
///   - Phase 1 (serial): records are loaded sequentially and searched linearly.
///   - Phase 2 (OpenMP): records() returns a reference so parallel for can
///     iterate the underlying vector directly.
///   - Phase 3 (SoA):    data()/size() give raw pointer access for transposing
///     the AoS layout into column arrays.
class DataStore {
public:
    DataStore() = default;
    virtual ~DataStore() = default;

    /// Pre-allocate capacity for the expected number of records
    /// and the TextPool for estimated string data.
    void reserve(size_t count);

    /// Append records by moving them from a source vector.
    void add_records(std::vector<core::ViolationRecord>&& records);

    /// Number of records currently stored.
    size_t size() const;

    /// Direct element access (bounds-unchecked for speed).
    const core::ViolationRecord& operator[](size_t index) const;
    core::ViolationRecord& operator[](size_t index);

    /// Reference to the underlying vector (for OpenMP iteration in Phase 2).
    const std::vector<core::ViolationRecord>& records() const;
    std::vector<core::ViolationRecord>& records();

    /// Raw pointer access (for AoS -> SoA transposition in Phase 3).
    const core::ViolationRecord* data() const;

    /// Current allocated capacity.
    size_t capacity() const;

    /// Access the shared TextPool (string data for all records).
    const TextPool& text_pool() const;
    TextPool& text_pool();

private:
    std::vector<core::ViolationRecord> records_;
    TextPool text_pool_;
};

} // namespace parking::data

#endif // PARKING_DATA_STORE_HPP
