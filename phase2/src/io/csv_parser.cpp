#include "io/csv_parser.hpp"
#include <cstring>

namespace parking::io {

// ── Line parsing ────────────────────────────────────────────────────────────
//
// State machine with two states:
//   UNQUOTED: comma ends field, quote at start enters QUOTED
//   QUOTED:   quote followed by comma/EOL ends field,
//             quote followed by quote is escaped quote (skip both)
//
// Fields are returned as (pointer, length) into the original buffer,
// pointing past any opening quote, length excluding any closing quote.

int CsvParser::parse_line(const char* line, int len, FieldView* fields) {
    int field_count = 0;
    int pos = 0;
    bool consumed_comma = false;

    while (pos < len && field_count < MAX_FIELDS) {
        consumed_comma = false;

        if (line[pos] == '"') {
            // Quoted field — find matching close quote
            pos++;  // skip opening quote
            int start = pos;

            while (pos < len) {
                if (line[pos] == '"') {
                    if (pos + 1 < len && line[pos + 1] == '"') {
                        // Escaped quote "" — skip both
                        pos += 2;
                        continue;
                    }
                    // Closing quote
                    break;
                }
                pos++;
            }

            fields[field_count].data = line + start;
            fields[field_count].length = pos - start;
            field_count++;

            // Skip closing quote and delimiter
            if (pos < len) pos++;  // skip "
            if (pos < len && line[pos] == ',') {
                pos++;  // skip ,
                consumed_comma = true;
            }

        } else {
            // Unquoted field — find next comma or end
            int start = pos;

            while (pos < len && line[pos] != ',') {
                pos++;
            }

            fields[field_count].data = line + start;
            fields[field_count].length = pos - start;
            field_count++;

            if (pos < len) {
                pos++;  // skip comma
                consumed_comma = true;
            }
        }
    }

    // If the line ended with a comma, there's a trailing empty field
    if (consumed_comma && pos == len && field_count < MAX_FIELDS) {
        fields[field_count].data = line + pos;
        fields[field_count].length = 0;
        field_count++;
    }

    return field_count;
}

// ── Type conversions ────────────────────────────────────────────────────────

uint64_t CsvParser::to_uint64(const char* str, int len) {
    if (len <= 0) return 0;
    uint64_t result = 0;
    for (int i = 0; i < len; ++i) {
        char c = str[i];
        if (c < '0' || c > '9') return 0;  // non-digit = invalid
        result = result * 10 + (c - '0');
    }
    return result;
}

uint32_t CsvParser::to_uint32(const char* str, int len) {
    if (len <= 0) return 0;
    uint32_t result = 0;
    for (int i = 0; i < len; ++i) {
        char c = str[i];
        if (c < '0' || c > '9') return 0;
        result = result * 10 + (c - '0');
    }
    return result;
}

uint16_t CsvParser::to_uint16(const char* str, int len) {
    if (len <= 0) return 0;
    uint16_t result = 0;
    for (int i = 0; i < len; ++i) {
        char c = str[i];
        if (c < '0' || c > '9') return 0;
        result = result * 10 + (c - '0');
    }
    return result;
}

void CsvParser::to_char_field(const char* src, int src_len,
                              char* dest, int dest_size) {
    if (dest_size <= 0) return;

    int copy_len = src_len;
    if (copy_len >= dest_size) {
        copy_len = dest_size - 1;  // leave room for null terminator
    }

    if (copy_len > 0) {
        std::memcpy(dest, src, copy_len);
    }
    dest[copy_len] = '\0';
}

// ── Null detection ──────────────────────────────────────────────────────────

bool CsvParser::is_null_date(const char* str, int len) {
    if (len == 0) return true;
    if (len == 1 && str[0] == '0') return true;
    if (len == 8) {
        // "00000000"
        if (str[0] == '0' && str[1] == '0' && str[2] == '0' && str[3] == '0' &&
            str[4] == '0' && str[5] == '0' && str[6] == '0' && str[7] == '0')
            return true;
        // "88888888"
        if (str[0] == '8' && str[1] == '8' && str[2] == '8' && str[3] == '8' &&
            str[4] == '8' && str[5] == '8' && str[6] == '8' && str[7] == '8')
            return true;
    }
    return false;
}

} // namespace parking::io
