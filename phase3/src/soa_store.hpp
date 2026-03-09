#ifndef SOA_STORE_HPP
#define SOA_STORE_HPP

#include <vector>
#include <cstdint>
#include <cstddef>
#include <string>

namespace parking {

// Contiguous pool for all text field data.
// Same design as Phase 2.
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

    size_t size() const { return pool_.size(); }
    const char* data_ptr() const { return pool_.data(); }

    uint32_t append_bulk(const char* data, size_t length) {
        uint32_t offset = static_cast<uint32_t>(pool_.size());
        pool_.append(data, length);
        return offset;
    }

    void clear() { pool_.clear(); }

    double size_gb() const {
        return pool_.size() / (1024.0 * 1024.0 * 1024.0);
    }

private:
    std::string pool_;
};

// Structure-of-Arrays data store.
// Each field is a separate contiguous array.
// This improves cache efficiency: searching by issue_date only loads issue_dates array.
struct SoADataStore {

    // HOT: fields used by search queries
    std::vector<uint32_t> issue_dates;
    std::vector<uint16_t> violation_codes;
    std::vector<uint16_t> violation_precincts;
    std::vector<uint16_t> fiscal_years;
    std::vector<uint8_t>  registration_states;
    std::vector<uint8_t>  violation_counties;

    // WARM: plate lookup needs these
    std::vector<uint32_t> plate_offsets;
    std::vector<uint8_t>  plate_lengths;

    // COLD: numeric fields not used in queries
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

    // COLD: enum fields not used in queries
    std::vector<uint8_t> plate_types;
    std::vector<uint8_t> issuing_agencies;
    std::vector<uint8_t> issuer_squads;
    std::vector<uint8_t> violation_front_opposites;
    std::vector<uint8_t> violation_legal_codes;
    std::vector<uint8_t> unregistered_vehicles;

    // String field references (20 fields, indices 1-20 from StringField enum)
    // Index 0 (plate_id) is stored separately above as plate_offsets/plate_lengths
    static constexpr int NUM_STR_FIELDS = 20;
    std::vector<uint32_t> str_offsets[NUM_STR_FIELDS];
    std::vector<uint8_t>  str_lengths[NUM_STR_FIELDS];

    // Shared text pool
    TextPool text_pool;

    size_t record_count = 0;

    void reserve(size_t n);
    void resize(size_t n);
    size_t size() const { return record_count; }
};

} // namespace parking

#endif
