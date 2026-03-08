#include "csv_reader.hpp"
#include "parking.hpp"

#include <fstream>
#include <iostream>
#include <string>
#include <chrono>
#include <cstring>

namespace parking {

// --- CsvParser ---

// State machine: UNQUOTED (comma ends field) and QUOTED (close-quote ends field).
// Fields are returned as (pointer, length) into the original buffer.
int CsvParser::parse_line(const char* line, int len, FieldView* fields) {
    int field_count = 0;
    int pos = 0;
    bool consumed_comma = false;

    while (pos < len && field_count < MAX_FIELDS) {
        consumed_comma = false;

        if (line[pos] == '"') {
            pos++;  // skip opening quote
            int start = pos;

            while (pos < len) {
                if (line[pos] == '"') {
                    if (pos + 1 < len && line[pos + 1] == '"') {
                        pos += 2;  // escaped quote ""
                        continue;
                    }
                    break;  // closing quote
                }
                pos++;
            }

            fields[field_count].data = line + start;
            fields[field_count].length = pos - start;
            field_count++;

            if (pos < len) pos++;  // skip closing "
            if (pos < len && line[pos] == ',') {
                pos++;
                consumed_comma = true;
            }

        } else {
            int start = pos;
            while (pos < len && line[pos] != ',') {
                pos++;
            }

            fields[field_count].data = line + start;
            fields[field_count].length = pos - start;
            field_count++;

            if (pos < len) {
                pos++;
                consumed_comma = true;
            }
        }
    }

    // Trailing empty field after final comma
    if (consumed_comma && pos == len && field_count < MAX_FIELDS) {
        fields[field_count].data = line + pos;
        fields[field_count].length = 0;
        field_count++;
    }

    return field_count;
}

uint64_t CsvParser::to_uint64(const char* str, int len) {
    if (len <= 0) return 0;
    uint64_t result = 0;
    for (int i = 0; i < len; ++i) {
        char c = str[i];
        if (c < '0' || c > '9') return 0;
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
    if (copy_len >= dest_size) copy_len = dest_size - 1;
    if (copy_len > 0) std::memcpy(dest, src, copy_len);
    dest[copy_len] = '\0';
}

bool CsvParser::is_null_date(const char* str, int len) {
    if (len == 0) return true;
    if (len == 1 && str[0] == '0') return true;
    if (len == 8) {
        if (str[0] == '0' && str[1] == '0' && str[2] == '0' && str[3] == '0' &&
            str[4] == '0' && str[5] == '0' && str[6] == '0' && str[7] == '0')
            return true;
        if (str[0] == '8' && str[1] == '8' && str[2] == '8' && str[3] == '8' &&
            str[4] == '8' && str[5] == '8' && str[6] == '8' && str[7] == '8')
            return true;
    }
    return false;
}

// --- CsvReader ---

// Helper to store a text field into the pool and record its offset/length.
static void store_text(const FieldView& f, int field_idx,
                       ViolationRecord& rec, TextPool& pool) {
    rec.str_offsets[field_idx] = pool.append(f.data, f.length);
    rec.str_lengths[field_idx] = static_cast<uint8_t>(f.length);
}

bool CsvReader::populate_record(const char* line, int line_len,
                                ViolationRecord& rec,
                                TextPool& pool) {
    FieldView fields[MAX_FIELDS];
    int n = CsvParser::parse_line(line, line_len, fields);

    if (n < 43) return false;  // malformed line

    // Numeric fields
    rec.summons_number = CsvParser::to_uint64(
        fields[static_cast<int>(Column::SUMMONS_NUMBER)].data,
        fields[static_cast<int>(Column::SUMMONS_NUMBER)].length);

    {
        auto& f = fields[static_cast<int>(Column::ISSUE_DATE)];
        rec.issue_date = CsvParser::is_null_date(f.data, f.length)
            ? NULL_DATE : CsvParser::to_uint32(f.data, f.length);
    }

    rec.street_code1 = CsvParser::to_uint32(
        fields[static_cast<int>(Column::STREET_CODE1)].data,
        fields[static_cast<int>(Column::STREET_CODE1)].length);

    rec.street_code2 = CsvParser::to_uint32(
        fields[static_cast<int>(Column::STREET_CODE2)].data,
        fields[static_cast<int>(Column::STREET_CODE2)].length);

    rec.street_code3 = CsvParser::to_uint32(
        fields[static_cast<int>(Column::STREET_CODE3)].data,
        fields[static_cast<int>(Column::STREET_CODE3)].length);

    {
        auto& f = fields[static_cast<int>(Column::VEHICLE_EXPIRATION_DATE)];
        rec.vehicle_expiration_date = CsvParser::is_null_date(f.data, f.length)
            ? NULL_DATE : CsvParser::to_uint32(f.data, f.length);
    }

    rec.issuer_code = CsvParser::to_uint32(
        fields[static_cast<int>(Column::ISSUER_CODE)].data,
        fields[static_cast<int>(Column::ISSUER_CODE)].length);

    {
        auto& f = fields[static_cast<int>(Column::DATE_FIRST_OBSERVED)];
        rec.date_first_observed = CsvParser::is_null_date(f.data, f.length)
            ? NULL_DATE : CsvParser::to_uint32(f.data, f.length);
    }

    rec.violation_code = CsvParser::to_uint16(
        fields[static_cast<int>(Column::VIOLATION_CODE)].data,
        fields[static_cast<int>(Column::VIOLATION_CODE)].length);

    rec.violation_precinct = CsvParser::to_uint16(
        fields[static_cast<int>(Column::VIOLATION_PRECINCT)].data,
        fields[static_cast<int>(Column::VIOLATION_PRECINCT)].length);

    rec.issuer_precinct = CsvParser::to_uint16(
        fields[static_cast<int>(Column::ISSUER_PRECINCT)].data,
        fields[static_cast<int>(Column::ISSUER_PRECINCT)].length);

    rec.law_section = CsvParser::to_uint16(
        fields[static_cast<int>(Column::LAW_SECTION)].data,
        fields[static_cast<int>(Column::LAW_SECTION)].length);

    rec.vehicle_year = CsvParser::to_uint16(
        fields[static_cast<int>(Column::VEHICLE_YEAR)].data,
        fields[static_cast<int>(Column::VEHICLE_YEAR)].length);

    rec.feet_from_curb = CsvParser::to_uint16(
        fields[static_cast<int>(Column::FEET_FROM_CURB)].data,
        fields[static_cast<int>(Column::FEET_FROM_CURB)].length);

    if (n >= 44) {
        rec.fiscal_year = CsvParser::to_uint16(
            fields[static_cast<int>(Column::FISCAL_YEAR)].data,
            fields[static_cast<int>(Column::FISCAL_YEAR)].length);
    }

    // Enum-encoded fields
    {
        auto& f = fields[static_cast<int>(Column::REGISTRATION_STATE)];
        rec.registration_state = state_to_enum(f.data, f.length);
    }
    {
        auto& f = fields[static_cast<int>(Column::PLATE_TYPE)];
        rec.plate_type = plate_type_to_enum(f.data, f.length);
    }
    {
        auto& f = fields[static_cast<int>(Column::ISSUING_AGENCY)];
        rec.issuing_agency = agency_to_enum(f.data, f.length);
    }
    {
        auto& f = fields[static_cast<int>(Column::ISSUER_SQUAD)];
        rec.issuer_squad = squad_to_enum(f.data, f.length);
    }
    {
        auto& f = fields[static_cast<int>(Column::VIOLATION_COUNTY)];
        rec.violation_county = county_to_enum(f.data, f.length);
    }
    {
        auto& f = fields[static_cast<int>(Column::VIOLATION_FRONT_OPPOSITE)];
        rec.violation_front_opposite = front_opposite_to_enum(f.data, f.length);
    }
    {
        auto& f = fields[static_cast<int>(Column::VIOLATION_LEGAL_CODE)];
        rec.violation_legal_code = legal_code_to_enum(f.data, f.length);
    }
    {
        auto& f = fields[static_cast<int>(Column::UNREGISTERED_VEHICLE)];
        rec.unregistered_vehicle = unreg_vehicle_to_enum(f.data, f.length);
    }

    // String fields -> TextPool
    store_text(fields[static_cast<int>(Column::PLATE_ID)],                SF_PLATE_ID, rec, pool);
    store_text(fields[static_cast<int>(Column::VEHICLE_BODY_TYPE)],       SF_VEHICLE_BODY_TYPE, rec, pool);
    store_text(fields[static_cast<int>(Column::VEHICLE_MAKE)],            SF_VEHICLE_MAKE, rec, pool);
    store_text(fields[static_cast<int>(Column::VIOLATION_LOCATION)],      SF_VIOLATION_LOCATION, rec, pool);
    store_text(fields[static_cast<int>(Column::ISSUER_COMMAND)],          SF_ISSUER_COMMAND, rec, pool);
    store_text(fields[static_cast<int>(Column::VIOLATION_TIME)],          SF_VIOLATION_TIME, rec, pool);
    store_text(fields[static_cast<int>(Column::TIME_FIRST_OBSERVED)],     SF_TIME_FIRST_OBSERVED, rec, pool);
    store_text(fields[static_cast<int>(Column::HOUSE_NUMBER)],            SF_HOUSE_NUMBER, rec, pool);
    store_text(fields[static_cast<int>(Column::STREET_NAME)],             SF_STREET_NAME, rec, pool);
    store_text(fields[static_cast<int>(Column::INTERSECTING_STREET)],     SF_INTERSECTING_STREET, rec, pool);
    store_text(fields[static_cast<int>(Column::SUB_DIVISION)],            SF_SUB_DIVISION, rec, pool);
    store_text(fields[static_cast<int>(Column::DAYS_PARKING_IN_EFFECT)],  SF_DAYS_PARKING_IN_EFFECT, rec, pool);
    store_text(fields[static_cast<int>(Column::FROM_HOURS_IN_EFFECT)],    SF_FROM_HOURS_IN_EFFECT, rec, pool);
    store_text(fields[static_cast<int>(Column::TO_HOURS_IN_EFFECT)],      SF_TO_HOURS_IN_EFFECT, rec, pool);
    store_text(fields[static_cast<int>(Column::VEHICLE_COLOR)],           SF_VEHICLE_COLOR, rec, pool);
    store_text(fields[static_cast<int>(Column::METER_NUMBER)],            SF_METER_NUMBER, rec, pool);
    store_text(fields[static_cast<int>(Column::VIOLATION_POST_CODE)],     SF_VIOLATION_POST_CODE, rec, pool);
    store_text(fields[static_cast<int>(Column::VIOLATION_DESCRIPTION)],   SF_VIOLATION_DESCRIPTION, rec, pool);
    store_text(fields[static_cast<int>(Column::NO_STANDING_STOPPING)],    SF_NO_STANDING_STOPPING, rec, pool);
    store_text(fields[static_cast<int>(Column::HYDRANT_VIOLATION)],       SF_HYDRANT_VIOLATION, rec, pool);
    store_text(fields[static_cast<int>(Column::DOUBLE_PARKING)],          SF_DOUBLE_PARKING, rec, pool);

    return true;
}

size_t CsvReader::read(const std::string& filepath,
                       std::vector<ViolationRecord>& records,
                       TextPool& pool) {

    auto t_start = std::chrono::high_resolution_clock::now();

    // Pass 1: count lines for pre-allocation
    std::cout << "  Pass 1: Counting lines in " << filepath << "..."
              << std::flush;

    std::ifstream count_file(filepath);
    if (!count_file.is_open()) {
        std::cerr << "ERROR: Cannot open file: " << filepath << std::endl;
        return 0;
    }

    size_t line_count = 0;
    std::string line;
    while (std::getline(count_file, line)) {
        line_count++;
    }
    count_file.close();

    auto t_count = std::chrono::high_resolution_clock::now();
    double count_sec = std::chrono::duration<double>(t_count - t_start).count();

    size_t data_lines = (line_count > 0) ? line_count - 1 : 0;
    std::cout << " done (" << line_count << " lines, "
              << count_sec << "s)" << std::endl;

    records.reserve(records.size() + data_lines);
    pool.reserve(pool.size() + data_lines * 70);

    // Pass 2: parse line by line
    std::cout << "  Pass 2: Parsing " << data_lines << " records..."
              << std::flush;

    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "ERROR: Cannot reopen file: " << filepath << std::endl;
        return 0;
    }

    std::getline(file, line);  // skip header

    size_t parsed = 0;
    size_t skipped = 0;

    while (std::getline(file, line)) {
        if (line.empty()) {
            skipped++;
            continue;
        }

        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        ViolationRecord rec;
        if (populate_record(line.c_str(), static_cast<int>(line.size()),
                            rec, pool)) {
            records.push_back(rec);
            parsed++;
        } else {
            skipped++;
        }

        if (parsed % 5000000 == 0 && parsed > 0) {
            std::cout << "\n    ... " << parsed << " records parsed"
                      << std::flush;
        }
    }

    file.close();

    auto t_parse = std::chrono::high_resolution_clock::now();
    double parse_sec = std::chrono::duration<double>(t_parse - t_count).count();
    double total_sec = std::chrono::duration<double>(t_parse - t_start).count();

    std::cout << "\n  Done: " << parsed << " records parsed, "
              << skipped << " skipped"
              << " (" << parse_sec << "s parse, "
              << total_sec << "s total)" << std::endl;
    std::cout << "  TextPool: " << pool.size_mb() << " MB ("
              << pool.size() << " bytes)" << std::endl;

    return parsed;
}

// --- load_csv (replaces FileLoader) ---

size_t load_csv(const std::string& filepath, DataStore& store) {
    std::cout << "Loading: " << filepath << std::endl;

    CsvReader reader;
    std::vector<ViolationRecord> records;
    size_t count = reader.read(filepath, records, store.text_pool());

    if (count > 0) {
        store.add_records(std::move(records));
    }

    double rec_gb = store.size() * sizeof(ViolationRecord)
                    / (1024.0 * 1024.0 * 1024.0);
    std::cout << "DataStore now holds " << store.size() << " records ("
              << rec_gb << " GB structs + "
              << store.text_pool().size_gb() << " GB text pool)" << std::endl;

    return count;
}

} // namespace parking
