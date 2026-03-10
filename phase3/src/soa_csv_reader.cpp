#include "soa_csv_reader.hpp"
#include "soa_store.hpp"
#include "csv_parser.hpp"
#include "record.hpp"

#include <iostream>
#include <chrono>
#include <vector>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <omp.h>

namespace parking {

using namespace csv;

// String column indices for the 20 additional string fields
static const Column STR_COLS[20] = {
    Column::VEHICLE_BODY_TYPE, Column::VEHICLE_MAKE, Column::VIOLATION_LOCATION,
    Column::ISSUER_COMMAND, Column::VIOLATION_TIME, Column::TIME_FIRST_OBSERVED,
    Column::HOUSE_NUMBER, Column::STREET_NAME, Column::INTERSECTING_STREET,
    Column::SUB_DIVISION, Column::DAYS_PARKING_IN_EFFECT, Column::FROM_HOURS_IN_EFFECT,
    Column::TO_HOURS_IN_EFFECT, Column::VEHICLE_COLOR, Column::METER_NUMBER,
    Column::VIOLATION_POST_CODE, Column::VIOLATION_DESCRIPTION, Column::NO_STANDING_STOPPING,
    Column::HYDRANT_VIOLATION, Column::DOUBLE_PARKING
};

// Store one record into SoA arrays at index i, using a thread-local TextPool
static bool store_record(const FieldView* f, int n, size_t i, SoADataStore& s, TextPool& local_pool) {
    if (n < 43) return false;

    // Numeric fields
    const FieldView& date = f[static_cast<int>(Column::ISSUE_DATE)];
    s.issue_dates[i] = is_null_date(date.data, date.length) ? 0 : to_u32(date.data, date.length);

    s.violation_codes[i] = to_u16(f[static_cast<int>(Column::VIOLATION_CODE)].data,
                                   f[static_cast<int>(Column::VIOLATION_CODE)].length);

    s.violation_precincts[i] = to_u16(f[static_cast<int>(Column::VIOLATION_PRECINCT)].data,
                                       f[static_cast<int>(Column::VIOLATION_PRECINCT)].length);

    s.fiscal_years[i] = (n >= 44) ? to_u16(f[static_cast<int>(Column::FISCAL_YEAR)].data,
                                            f[static_cast<int>(Column::FISCAL_YEAR)].length) : 0;

    s.summons_numbers[i] = to_u64(f[static_cast<int>(Column::SUMMONS_NUMBER)].data,
                                   f[static_cast<int>(Column::SUMMONS_NUMBER)].length);

    s.street_code1[i] = to_u32(f[static_cast<int>(Column::STREET_CODE1)].data,
                                f[static_cast<int>(Column::STREET_CODE1)].length);
    s.street_code2[i] = to_u32(f[static_cast<int>(Column::STREET_CODE2)].data,
                                f[static_cast<int>(Column::STREET_CODE2)].length);
    s.street_code3[i] = to_u32(f[static_cast<int>(Column::STREET_CODE3)].data,
                                f[static_cast<int>(Column::STREET_CODE3)].length);

    const FieldView& vexp = f[static_cast<int>(Column::VEHICLE_EXPIRATION_DATE)];
    s.vehicle_expiration_dates[i] = is_null_date(vexp.data, vexp.length) ? 0 : to_u32(vexp.data, vexp.length);

    s.issuer_codes[i] = to_u32(f[static_cast<int>(Column::ISSUER_CODE)].data,
                                f[static_cast<int>(Column::ISSUER_CODE)].length);

    const FieldView& dfo = f[static_cast<int>(Column::DATE_FIRST_OBSERVED)];
    s.date_first_observed[i] = is_null_date(dfo.data, dfo.length) ? 0 : to_u32(dfo.data, dfo.length);

    s.issuer_precincts[i] = to_u16(f[static_cast<int>(Column::ISSUER_PRECINCT)].data,
                                    f[static_cast<int>(Column::ISSUER_PRECINCT)].length);
    s.law_sections[i] = to_u16(f[static_cast<int>(Column::LAW_SECTION)].data,
                                f[static_cast<int>(Column::LAW_SECTION)].length);
    s.vehicle_years[i] = to_u16(f[static_cast<int>(Column::VEHICLE_YEAR)].data,
                                 f[static_cast<int>(Column::VEHICLE_YEAR)].length);
    s.feet_from_curb[i] = to_u16(f[static_cast<int>(Column::FEET_FROM_CURB)].data,
                                  f[static_cast<int>(Column::FEET_FROM_CURB)].length);

    // Enum fields
    s.registration_states[i] = state_to_enum(f[static_cast<int>(Column::REGISTRATION_STATE)].data,
                                              f[static_cast<int>(Column::REGISTRATION_STATE)].length);
    s.violation_counties[i] = county_to_enum(f[static_cast<int>(Column::VIOLATION_COUNTY)].data,
                                              f[static_cast<int>(Column::VIOLATION_COUNTY)].length);
    s.plate_types[i] = plate_type_to_enum(f[static_cast<int>(Column::PLATE_TYPE)].data,
                                           f[static_cast<int>(Column::PLATE_TYPE)].length);
    s.issuing_agencies[i] = agency_to_enum(f[static_cast<int>(Column::ISSUING_AGENCY)].data,
                                            f[static_cast<int>(Column::ISSUING_AGENCY)].length);
    s.issuer_squads[i] = squad_to_enum(f[static_cast<int>(Column::ISSUER_SQUAD)].data,
                                        f[static_cast<int>(Column::ISSUER_SQUAD)].length);
    s.violation_front_opposites[i] = front_opposite_to_enum(f[static_cast<int>(Column::VIOLATION_FRONT_OPPOSITE)].data,
                                                             f[static_cast<int>(Column::VIOLATION_FRONT_OPPOSITE)].length);
    s.violation_legal_codes[i] = legal_code_to_enum(f[static_cast<int>(Column::VIOLATION_LEGAL_CODE)].data,
                                                     f[static_cast<int>(Column::VIOLATION_LEGAL_CODE)].length);
    s.unregistered_vehicles[i] = unreg_vehicle_to_enum(f[static_cast<int>(Column::UNREGISTERED_VEHICLE)].data,
                                                        f[static_cast<int>(Column::UNREGISTERED_VEHICLE)].length);

    // Plate string
    const FieldView& plate = f[static_cast<int>(Column::PLATE_ID)];
    s.plate_offsets[i] = local_pool.append(plate.data, plate.length);
    s.plate_lengths[i] = plate.length;

    // Other 20 string fields
    for (int j = 0; j < 20; j++) {
        const FieldView& sf = f[static_cast<int>(STR_COLS[j])];
        s.str_offsets[j][i] = local_pool.append(sf.data, sf.length);
        s.str_lengths[j][i] = sf.length;
    }

    return true;
}

size_t SoACsvReader::read(const std::string& filepath, SoADataStore& store) {
    std::cout << "Loading: " << filepath << std::endl;
    auto t_start = std::chrono::high_resolution_clock::now();

    // Open and mmap file
    int fd = open(filepath.c_str(), O_RDONLY);
    if (fd < 0) {
        std::cerr << "Cannot open file" << std::endl;
        return 0;
    }

    struct stat st;
    fstat(fd, &st);
    size_t file_size = st.st_size;

    const char* data = static_cast<const char*>(
        mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0));
    close(fd);

