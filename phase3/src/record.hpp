#ifndef PARKING_RECORD_HPP
#define PARKING_RECORD_HPP

#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <string>

namespace parking {

// Column indices (0-based) in the canonical merged CSV
enum class Column : int {
    SUMMONS_NUMBER = 0,
    PLATE_ID = 1,
    REGISTRATION_STATE = 2,
    PLATE_TYPE = 3,
    ISSUE_DATE = 4,
    VIOLATION_CODE = 5,
    VEHICLE_BODY_TYPE = 6,
    VEHICLE_MAKE = 7,
    ISSUING_AGENCY = 8,
    STREET_CODE1 = 9,
    STREET_CODE2 = 10,
    STREET_CODE3 = 11,
    VEHICLE_EXPIRATION_DATE = 12,
    VIOLATION_LOCATION = 13,
    VIOLATION_PRECINCT = 14,
    ISSUER_PRECINCT = 15,
    ISSUER_CODE = 16,
    ISSUER_COMMAND = 17,
    ISSUER_SQUAD = 18,
    VIOLATION_TIME = 19,
    TIME_FIRST_OBSERVED = 20,
    VIOLATION_COUNTY = 21,
    VIOLATION_FRONT_OPPOSITE = 22,
    HOUSE_NUMBER = 23,
    STREET_NAME = 24,
    INTERSECTING_STREET = 25,
    DATE_FIRST_OBSERVED = 26,
    LAW_SECTION = 27,
    SUB_DIVISION = 28,
    VIOLATION_LEGAL_CODE = 29,
    DAYS_PARKING_IN_EFFECT = 30,
    FROM_HOURS_IN_EFFECT = 31,
    TO_HOURS_IN_EFFECT = 32,
    VEHICLE_COLOR = 33,
    UNREGISTERED_VEHICLE = 34,
    VEHICLE_YEAR = 35,
    METER_NUMBER = 36,
    FEET_FROM_CURB = 37,
    VIOLATION_POST_CODE = 38,
    VIOLATION_DESCRIPTION = 39,
    NO_STANDING_STOPPING = 40,
    HYDRANT_VIOLATION = 41,
    DOUBLE_PARKING = 42,
    FISCAL_YEAR = 43
};

constexpr int COLUMN_COUNT = 44;

// Null sentinel values
constexpr uint32_t NULL_DATE = 0;
constexpr uint64_t NULL_UINT64 = 0;
constexpr uint32_t NULL_UINT32 = 0;
constexpr uint16_t NULL_UINT16 = 0;
constexpr uint8_t  NULL_ENUM = 0;

// --- Enum IDs for low-cardinality fields ---
// Each enum uses 0 = unknown/empty, 1+ for known codes.

namespace county {
    constexpr uint8_t UNKNOWN  = 0;
    constexpr uint8_t MN       = 1;  // Manhattan
    constexpr uint8_t BK       = 2;  // Brooklyn
    constexpr uint8_t QN       = 3;  // Queens
    constexpr uint8_t BX       = 4;  // Bronx
    constexpr uint8_t SI       = 5;  // Staten Island
}

inline uint8_t county_to_enum(const char* s, int len) {
    if (len == 0) return county::UNKNOWN;
    if (len == 2) {
        if (s[0] == 'M' && s[1] == 'N') return county::MN;
        if (s[0] == 'B' && s[1] == 'K') return county::BK;
        if (s[0] == 'Q' && s[1] == 'N') return county::QN;
        if (s[0] == 'B' && s[1] == 'X') return county::BX;
        if (s[0] == 'S' && s[1] == 'I') return county::SI;
    }
    return county::UNKNOWN;
}

// Issuing Agency (25 codes, top 4 dominate 99.6%)
namespace agency {
    constexpr uint8_t UNKNOWN = 0;
    constexpr uint8_t V = 1;   // Camera/photo
    constexpr uint8_t T = 2;   // Traffic
    constexpr uint8_t S = 3;   // Sanitation
    constexpr uint8_t P = 4;   // Police
    constexpr uint8_t K = 5;
    constexpr uint8_t Y = 6;
    constexpr uint8_t O = 7;
    constexpr uint8_t A = 8;
    constexpr uint8_t M = 9;
    constexpr uint8_t EIGHT = 10;
    constexpr uint8_t L = 11;
    constexpr uint8_t R = 12;
    constexpr uint8_t N = 13;
    constexpr uint8_t NINE = 14;
    constexpr uint8_t C = 15;
    constexpr uint8_t F = 16;
    constexpr uint8_t THREE = 17;
    constexpr uint8_t G = 18;
    constexpr uint8_t X = 19;
    constexpr uint8_t W = 20;
    constexpr uint8_t FOUR = 21;
    constexpr uint8_t Q = 22;
    constexpr uint8_t ONE = 23;
    constexpr uint8_t U = 24;
    constexpr uint8_t E = 25;
}

inline uint8_t agency_to_enum(const char* s, int len) {
    if (len != 1) return agency::UNKNOWN;
    switch (s[0]) {
        case 'V': return agency::V;
        case 'T': return agency::T;
        case 'S': return agency::S;
        case 'P': return agency::P;
        case 'K': return agency::K;
        case 'Y': return agency::Y;
        case 'O': return agency::O;
        case 'A': return agency::A;
        case 'M': return agency::M;
        case '8': return agency::EIGHT;
        case 'L': return agency::L;
        case 'R': return agency::R;
        case 'N': return agency::N;
        case '9': return agency::NINE;
        case 'C': return agency::C;
        case 'F': return agency::F;
        case '3': return agency::THREE;
        case 'G': return agency::G;
        case 'X': return agency::X;
        case 'W': return agency::W;
        case '4': return agency::FOUR;
        case 'Q': return agency::Q;
        case '1': return agency::ONE;
        case 'U': return agency::U;
        case 'E': return agency::E;
        default:  return agency::UNKNOWN;
    }
}

// Violation In Front Of Or Opposite (6 codes)
namespace front_opposite {
    constexpr uint8_t UNKNOWN = 0;
    constexpr uint8_t F = 1;  // Front
    constexpr uint8_t O = 2;  // Opposite
    constexpr uint8_t I = 3;  // Intersecting
    constexpr uint8_t R = 4;
    constexpr uint8_t X = 5;
}

inline uint8_t front_opposite_to_enum(const char* s, int len) {
    if (len != 1) return front_opposite::UNKNOWN;
    switch (s[0]) {
        case 'F': return front_opposite::F;
        case 'O': return front_opposite::O;
        case 'I': return front_opposite::I;
        case 'R': return front_opposite::R;
        case 'X': return front_opposite::X;
        default:  return front_opposite::UNKNOWN;
    }
}

// Violation Legal Code (2 values)
namespace legal_code {
    constexpr uint8_t UNKNOWN = 0;
    constexpr uint8_t T = 1;
}

inline uint8_t legal_code_to_enum(const char* s, int len) {
    if (len == 1 && s[0] == 'T') return legal_code::T;
    return legal_code::UNKNOWN;
}

// Unregistered Vehicle (2 values)
namespace unreg_vehicle {
    constexpr uint8_t UNKNOWN = 0;
    constexpr uint8_t ZERO = 1;
}

inline uint8_t unreg_vehicle_to_enum(const char* s, int len) {
    if (len == 1 && s[0] == '0') return unreg_vehicle::ZERO;
    return unreg_vehicle::UNKNOWN;
}

// Registration State (65 codes)
inline uint8_t state_to_enum(const char* s, int len) {
    static const std::unordered_map<uint16_t, uint8_t> table = []() {
        std::unordered_map<uint16_t, uint8_t> m;
        auto pack = [](const char* code) -> uint16_t {
            return (static_cast<uint16_t>(code[0]) << 8) | code[1];
        };
        const char* codes[] = {
            "NY","NJ","PA","FL","CT","VA","MA","GA","NC","MD",
            "IN","TX","IL","OH","CA","AZ","SC","99","ME","TN",
            "AL","DE","MI","RI","MN","WA","VT","MS","NH","CO",
            "MO","WI","OK","ON","SD","OR","KY","LA","MT","DC",
            "WV","UT","AR","NV","KS","IA","NE","NM","ID","DP",
            "WY","ND","AK","PR","GV","HI","AB","NB","BC","PE",
            "NS","MB","FO","QB","QC"
        };
        for (uint8_t i = 0; i < sizeof(codes)/sizeof(codes[0]); ++i) {
            m[pack(codes[i])] = i + 1;
        }
        return m;
    }();

    if (len != 2) return 0;
    uint16_t key = (static_cast<uint16_t>(s[0]) << 8) | s[1];
    auto it = table.find(key);
    return (it != table.end()) ? it->second : 0;
}

// Plate Type (66 codes)
inline uint8_t plate_type_to_enum(const char* s, int len) {
    static const std::unordered_map<std::string, uint8_t> table = []() {
        std::unordered_map<std::string, uint8_t> m;
        const char* codes[] = {
            "PAS","COM","OMT","OMS","SRF","MOT","999","ORG","APP","SPO",
            "MED","PSD","OMR","TRC","RGL","CMB","TOW","SCL","OML","VAS",
            "IRP","LMB","ITP","SRN","TRL","HIS","OMV","DLR","TRA","STA",
            "MCL","TMP","ORC","LMA","PHS","AMB","RGC","SPC","AGR","SOS",
            "NYS","NLM","HAM","LTR","BOB","ATV","VPL","STG","SEM","PPH",
            "MCD","LUA","CMH","CHC","CCK","CBS","AYG","USC","THC","HSM",
            "HIR","FAR","CSP","CME","AGC"
        };
        for (uint8_t i = 0; i < sizeof(codes)/sizeof(codes[0]); ++i) {
            m[codes[i]] = i + 1;
        }
        return m;
    }();

    if (len == 0 || len > 3) return 0;
    auto it = table.find(std::string(s, len));
    return (it != table.end()) ? it->second : 0;
}

// Issuer Squad (42 codes)
inline uint8_t squad_to_enum(const char* s, int len) {
    static const std::unordered_map<std::string, uint8_t> table = []() {
        std::unordered_map<std::string, uint8_t> m;
        const char* codes[] = {
            "A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P",
            "Q","R","S","T","U","V","X","Y","Z","0000",
            "AA","BB","CC","DD","EE","FF","GG","HH","PP","YP","YA",
            "X1","X2","B2","A2"
        };
        for (uint8_t i = 0; i < sizeof(codes)/sizeof(codes[0]); ++i) {
            m[codes[i]] = i + 1;
        }
        return m;
    }();

    if (len == 0) return 0;
    auto it = table.find(std::string(s, len));
    return (it != table.end()) ? it->second : 0;
}

// --- Text field references ---

constexpr int NUM_TEXT_FIELDS = 21;

// Indexes into ViolationRecord::str_offsets[] and str_lengths[]
enum StringField : int {
    SF_PLATE_ID                = 0,
    SF_VEHICLE_BODY_TYPE       = 1,
    SF_VEHICLE_MAKE            = 2,
    SF_VIOLATION_LOCATION      = 3,
    SF_ISSUER_COMMAND          = 4,
    SF_VIOLATION_TIME          = 5,
    SF_TIME_FIRST_OBSERVED     = 6,
    SF_HOUSE_NUMBER            = 7,
    SF_STREET_NAME             = 8,
    SF_INTERSECTING_STREET     = 9,
    SF_SUB_DIVISION            = 10,
    SF_DAYS_PARKING_IN_EFFECT  = 11,
    SF_FROM_HOURS_IN_EFFECT    = 12,
    SF_TO_HOURS_IN_EFFECT      = 13,
    SF_VEHICLE_COLOR           = 14,
    SF_METER_NUMBER            = 15,
    SF_VIOLATION_POST_CODE     = 16,
    SF_VIOLATION_DESCRIPTION   = 17,
    SF_NO_STANDING_STOPPING    = 18,
    SF_HYDRANT_VIOLATION       = 19,
    SF_DOUBLE_PARKING          = 20,
};

// Human-readable name for a StringField index (for debug output).
inline const char* text_field_name(int idx) {
    static const char* names[NUM_TEXT_FIELDS] = {
        "Plate ID",
        "Vehicle Body Type",
        "Vehicle Make",
        "Violation Location",
        "Issuer Command",
        "Violation Time",
        "Time First Observed",
        "House Number",
        "Street Name",
        "Intersecting Street",
        "Sub Division",
        "Days Parking In Effect",
        "From Hours In Effect",
        "To Hours In Effect",
        "Vehicle Color",
        "Meter Number",
        "Violation Post Code",
        "Violation Description",
        "No Standing or Stopping Violation",
        "Hydrant Violation",
        "Double Parking Violation",
    };
    if (idx >= 0 && idx < NUM_TEXT_FIELDS) return names[idx];
    return "Unknown";
}

// --- Violation record struct ---

// A single parking violation record using primitive types only.
// Low-cardinality text fields are encoded as uint8_t enum IDs.
// High-cardinality string fields are stored in an external TextPool
// as (offset, length) pairs -- use StringField enum to index.
// Layout: Array-of-Structs (AoS), one struct per CSV row.
struct ViolationRecord {
    // Numeric fields
    uint64_t summons_number;            // 10-digit ID
    uint32_t issue_date;                // YYYYMMDD
    uint32_t street_code1;
    uint32_t street_code2;
    uint32_t street_code3;
    uint32_t vehicle_expiration_date;   // YYYYMMDD
    uint32_t issuer_code;
    uint32_t date_first_observed;       // YYYYMMDD
    uint16_t violation_code;
    uint16_t violation_precinct;
    uint16_t issuer_precinct;
    uint16_t law_section;
    uint16_t vehicle_year;
    uint16_t feet_from_curb;
    uint16_t fiscal_year;

    // Enum-encoded fields
    uint8_t  registration_state;        // 65 codes
    uint8_t  plate_type;                // 66 codes
    uint8_t  issuing_agency;            // 25 codes
    uint8_t  issuer_squad;              // 42 codes
    uint8_t  violation_county;          // 5 boroughs
    uint8_t  violation_front_opposite;  // 6 codes
    uint8_t  violation_legal_code;      // 2 values
    uint8_t  unregistered_vehicle;      // 2 values

    // String field references (into external TextPool)
    uint32_t str_offsets[NUM_TEXT_FIELDS];
    uint8_t  str_lengths[NUM_TEXT_FIELDS]; // max 71

    ViolationRecord();

    static constexpr int FIELD_COUNT = 44;
};

} // namespace parking

#endif // PARKING_RECORD_HPP
