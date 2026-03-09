#include "soa_search.hpp"
#include <chrono>
#include <cstring>
#include <array>
#include <omp.h>

namespace parking {

SearchResult search_by_date_range(const SoADataStore& store, uint32_t start, uint32_t end) {
    SearchResult result;
    auto t0 = std::chrono::high_resolution_clock::now();

    size_t n = store.size();
    result.total_scanned = n;

    int num_threads = omp_get_max_threads();
    std::vector<std::vector<size_t>> thread_results(num_threads);

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        std::vector<size_t>& local = thread_results[tid];

        #pragma omp for
        for (size_t i = 0; i < n; i++) {
            uint32_t date = store.issue_dates[i];
            if (date >= start && date <= end) {
                local.push_back(i);
            }
        }
    }

    // Merge results
    for (int t = 0; t < num_threads; t++) {
        result.indices.insert(result.indices.end(),
                              thread_results[t].begin(),
                              thread_results[t].end());
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return result;
}

SearchResult search_by_violation_code(const SoADataStore& store, uint16_t code) {
    SearchResult result;
    auto t0 = std::chrono::high_resolution_clock::now();

    size_t n = store.size();
    result.total_scanned = n;

    int num_threads = omp_get_max_threads();
    std::vector<std::vector<size_t>> thread_results(num_threads);

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        std::vector<size_t>& local = thread_results[tid];

        #pragma omp for
        for (size_t i = 0; i < n; i++) {
            if (store.violation_codes[i] == code) {
                local.push_back(i);
            }
        }
    }

    for (int t = 0; t < num_threads; t++) {
        result.indices.insert(result.indices.end(),
                              thread_results[t].begin(),
                              thread_results[t].end());
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return result;
}

SearchResult search_by_plate(const SoADataStore& store, const char* plate, int len) {
    SearchResult result;
    auto t0 = std::chrono::high_resolution_clock::now();

    size_t n = store.size();
    result.total_scanned = n;

    int num_threads = omp_get_max_threads();
    std::vector<std::vector<size_t>> thread_results(num_threads);

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        std::vector<size_t>& local = thread_results[tid];

        #pragma omp for
        for (size_t i = 0; i < n; i++) {
            if (store.plate_lengths[i] == len) {
                const char* p = store.text_pool.get(store.plate_offsets[i]);
                if (std::memcmp(p, plate, len) == 0) {
                    local.push_back(i);
                }
            }
        }
    }

    for (int t = 0; t < num_threads; t++) {
        result.indices.insert(result.indices.end(),
                              thread_results[t].begin(),
                              thread_results[t].end());
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return result;
}

SearchResult search_by_state(const SoADataStore& store, uint8_t state) {
    SearchResult result;
    auto t0 = std::chrono::high_resolution_clock::now();

    size_t n = store.size();
    result.total_scanned = n;

    int num_threads = omp_get_max_threads();
    std::vector<std::vector<size_t>> thread_results(num_threads);

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        std::vector<size_t>& local = thread_results[tid];

        #pragma omp for
        for (size_t i = 0; i < n; i++) {
            if (store.registration_states[i] == state) {
                local.push_back(i);
            }
        }
    }

    for (int t = 0; t < num_threads; t++) {
        result.indices.insert(result.indices.end(),
                              thread_results[t].begin(),
                              thread_results[t].end());
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return result;
}

SearchResult search_by_county(const SoADataStore& store, uint8_t county) {
    SearchResult result;
    auto t0 = std::chrono::high_resolution_clock::now();

    size_t n = store.size();
    result.total_scanned = n;

    int num_threads = omp_get_max_threads();
    std::vector<std::vector<size_t>> thread_results(num_threads);

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        std::vector<size_t>& local = thread_results[tid];

        #pragma omp for
        for (size_t i = 0; i < n; i++) {
            if (store.violation_counties[i] == county) {
                local.push_back(i);
            }
        }
    }

    for (int t = 0; t < num_threads; t++) {
        result.indices.insert(result.indices.end(),
                              thread_results[t].begin(),
                              thread_results[t].end());
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return result;
}

AggregateResult count_by_precinct(const SoADataStore& store) {
    AggregateResult result;
    auto t0 = std::chrono::high_resolution_clock::now();

    size_t n = store.size();
    result.total_scanned = n;

    constexpr size_t MAX_PRECINCT = 200;
    std::array<size_t, MAX_PRECINCT> counts{};

    #pragma omp parallel
    {
        std::array<size_t, MAX_PRECINCT> local_counts{};

        #pragma omp for
        for (size_t i = 0; i < n; i++) {
            uint16_t p = store.violation_precincts[i];
            if (p < MAX_PRECINCT) {
                local_counts[p]++;
            }
        }

        #pragma omp critical
        {
            for (size_t j = 0; j < MAX_PRECINCT; j++) {
                counts[j] += local_counts[j];
            }
        }
    }

    for (uint16_t p = 0; p < MAX_PRECINCT; p++) {
        if (counts[p] > 0) {
            result.counts.emplace_back(p, counts[p]);
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return result;
}

AggregateResult count_by_fiscal_year(const SoADataStore& store) {
    AggregateResult result;
    auto t0 = std::chrono::high_resolution_clock::now();

    size_t n = store.size();
    result.total_scanned = n;

    constexpr size_t MAX_FY = 50;
    constexpr uint16_t BASE_YEAR = 2000;
    std::array<size_t, MAX_FY> counts{};

    #pragma omp parallel
    {
        std::array<size_t, MAX_FY> local_counts{};

        #pragma omp for
        for (size_t i = 0; i < n; i++) {
            uint16_t fy = store.fiscal_years[i];
            if (fy >= BASE_YEAR && fy < BASE_YEAR + MAX_FY) {
                local_counts[fy - BASE_YEAR]++;
            }
        }

        #pragma omp critical
        {
            for (size_t j = 0; j < MAX_FY; j++) {
                counts[j] += local_counts[j];
            }
        }
    }

    for (uint16_t i = 0; i < MAX_FY; i++) {
        if (counts[i] > 0) {
            result.counts.emplace_back(BASE_YEAR + i, counts[i]);
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return result;
}

} // namespace parking
