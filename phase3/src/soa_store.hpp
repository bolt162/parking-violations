#ifndef SOA_STORE_HPP
#define SOA_STORE_HPP

#include <vector>
#include <cstdint>
#include <cstddef>
#include <string>

namespace parking {

// Contiguous pool for all text field data.
class TextPool {
public:
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

    size_t size() const { 
        return pool_.size(); }
    void clear() { pool_.clear(); }

    double size_gb() const {
        return pool_.size() / (1024.0 * 1024.0 * 1024.0);
    }

    // Raw pointer to underlying data (for bulk merging)
    const char* data_ptr() const { return pool_.data(); }

    // Append bulk data (for merging thread-local pools)
    void append_bulk(const char* data, size_t length) {
        pool_.append(data, length);
    }

private:
    std::string pool_;
};


// Structure-of-Arrays data store.
// Each field is a separate contiguous array for cache efficiency.
struct SoADataStore {
    static constexpr int NUM_STR_FIELDS = 20;

    // Queried columns
    std::vector<uint32_t> issue_dates;
    std::vector<uint16_t> violation_codes;
    std::vector<uint16_t> violation_precincts;
    std::vector<uint16_t> fiscal_years;
    std::vector<uint8_t>  registration_states;
    std::vector<uint8_t>  violation_counties;

    // Plate lookup
    std::vector<uint32_t> plate_offsets;
    std::vector<uint8_t>  plate_lengths;


    // Other numeric columns
    std::vector<uint64_t> summons_numbers;
    std::vector<uint32_t> street_code1;
    std::vector<uint32_t> street_code2;
    std::vector<uint32_t> street_code3;
    std::vector<uint32_t> vehicle_expiration_dates;
    std::vector<uint32_t> issuer_codes;
    std::vector<uint32_t> date_first_observed;
    std::vector<uint16_t> issuer_precincts;
    std::vector<uint16_t> law_sections;
    std::vector<uint16_t> vehicle_years;
    std::vector<uint16_t> feet_from_curb;
    // Other enum columns
    std::vector<uint8_t> plate_types;
    std::vector<uint8_t> issuing_agencies;
    std::vector<uint8_t> issuer_squads;
    std::vector<uint8_t> violation_front_opposites;
    std::vector<uint8_t> violation_legal_codes;
    std::vector<uint8_t> unregistered_vehicles;

    // String field references
    std::vector<uint32_t> str_offsets[NUM_STR_FIELDS];
    std::vector<uint8_t>  str_lengths[NUM_STR_FIELDS];

    // Shared text pool
    TextPool text_pool;

    size_t record_count = 0;

    void reserve(size_t n);
    void resize(size_t n);
    size_t size() const { return record_count; }
};

}

#endif
