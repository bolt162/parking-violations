#include "io/csv_reader.hpp"
#include "io/csv_parser.hpp"
#include "core/field_types.hpp"
#include "core/text_ref.hpp"

#include <fstream>
#include <iostream>
#include <string>
#include <chrono>
#include <vector>
#include <omp.h>

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

// ── File reading (two-pass with chunked parallel parsing) ────────────────────
//
// Instead of buffering ALL 41.7M lines into a vector<string> (~8-9 GB),
// we read lines in chunks of 1M, parse each chunk in parallel, merge into
// the global output, then free the chunk. Peak overhead: ~200 MB instead of 9 GB.

size_t SingleFileReader::read(const std::string& filepath,
                              std::vector<core::ViolationRecord>& records,
                              data::TextPool& pool) {

    auto t_start = std::chrono::high_resolution_clock::now();
    int num_threads = omp_get_max_threads();

    static constexpr size_t CHUNK_SIZE = 1000000;  // 1M lines per chunk

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

    size_t data_lines = (line_count > 0) ? line_count - 1 : 0;
    std::cout << " done (" << line_count << " lines, "
              << count_sec << "s)" << std::endl;

    // Pre-allocate output containers
    records.reserve(records.size() + data_lines);
    pool.reserve(pool.size() + data_lines * 70);

    // ── Pass 2: Chunked read + parallel parse ───────────────────────────

    std::cout << "  Pass 2: Chunked parallel parsing (" << num_threads
              << " threads, chunk=" << CHUNK_SIZE << ")..." << std::endl;

    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "ERROR: Cannot reopen file: " << filepath << std::endl;
        return 0;
    }

    // Skip header
    std::getline(file, line);

    // Reusable chunk buffer and thread-local storage
    std::vector<std::string> chunk;
    chunk.reserve(CHUNK_SIZE);

    std::vector<std::vector<core::ViolationRecord>> thread_records(num_threads);
    std::vector<data::TextPool> thread_pools(num_threads);

    size_t total_parsed = 0;
    size_t total_skipped = 0;
    double parse_time = 0.0;
    double merge_time = 0.0;

    while (!file.eof()) {
        // ── Read a chunk of lines (sequential I/O) ──────────────────
        chunk.clear();
        while (chunk.size() < CHUNK_SIZE && std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            chunk.push_back(std::move(line));
        }

        if (chunk.empty()) break;
        const size_t chunk_lines = chunk.size();

        // ── Parallel parse this chunk ───────────────────────────────
        auto tp0 = std::chrono::high_resolution_clock::now();

        for (int t = 0; t < num_threads; ++t) {
            thread_records[t].clear();
            thread_pools[t].clear();
        }

        #pragma omp parallel
        {
            int tid = omp_get_thread_num();
            auto& local_recs = thread_records[tid];
            auto& local_pool = thread_pools[tid];

            #pragma omp for schedule(static)
            for (size_t i = 0; i < chunk_lines; ++i) {
                const std::string& l = chunk[i];
                if (l.empty()) continue;

                core::ViolationRecord rec;
                if (populate_record(l.c_str(), static_cast<int>(l.size()),
                                    rec, local_pool)) {
                    local_recs.push_back(rec);
                }
            }
        }

        auto tp1 = std::chrono::high_resolution_clock::now();
        parse_time += std::chrono::duration<double>(tp1 - tp0).count();

        // ── Merge thread-local results into global output ───────────
        auto tm0 = std::chrono::high_resolution_clock::now();

        size_t chunk_parsed = 0;
        for (int t = 0; t < num_threads; ++t) {
            if (thread_records[t].empty()) continue;

            uint32_t base_offset = 0;
            if (thread_pools[t].size() > 0) {
                base_offset = pool.append_bulk(
                    thread_pools[t].data_ptr(), thread_pools[t].size());
            }

            for (auto& rec : thread_records[t]) {
                if (base_offset > 0) {
                    for (int f = 0; f < core::NUM_TEXT_FIELDS; ++f) {
                        if (rec.str_lengths[f] > 0) {
                            rec.str_offsets[f] += base_offset;
                        }
                    }
                }
                records.push_back(std::move(rec));
            }
            chunk_parsed += thread_records[t].size();
        }

        auto tm1 = std::chrono::high_resolution_clock::now();
        merge_time += std::chrono::duration<double>(tm1 - tm0).count();

        total_parsed += chunk_parsed;
        total_skipped += chunk_lines - chunk_parsed;

        if (total_parsed % 5000000 < CHUNK_SIZE) {
            std::cout << "    ... " << total_parsed << " records parsed"
                      << std::endl;
        }
    }

    file.close();

    auto t_end = std::chrono::high_resolution_clock::now();
    double read_parse_sec = std::chrono::duration<double>(t_end - t_count).count();
    double total_sec = std::chrono::duration<double>(t_end - t_start).count();

    std::cout << "  Done: " << total_parsed << " records parsed, "
              << total_skipped << " skipped" << std::endl;
    std::cout << "  Timing: count=" << count_sec << "s, read+parse="
              << read_parse_sec << "s (parse=" << parse_time
              << "s, merge=" << merge_time << "s), total="
              << total_sec << "s" << std::endl;
    std::cout << "  TextPool: " << pool.size_mb() << " MB ("
              << pool.size() << " bytes)" << std::endl;

    return total_parsed;
}

} // namespace parking::io
