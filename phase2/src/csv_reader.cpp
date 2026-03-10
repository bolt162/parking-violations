#include "csv_reader.hpp"
#include "parking.hpp"

#include <fstream>
#include <iostream>
#include <string>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <omp.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace parking {

int CsvParser::parse_line(const char* line, int len, FieldView* fields) {
    int field_count = 0;
    int pos = 0;
    bool consumed_comma = false;

    while (pos < len && field_count < MAX_FIELDS) {
        consumed_comma = false;

        if (line[pos] == '"') {
            pos++; 
            int start = pos;

            while (pos < len) {
                if (line[pos] == '"') {
                    if (pos + 1 < len && line[pos + 1] == '"') {
                        pos += 2;
                        continue;
                    }
                    break;
                }
                pos++;
            }

            fields[field_count].data = line + start;
            fields[field_count].length = pos - start;
            field_count++;

            if (pos < len) pos++;
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

    if (n < 43) return false;

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
    int num_threads = omp_get_max_threads();

    // -- Step 1: mmap the file --

    std::cout << "  Step 1: Memory-mapping " << filepath << "..."
              << std::flush;

    int fd = open(filepath.c_str(), O_RDONLY);
    if (fd < 0) {
        std::cerr << "\nERROR: Cannot open file: " << filepath << std::endl;
        return 0;
    }

    struct stat st;
    if (fstat(fd, &st) != 0) {
        std::cerr << "\nERROR: Cannot stat file: " << filepath << std::endl;
        close(fd);
        return 0;
    }
    size_t file_size = static_cast<size_t>(st.st_size);

    const char* file_data = static_cast<const char*>(
        mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0));
    if (file_data == MAP_FAILED) {
        std::cerr << "\nERROR: mmap failed for " << filepath << std::endl;
        close(fd);
        return 0;
    }
    close(fd);

    madvise(const_cast<void*>(static_cast<const void*>(file_data)),
            file_size, MADV_SEQUENTIAL);

    auto t_mmap = std::chrono::high_resolution_clock::now();
    double mmap_sec = std::chrono::duration<double>(t_mmap - t_start).count();
    double file_mb = file_size / (1024.0 * 1024.0);

    std::cout << " done (" << file_mb << " MB, " << mmap_sec << "s)"
              << std::endl;

    std::cout << "  Step 2: Scanning for line boundaries..." << std::flush;

    size_t header_end = 0;
    while (header_end < file_size && file_data[header_end] != '\n') header_end++;
    header_end++;

    // Build array of line start offsets
    std::vector<size_t> line_offsets;
    line_offsets.reserve(42000000);

    size_t scan_pos = header_end;
    while (scan_pos < file_size) {
        // Skip trailing empty lines
        if (file_data[scan_pos] == '\n' ||
            (file_data[scan_pos] == '\r' && scan_pos + 1 < file_size &&
             file_data[scan_pos + 1] == '\n')) {
            scan_pos++;
            continue;
        }
        line_offsets.push_back(scan_pos);
        while (scan_pos < file_size && file_data[scan_pos] != '\n') scan_pos++;
        scan_pos++;
    }

    const size_t total_lines = line_offsets.size();

    auto t_scan = std::chrono::high_resolution_clock::now();
    double scan_sec = std::chrono::duration<double>(t_scan - t_mmap).count();

    std::cout << " done (" << total_lines << " lines, "
              << scan_sec << "s)" << std::endl;

    size_t per_thread = total_lines / num_threads;

    std::cout << "  Step 3: Parallel parsing (" << num_threads
              << " threads, ~" << per_thread << " lines/thread)..."
              << std::endl;

    records.resize(total_lines);
    pool.reserve(total_lines * 70);

    std::vector<TextPool> thread_pools(num_threads);
    std::vector<std::pair<size_t, size_t>> thread_ranges(num_threads);
    std::vector<size_t> thread_valid(num_threads, 0);

    for (int t = 0; t < num_threads; ++t) {
        thread_pools[t].reserve(per_thread * 70);
    }

    auto tp0 = std::chrono::high_resolution_clock::now();

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        int nthreads = omp_get_num_threads();

        // Compute contiguous range for this thread
        size_t chunk = total_lines / nthreads;
        size_t remainder = total_lines % nthreads;
        size_t start = tid * chunk +
                       std::min(static_cast<size_t>(tid), remainder);
        size_t end = start + chunk +
                     (static_cast<size_t>(tid) < remainder ? 1 : 0);

        thread_ranges[tid] = {start, end};
        auto& local_pool = thread_pools[tid];
        size_t valid = 0;

        for (size_t i = start; i < end; ++i) {

            size_t line_start = line_offsets[i];
            size_t line_end;
            if (i + 1 < total_lines) {
                line_end = line_offsets[i + 1];
            } else {
                line_end = file_size;
            }


            while (line_end > line_start &&
                   (file_data[line_end - 1] == '\n' ||
                    file_data[line_end - 1] == '\r')) {
                line_end--;
            }

            int line_len = static_cast<int>(line_end - line_start);
            if (line_len <= 0) continue;

            if (populate_record(file_data + line_start, line_len,
                                records[i], local_pool)) {
                valid++;
            }
        }

        thread_valid[tid] = valid;
    }

    auto tp1 = std::chrono::high_resolution_clock::now();
    double parse_sec = std::chrono::duration<double>(tp1 - tp0).count();

    size_t total_parsed = 0;
    for (int t = 0; t < num_threads; ++t) {
        total_parsed += thread_valid[t];
    }


    line_offsets.clear();
    line_offsets.shrink_to_fit();

    // Merge TextPools
    auto tm0 = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < num_threads; ++t) {
        auto [start, end] = thread_ranges[t];

        uint32_t base_offset = 0;
        if (thread_pools[t].size() > 0) {
            base_offset = pool.append_bulk(
                thread_pools[t].data_ptr(), thread_pools[t].size());
        }

        if (base_offset > 0) {
            for (size_t i = start; i < end; ++i) {
                for (int f = 0; f < NUM_TEXT_FIELDS; ++f) {
                    if (records[i].str_lengths[f] > 0) {
                        records[i].str_offsets[f] += base_offset;
                    }
                }
            }
        }
    }

    thread_pools.clear();
    thread_pools.shrink_to_fit();

    if (total_parsed < total_lines) {
        size_t write_pos = 0;
        for (size_t i = 0; i < total_lines; ++i) {
            if (records[i].summons_number != 0) {
                if (write_pos != i) records[write_pos] = std::move(records[i]);
                write_pos++;
            }
        }
        records.resize(write_pos);
        std::cout << "  Note: compacted " << (total_lines - total_parsed)
                  << " invalid records" << std::endl;
    }

    auto tm1 = std::chrono::high_resolution_clock::now();
    double merge_sec = std::chrono::duration<double>(tm1 - tm0).count();

    munmap(const_cast<void*>(static_cast<const void*>(file_data)), file_size);

    double total_sec = std::chrono::duration<double>(tm1 - t_start).count();

    std::cout << "  Done: " << total_parsed << " records parsed, "
              << (total_lines - total_parsed) << " skipped" << std::endl;
    std::cout << "  Timing: mmap=" << mmap_sec << "s, scan="
              << scan_sec << "s, parse=" << parse_sec
              << "s, merge=" << merge_sec << "s, total="
              << total_sec << "s" << std::endl;
    std::cout << "  TextPool: " << pool.size_mb() << " MB ("
              << pool.size() << " bytes)" << std::endl;

    return total_parsed;
}

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

}