    if (data == MAP_FAILED) {
        std::cerr << "mmap failed" << std::endl;
        return 0;
    }

    auto t_mmap = std::chrono::high_resolution_clock::now();

    // Skip header and find line boundaries
    size_t header_end = 0;
    while (header_end < file_size && data[header_end] != '\n') header_end++;
    header_end++;

    std::vector<size_t> line_starts;
    line_starts.reserve(42000000);

    size_t pos = header_end;
    while (pos < file_size) {
        if (data[pos] == '\n' || data[pos] == '\r') {
            pos++;
            continue;
        }
        line_starts.push_back(pos);
        while (pos < file_size && data[pos] != '\n') pos++;
        pos++;
    }

    size_t total_lines = line_starts.size();
    std::cout << "Found " << total_lines << " lines" << std::endl;

    auto t_scan = std::chrono::high_resolution_clock::now();
    store.resize(total_lines);

    // Parallel parsing with thread-local TextPools
    int num_threads = omp_get_max_threads();
    std::vector<TextPool> thread_pools(num_threads);
    std::vector<size_t> thread_start_idx(num_threads);
    std::vector<size_t> thread_end_idx(num_threads);

    // Pre-computing chunk boundaries
    size_t chunk_size = total_lines / num_threads;
    for (int t = 0; t < num_threads; t++) {
        thread_start_idx[t] = t * chunk_size;
        thread_end_idx[t] = (t == num_threads - 1) ? total_lines : (t + 1) * chunk_size;
        thread_pools[t].reserve((thread_end_idx[t] - thread_start_idx[t]) * 80);
    }

