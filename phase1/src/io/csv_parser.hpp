#ifndef PARKING_CSV_PARSER_HPP
#define PARKING_CSV_PARSER_HPP

#include <cstdint>
#include <cstddef>

namespace parking::io {

/// Maximum number of fields per CSV line.
constexpr int MAX_FIELDS = 48;

/// Represents a single parsed field as a pointer+length into the line buffer.
/// No copies are made — the field points directly into the original CSV data.
struct FieldView {
    const char* data;
    int length;
};

/// Low-level CSV parser for the merged parking violations CSV.
///
/// Handles:
///   - Double-quoted fields ("value")
///   - Empty fields (adjacent commas: ,,)
///   - Fields containing commas inside quotes
///
/// All parsing is zero-copy: fields are returned as pointer+length pairs
/// into the original line buffer.
class CsvParser {
public:
    // ── Line parsing ────────────────────────────────────────────────────

    /// Parse a single CSV line into an array of FieldView.
    /// Returns the number of fields extracted.
    /// @param line     Pointer to the start of the line
    /// @param len      Length of the line (excluding newline)
    /// @param fields   Output array of FieldView (must hold MAX_FIELDS)
    /// @return         Number of fields parsed
    static int parse_line(const char* line, int len, FieldView* fields);

    // ── Type conversions ────────────────────────────────────────────────
    // All converters handle empty/null input by returning 0.

    /// Convert a string to uint64_t. Manual digit loop (faster than stoull).
    static uint64_t to_uint64(const char* str, int len);

    /// Convert a string to uint32_t.
    static uint32_t to_uint32(const char* str, int len);

    /// Convert a string to uint16_t.
    static uint16_t to_uint16(const char* str, int len);

    /// Copy a field into a fixed-size null-terminated char buffer.
    /// Truncates if source exceeds dest_size-1.
    static void to_char_field(const char* src, int src_len,
                              char* dest, int dest_size);

    // ── Null detection ──────────────────────────────────────────────────

    /// Check if a value is a known null representation.
    /// Recognizes: "", "0", "00000000", "88888888"
    static bool is_null_date(const char* str, int len);
};

} // namespace parking::io

#endif // PARKING_CSV_PARSER_HPP
