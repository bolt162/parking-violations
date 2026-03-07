#ifndef PARKING_TEXT_POOL_HPP
#define PARKING_TEXT_POOL_HPP

#include <string>
#include <cstdint>
#include <cstddef>

namespace parking::data {

/// A contiguous pool of all text (string) field data.
///
/// Instead of storing fixed-size char arrays inside each ViolationRecord
/// (which wastes memory through padding), all variable-length string
/// values are appended here. Each record stores (offset, length) pairs
/// that point into this pool.
///
/// Memory layout:
///   "GBB1234SUBNHONDAMAIN ST...XYZ999SEDNTOYOTA5TH AVE..."
///    ↑ rec0 fields                ↑ rec1 fields
///
/// Benefits:
///   - Zero padding waste (each value stores only its actual chars)
///   - Only ONE heap allocation (the underlying std::string)
///   - All text data is contiguous in memory
class TextPool {
public:
    TextPool() = default;

    /// Pre-allocate capacity for the estimated total text bytes.
    /// Call before loading to avoid repeated reallocations.
    /// @param bytes  Estimated total bytes (e.g., record_count * 70)
    void reserve(size_t bytes) {
        pool_.reserve(bytes);
    }

    /// Append a string value to the pool.
    /// @param data   Pointer to the string data (not null-terminated required)
    /// @param length Number of characters to append
    /// @return       Offset (byte position) where this value was stored
    uint32_t append(const char* data, int length) {
        uint32_t offset = static_cast<uint32_t>(pool_.size());
        if (length > 0 && data != nullptr) {
            pool_.append(data, length);
        }
        return offset;
    }

    /// Retrieve a pointer to text at the given offset.
    /// NOTE: The returned pointer is NOT null-terminated. Always use
    ///       the corresponding length from ViolationRecord::str_lengths[].
    /// @param offset  Byte offset returned by append()
    /// @return        Pointer to the first character
    const char* get(uint32_t offset) const {
        return pool_.data() + offset;
    }

    /// Current number of bytes stored in the pool.
    size_t size() const { return pool_.size(); }

    /// Current allocated capacity in bytes.
    size_t capacity() const { return pool_.capacity(); }

    /// Size in megabytes (for reporting).
    double size_mb() const {
        return pool_.size() / (1024.0 * 1024.0);
    }

    /// Size in gigabytes (for reporting).
    double size_gb() const {
        return pool_.size() / (1024.0 * 1024.0 * 1024.0);
    }

    /// Clear all data (for reuse in chunked parsing).
    void clear() { pool_.clear(); }

    /// Raw pointer to the underlying data (for bulk merging).
    const char* data_ptr() const { return pool_.data(); }

    /// Append a bulk chunk of data (for merging thread-local pools).
    /// @param data   Pointer to data to append
    /// @param length Number of bytes
    /// @return       Offset where the bulk data starts in this pool
    uint32_t append_bulk(const char* data, size_t length) {
        uint32_t offset = static_cast<uint32_t>(pool_.size());
        pool_.append(data, length);
        return offset;
    }

private:
    std::string pool_;
};

} // namespace parking::data

#endif // PARKING_TEXT_POOL_HPP
