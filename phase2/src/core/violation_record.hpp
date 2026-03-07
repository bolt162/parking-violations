#ifndef PARKING_VIOLATION_RECORD_HPP
#define PARKING_VIOLATION_RECORD_HPP

#include <cstdint>
#include "core/text_ref.hpp"

namespace parking::core {

/// A single parking violation record. All fields use primitive types
/// (no std::string) to keep records contiguous in memory and avoid
/// heap allocations. Low-cardinality text fields are encoded as uint8_t
/// enum IDs via lookup tables in field_types.hpp.
///
/// String fields are stored in an external TextPool (owned by DataStore).
/// Each string field is represented by an (offset, length) pair:
///   str_offsets[i] = byte offset into TextPool
///   str_lengths[i] = length of the value (max 71)
/// Use the StringField enum (in text_ref.hpp) to index these arrays.
///
/// Layout: Array-of-Structs (AoS) — one struct per CSV row.
/// Phase 3 will transpose this into Struct-of-Arrays (SoA).
struct ViolationRecord {
    // ── Numeric fields ──────────────────────────────────────────────────
    uint64_t summons_number;            // 8 bytes  | 10-digit ID, max ~9.2B
    uint32_t issue_date;                // 4 bytes  | YYYYMMDD as integer
    uint32_t street_code1;              // 4 bytes
    uint32_t street_code2;              // 4 bytes
    uint32_t street_code3;              // 4 bytes
    uint32_t vehicle_expiration_date;   // 4 bytes  | YYYYMMDD
    uint32_t issuer_code;               // 4 bytes
    uint32_t date_first_observed;       // 4 bytes  | YYYYMMDD
    uint16_t violation_code;            // 2 bytes
    uint16_t violation_precinct;        // 2 bytes
    uint16_t issuer_precinct;           // 2 bytes
    uint16_t law_section;               // 2 bytes
    uint16_t vehicle_year;              // 2 bytes
    uint16_t feet_from_curb;            // 2 bytes
    uint16_t fiscal_year;               // 2 bytes

    // ── Enum-encoded fields (uint8_t) ───────────────────────────────────
    uint8_t  registration_state;        // 1 byte   | 65 codes
    uint8_t  plate_type;                // 1 byte   | 66 codes
    uint8_t  issuing_agency;            // 1 byte   | 25 codes
    uint8_t  issuer_squad;              // 1 byte   | 42 codes
    uint8_t  violation_county;          // 1 byte   | 5 boroughs
    uint8_t  violation_front_opposite;  // 1 byte   | 6 codes
    uint8_t  violation_legal_code;      // 1 byte   | 2 values
    uint8_t  unregistered_vehicle;      // 1 byte   | 2 values

    // ── String field references (into external TextPool) ────────────────
    // Use StringField enum to index: str_offsets[SF_PLATE_ID], etc.
    uint32_t str_offsets[NUM_TEXT_FIELDS]; // byte offsets into TextPool
    uint8_t  str_lengths[NUM_TEXT_FIELDS]; // lengths (max 71)

    /// Default constructor: zero-initializes all fields.
    ViolationRecord();

    /// Virtual destructor for OOP polymorphism readiness.
    virtual ~ViolationRecord() = default;

    /// Total number of fields in the canonical schema.
    static constexpr int FIELD_COUNT = 44;
};

} // namespace parking::core

#endif // PARKING_VIOLATION_RECORD_HPP