    std::vector<size_t> thread_parsed(num_threads, 0);

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        size_t start_i = thread_start_idx[tid];
        size_t end_i = thread_end_idx[tid];
        TextPool& local_pool = thread_pools[tid];
        FieldView fields[MAX_FIELDS];
        size_t local_parsed = 0;

        for (size_t i = start_i; i < end_i; i++) {
            size_t line_start = line_starts[i];
            size_t line_end = (i + 1 < total_lines) ? line_starts[i + 1] : file_size;

            // Trim newlines
            while (line_end > line_start && (data[line_end - 1] == '\n' || data[line_end - 1] == '\r')) {
                line_end--;
            }

            int n = parse_line(data + line_start, line_end - line_start, fields);
            if (store_record(fields, n, i, store, local_pool)) {
                local_parsed++;
            }
        }

        thread_parsed[tid] = local_parsed;
    }

    auto t_parse = std::chrono::high_resolution_clock::now();

    // Merge thread-local TextPools and adjust offsets
    std::vector<uint32_t> pool_base_offsets(num_threads);

    // Calculate total text size and base offsets
    size_t total_text_size = 0;
    for (int t = 0; t < num_threads; t++) {
        pool_base_offsets[t] = static_cast<uint32_t>(total_text_size);
        total_text_size += thread_pools[t].size();
    }

    // Reserve space in main pool and merge
    store.text_pool.reserve(total_text_size);
    for (int t = 0; t < num_threads; t++) {
        store.text_pool.append_bulk(thread_pools[t].data_ptr(), thread_pools[t].size());
    }

    // Adjust string offsets in parallel
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        size_t start_i = thread_start_idx[tid];
        size_t end_i = thread_end_idx[tid];
        uint32_t base_offset = pool_base_offsets[tid];

        for (size_t i = start_i; i < end_i; i++) {
            store.plate_offsets[i] += base_offset;
            for (int j = 0; j < 20; j++) {
                store.str_offsets[j][i] += base_offset;
            }
        }
    }

    auto t_merge = std::chrono::high_resolution_clock::now();

    // Count total parsed
    size_t total_parsed = 0;
    for (int t = 0; t < num_threads; t++) {
        total_parsed += thread_parsed[t];
    }
    store.record_count = total_parsed;

    // Release memory mapping
    munmap(const_cast<void*>(static_cast<const void*>(data)), file_size);

    auto t_end = std::chrono::high_resolution_clock::now();

    // Print timing breakdown
    double mmap_ms = std::chrono::duration<double, std::milli>(t_mmap - t_start).count();
    double scan_ms = std::chrono::duration<double, std::milli>(t_scan - t_mmap).count();
    double parse_ms = std::chrono::duration<double, std::milli>(t_parse - t_scan).count();
    double merge_ms = std::chrono::duration<double, std::milli>(t_merge - t_parse).count();
    double total_s = std::chrono::duration<double>(t_end - t_start).count();

    std::cout << "Timing breakdown:" << std::endl;
    std::cout << "  mmap:  " << mmap_ms << " ms" << std::endl;
    std::cout << "  scan:  " << scan_ms << " ms" << std::endl;
    std::cout << "  parse: " << parse_ms << " ms (" << num_threads << " threads)" << std::endl;
    std::cout << "  merge: " << merge_ms << " ms" << std::endl;
    std::cout << "Loaded " << total_parsed << " records in " << total_s << "s" << std::endl;
    std::cout << "TextPool: " << store.text_pool.size_gb() << " GB" << std::endl;

    return total_parsed;
}

}
