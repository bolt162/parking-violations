#include "io/csv_reader.hpp"
#include "io/csv_parser.hpp"
#include "core/field_types.hpp"
#include "core/text_ref.hpp"

#include <fstream>
#include <iostream>
#include <string>
#include <chrono>

namespace parking::io {

// ── Helper: map parsed fields to a ViolationRecord ──────────────────────────

bool SingleFileReader::populate_record(const char* line, int line_len,
                                       core::ViolationRecord& rec,
                                       data::TextPool& pool) {
    FieldView fields[MAX_FIELDS];
    int n = CsvParser::parse_line(line, line_len, fields);

    if (n < 43) {
        // Too few fields — malformed line, skip it
        return false;
    }

    // ── Numeric fields ──────────────────────────────────────────────────

    rec.summons_number = CsvParser::to_uint64(
        fields[static_cast<int>(core::Column::SUMMONS_NUMBER)].data,
        fields[static_cast<int>(core::Column::SUMMONS_NUMBER)].length);

    // Issue Date
    {
        auto& f = fields[static_cast<int>(core::Column::ISSUE_DATE)];
        if (CsvParser::is_null_date(f.data, f.length))
            rec.issue_date = core::NULL_DATE;
        else
            rec.issue_date = CsvParser::to_uint32(f.data, f.length);
    }

    rec.street_code1 = CsvParser::to_uint32(
        fields[static_cast<int>(core::Column::STREET_CODE1)].data,
        fields[static_cast<int>(core::Column::STREET_CODE1)].length);

    rec.street_code2 = CsvParser::to_uint32(
        fields[static_cast<int>(core::Column::STREET_CODE2)].data,
        fields[static_cast<int>(core::Column::STREET_CODE2)].length);

    rec.street_code3 = CsvParser::to_uint32(
        fields[static_cast<int>(core::Column::STREET_CODE3)].data,
        fields[static_cast<int>(core::Column::STREET_CODE3)].length);

    // Vehicle Expiration Date
    {
        auto& f = fields[static_cast<int>(core::Column::VEHICLE_EXPIRATION_DATE)];
        if (CsvParser::is_null_date(f.data, f.length))
            rec.vehicle_expiration_date = core::NULL_DATE;
        else
            rec.vehicle_expiration_date = CsvParser::to_uint32(f.data, f.length);
    }

    rec.issuer_code = CsvParser::to_uint32(
        fields[static_cast<int>(core::Column::ISSUER_CODE)].data,
        fields[static_cast<int>(core::Column::ISSUER_CODE)].length);

    // Date First Observed
    {
        auto& f = fields[static_cast<int>(core::Column::DATE_FIRST_OBSERVED)];
        if (CsvParser::is_null_date(f.data, f.length))
            rec.date_first_observed = core::NULL_DATE;
        else
            rec.date_first_observed = CsvParser::to_uint32(f.data, f.length);
    }

    rec.violation_code = CsvParser::to_uint16(
        fields[static_cast<int>(core::Column::VIOLATION_CODE)].data,
        fields[static_cast<int>(core::Column::VIOLATION_CODE)].length);

    rec.violation_precinct = CsvParser::to_uint16(
        fields[static_cast<int>(core::Column::VIOLATION_PRECINCT)].data,
        fields[static_cast<int>(core::Column::VIOLATION_PRECINCT)].length);

    rec.issuer_precinct = CsvParser::to_uint16(
        fields[static_cast<int>(core::Column::ISSUER_PRECINCT)].data,
        fields[static_cast<int>(core::Column::ISSUER_PRECINCT)].length);

    rec.law_section = CsvParser::to_uint16(
        fields[static_cast<int>(core::Column::LAW_SECTION)].data,
        fields[static_cast<int>(core::Column::LAW_SECTION)].length);

    rec.vehicle_year = CsvParser::to_uint16(
        fields[static_cast<int>(core::Column::VEHICLE_YEAR)].data,
        fields[static_cast<int>(core::Column::VEHICLE_YEAR)].length);

    rec.feet_from_curb = CsvParser::to_uint16(
        fields[static_cast<int>(core::Column::FEET_FROM_CURB)].data,
        fields[static_cast<int>(core::Column::FEET_FROM_CURB)].length);

    // Fiscal Year (column 43 — may not be present in 43-column files)
    if (n >= 44) {
        rec.fiscal_year = CsvParser::to_uint16(
            fields[static_cast<int>(core::Column::FISCAL_YEAR)].data,
            fields[static_cast<int>(core::Column::FISCAL_YEAR)].length);
    }

    // ── Enum-encoded fields ─────────────────────────────────────────────

    {
        auto& f = fields[static_cast<int>(core::Column::REGISTRATION_STATE)];
        rec.registration_state = core::state_to_enum(f.data, f.length);
    }
    {
        auto& f = fields[static_cast<int>(core::Column::PLATE_TYPE)];
        rec.plate_type = core::plate_type_to_enum(f.data, f.length);
    }
    {
        auto& f = fields[static_cast<int>(core::Column::ISSUING_AGENCY)];
        rec.issuing_agency = core::agency_to_enum(f.data, f.length);
    }
    {
        auto& f = fields[static_cast<int>(core::Column::ISSUER_SQUAD)];
        rec.issuer_squad = core::squad_to_enum(f.data, f.length);
    }
    {
        auto& f = fields[static_cast<int>(core::Column::VIOLATION_COUNTY)];
        rec.violation_county = core::county_to_enum(f.data, f.length);
    }
    {
        auto& f = fields[static_cast<int>(core::Column::VIOLATION_FRONT_OPPOSITE)];
        rec.violation_front_opposite = core::front_opposite_to_enum(f.data, f.length);
    }
    {
        auto& f = fields[static_cast<int>(core::Column::VIOLATION_LEGAL_CODE)];
        rec.violation_legal_code = core::legal_code_to_enum(f.data, f.length);
    }
    {
        auto& f = fields[static_cast<int>(core::Column::UNREGISTERED_VEHICLE)];
        rec.unregistered_vehicle = core::unreg_vehicle_to_enum(f.data, f.length);
    }

    // ── String fields → append to TextPool ──────────────────────────────

    #define POOL_FIELD(col, field_idx) { \
        auto& f = fields[static_cast<int>(core::Column::col)]; \
        rec.str_offsets[field_idx] = pool.append(f.data, f.length); \
        rec.str_lengths[field_idx] = static_cast<uint8_t>(f.length); \
    }

