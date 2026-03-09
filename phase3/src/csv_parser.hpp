#ifndef CSV_PARSER_HPP
#define CSV_PARSER_HPP

#include <cstdint>
#include <cstring>

namespace parking {

constexpr int MAX_FIELDS = 48;

struct FieldView {
    const char* data;
    int length;
};

// CSV parsing utilities - same as Phase 2
namespace csv {

inline int parse_line(const char* line, int len, FieldView* fields) {
    int count = 0, pos = 0;
    bool ate_comma = false;

    while (pos < len && count < MAX_FIELDS) {
        ate_comma = false;
        if (line[pos] == '"') {
            int start = ++pos;
            while (pos < len) {
                if (line[pos] == '"') {
                    if (pos + 1 < len && line[pos + 1] == '"') { pos += 2; continue; }
                    break;
                }
                pos++;
            }
            fields[count++] = {line + start, pos - start};
            if (pos < len) pos++;
            if (pos < len && line[pos] == ',') { pos++; ate_comma = true; }
        } else {
            int start = pos;
            while (pos < len && line[pos] != ',') pos++;
            fields[count++] = {line + start, pos - start};
            if (pos < len) { pos++; ate_comma = true; }
        }
    }
    if (ate_comma && pos == len && count < MAX_FIELDS)
        fields[count++] = {line + pos, 0};
    return count;
}

inline uint64_t to_u64(const char* s, int len) {
    uint64_t r = 0;
    for (int i = 0; i < len; ++i) {
        if (s[i] < '0' || s[i] > '9') return 0;
        r = r * 10 + (s[i] - '0');
    }
    return r;
}

inline uint32_t to_u32(const char* s, int len) {
    return static_cast<uint32_t>(to_u64(s, len));
}

inline uint16_t to_u16(const char* s, int len) {
    return static_cast<uint16_t>(to_u64(s, len));
}

inline bool is_null_date(const char* s, int len) {
    if (len == 0) return true;
    if (len == 1 && s[0] == '0') return true;
    if (len == 8 && (std::memcmp(s, "00000000", 8) == 0 ||
                     std::memcmp(s, "88888888", 8) == 0)) return true;
    return false;
}

} // namespace csv
} // namespace parking

#endif
