#ifndef PARKING_TEXT_REF_HPP
#define PARKING_TEXT_REF_HPP

#include <cstdint>

namespace parking::core {

/// Number of text (string) fields stored in the global TextPool.
constexpr int NUM_TEXT_FIELDS = 21;

/// Enum indexing the 21 string fields within ViolationRecord's
/// str_offsets[] and str_lengths[] arrays.
///
/// These map to the original CSV columns that have high cardinality
/// (too many unique values for uint8_t enum encoding) and are stored
/// as raw text in a global pool rather than inline char arrays.
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

/// Return a human-readable name for a StringField index (for debug output).
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

} // namespace parking::core

#endif // PARKING_TEXT_REF_HPP