    POOL_FIELD(PLATE_ID,                core::SF_PLATE_ID)
    POOL_FIELD(VEHICLE_BODY_TYPE,       core::SF_VEHICLE_BODY_TYPE)
    POOL_FIELD(VEHICLE_MAKE,            core::SF_VEHICLE_MAKE)
    POOL_FIELD(VIOLATION_LOCATION,      core::SF_VIOLATION_LOCATION)
    POOL_FIELD(ISSUER_COMMAND,          core::SF_ISSUER_COMMAND)
    POOL_FIELD(VIOLATION_TIME,          core::SF_VIOLATION_TIME)
    POOL_FIELD(TIME_FIRST_OBSERVED,     core::SF_TIME_FIRST_OBSERVED)
    POOL_FIELD(HOUSE_NUMBER,            core::SF_HOUSE_NUMBER)
    POOL_FIELD(STREET_NAME,             core::SF_STREET_NAME)
    POOL_FIELD(INTERSECTING_STREET,     core::SF_INTERSECTING_STREET)
    POOL_FIELD(SUB_DIVISION,            core::SF_SUB_DIVISION)
    POOL_FIELD(DAYS_PARKING_IN_EFFECT,  core::SF_DAYS_PARKING_IN_EFFECT)
    POOL_FIELD(FROM_HOURS_IN_EFFECT,    core::SF_FROM_HOURS_IN_EFFECT)
    POOL_FIELD(TO_HOURS_IN_EFFECT,      core::SF_TO_HOURS_IN_EFFECT)
    POOL_FIELD(VEHICLE_COLOR,           core::SF_VEHICLE_COLOR)
    POOL_FIELD(METER_NUMBER,            core::SF_METER_NUMBER)
    POOL_FIELD(VIOLATION_POST_CODE,     core::SF_VIOLATION_POST_CODE)
    POOL_FIELD(VIOLATION_DESCRIPTION,   core::SF_VIOLATION_DESCRIPTION)
    POOL_FIELD(NO_STANDING_STOPPING,    core::SF_NO_STANDING_STOPPING)
    POOL_FIELD(HYDRANT_VIOLATION,       core::SF_HYDRANT_VIOLATION)
    POOL_FIELD(DOUBLE_PARKING,          core::SF_DOUBLE_PARKING)

    #undef POOL_FIELD

    return true;
}

// ── File reading (line-by-line, two-pass) ────────────────────────────────────

size_t SingleFileReader::read(const std::string& filepath,
                              std::vector<core::ViolationRecord>& records,
                              data::TextPool& pool) {

    auto t_start = std::chrono::high_resolution_clock::now();

    // ── Pass 1: Count lines for pre-allocation ──────────────────────────

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

    // line_count includes header, so data lines = line_count - 1
    size_t data_lines = (line_count > 0) ? line_count - 1 : 0;
    std::cout << " done (" << line_count << " lines, "
              << count_sec << "s)" << std::endl;

    // Pre-allocate
    records.reserve(records.size() + data_lines);
    // TextPool: estimate ~70 bytes of text per record
    pool.reserve(pool.size() + data_lines * 70);

    // ── Pass 2: Parse line by line ──────────────────────────────────────

    std::cout << "  Pass 2: Parsing " << data_lines << " records..."
              << std::flush;

    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "ERROR: Cannot reopen file: " << filepath << std::endl;
        return 0;
    }

    // Skip header
    std::getline(file, line);

    size_t parsed = 0;
    size_t skipped = 0;

    while (std::getline(file, line)) {
        if (line.empty()) {
            skipped++;
            continue;
        }

        // Trim trailing \r if present (Windows line endings)
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        core::ViolationRecord rec;
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

} // namespace parking::io
