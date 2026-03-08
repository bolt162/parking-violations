#ifndef PARKING_CSV_READER_HPP
#define PARKING_CSV_READER_HPP

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include "record.hpp"

namespace parking {

class DataStore;
class TextPool;

constexpr int MAX_FIELDS = 48;

// A parsed field as a pointer+length into the line buffer (zero-copy).
struct FieldView {
    const char* data;
    int length;
};

// Low-level CSV parser. Handles double-quoted fields, empty fields,
// and commas inside quotes. All parsing is zero-copy.
class CsvParser {
public:
    static int parse_line(const char* line, int len, FieldView* fields);

    static uint64_t to_uint64(const char* str, int len);
    static uint32_t to_uint32(const char* str, int len);
    static uint16_t to_uint16(const char* str, int len);

    static void to_char_field(const char* src, int src_len,
                              char* dest, int dest_size);

    // Check if a value is a known null date ("", "0", "00000000", "88888888").
    static bool is_null_date(const char* str, int len);
};

// Reads the merged/normalized parking violations CSV (two-pass, line-by-line).
// Pass 1 counts lines for pre-allocation, pass 2 parses records.
class CsvReader {
public:
    CsvReader() = default;

    size_t read(const std::string& filepath,
                std::vector<ViolationRecord>& records,
                TextPool& pool);

    int expected_columns() const { return 44; }

private:
    static bool populate_record(const char* line, int line_len,
                                ViolationRecord& rec,
                                TextPool& pool);
};

// Convenience function: load a CSV file into a DataStore.
size_t load_csv(const std::string& filepath, DataStore& store);

} // namespace parking

#endif // PARKING_CSV_READER_HPP
