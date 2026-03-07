#include <iostream>
#include <cstring>
#include <string>
#include <cstdio>

#include "core/violation_record.hpp"
#include "core/field_types.hpp"
#include "core/text_ref.hpp"
#include "data/data_store.hpp"
#include "data/text_pool.hpp"
#include "io/file_loader.hpp"
#include "search/linear_search.hpp"
#include "search/indexed_search.hpp"
#include "search/parallel_search.hpp"

using namespace parking;

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    std::cout << "  TEST: " << name << " ... " << std::flush; \
    tests_passed++;

#define PASS() std::cout << "PASSED" << std::endl;

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) { \
        std::cout << "FAILED (expected " << (b) << ", got " << (a) << ")" << std::endl; \
        tests_failed++; tests_passed--; \
    } else { PASS(); }

#define ASSERT_GT(a, b) \
    if (!((a) > (b))) { \
        std::cout << "FAILED (expected " << (a) << " > " << (b) << ")" << std::endl; \
        tests_failed++; tests_passed--; \
    } else { PASS(); }

#define ASSERT_TRUE(a) \
    if (!(a)) { \
        std::cout << "FAILED (expected true)" << std::endl; \
        tests_failed++; tests_passed--; \
    } else { PASS(); }

// ── Build a small in-memory test dataset ────────────────────────────────────

static data::DataStore build_test_store() {
    data::DataStore store;
    std::vector<core::ViolationRecord> recs;
    recs.reserve(100);

    // Access the store's TextPool to append plate strings
    data::TextPool& pool = store.text_pool();

    // Create 100 synthetic records with known values
    for (int i = 0; i < 100; ++i) {
        core::ViolationRecord r;
        r.summons_number = 9000000000ULL + i;
        r.issue_date = 20240101 + (i % 28);  // Jan 1-28 2024
        r.violation_code = (i % 10 == 0) ? 46 : 21;  // 10 records with code 46
        r.registration_state = (i < 70) ? 1 : 2;  // 70 NY, 30 NJ
        r.violation_county = (i % 5) + 1;  // rotate MN(1),BK(2),QN(3),BX(4),SI(5)
        r.violation_precinct = (i % 3) + 1;  // precincts 1, 2, 3
        r.fiscal_year = (i < 50) ? 2024 : 2025;

        // Plate: first 5 records get "TEST001", rest get unique plates
        if (i < 5) {
            const char* plate = "TEST001";
            int plen = 7;
            r.str_offsets[core::SF_PLATE_ID] = pool.append(plate, plen);
            r.str_lengths[core::SF_PLATE_ID] = static_cast<uint8_t>(plen);
        } else {
            char plate[16];
            int plen = snprintf(plate, sizeof(plate), "PLT%05d", i);
            r.str_offsets[core::SF_PLATE_ID] = pool.append(plate, plen);
            r.str_lengths[core::SF_PLATE_ID] = static_cast<uint8_t>(plen);
        }

        recs.push_back(r);
    }

    store.add_records(std::move(recs));
    return store;
}

// ── Test LinearSearch ───────────────────────────────────────────────────────

static void test_linear_search() {
    std::cout << "\n=== LinearSearch ===" << std::endl;

    data::DataStore store = build_test_store();
    const data::TextPool& pool = store.text_pool();
    search::LinearSearch engine;

    TEST("date range Jan 1-7")
    auto r1 = engine.search_by_date_range(store, 20240101, 20240107);
    // Days 1-7 → i % 28 produces 1-7 for i=0..6, and again for i=28..34, etc.
    // i%28: 0→1, 1→2, ..., 6→7, 28→1, 29→2, ..., 34→7, 56→1, ..., 62→7, 84→1, ..., 90→7
    // That's 4 full cycles × 7 = 28 records
    ASSERT_EQ(r1.count(), 28u)

    TEST("date range total_scanned")
    ASSERT_EQ(r1.total_scanned, 100u)

    TEST("date range elapsed > 0")
    ASSERT_GT(r1.elapsed_ms, 0.0)

    TEST("violation code 46")
    auto r2 = engine.search_by_violation_code(store, 46);
    ASSERT_EQ(r2.count(), 10u)  // every 10th record

    TEST("violation code 21")
    auto r3 = engine.search_by_violation_code(store, 21);
    ASSERT_EQ(r3.count(), 90u)

    TEST("violation code 99 (none)")
    auto r4 = engine.search_by_violation_code(store, 99);
    ASSERT_EQ(r4.count(), 0u)

    TEST("plate TEST001")
    auto r5 = engine.search_by_plate(store, pool, "TEST001", 7);
    ASSERT_EQ(r5.count(), 5u)

    TEST("plate nonexistent")
    auto r6 = engine.search_by_plate(store, pool, "ZZZZZZ", 6);
    ASSERT_EQ(r6.count(), 0u)

    TEST("state NY (enum 1)")
    auto r7 = engine.search_by_state(store, 1);
    ASSERT_EQ(r7.count(), 70u)

    TEST("state NJ (enum 2)")
    auto r8 = engine.search_by_state(store, 2);
    ASSERT_EQ(r8.count(), 30u)

    TEST("county BK (enum 2)")
    auto r9 = engine.search_by_county(store, core::county::BK);
    ASSERT_EQ(r9.count(), 20u)  // every 5th record with county=(i%5)+1=2

    TEST("count by precinct")
    auto agg1 = engine.count_by_precinct(store);
    ASSERT_EQ(agg1.counts.size(), 3u)  // precincts 1, 2, 3

    TEST("count by precinct - precinct 1 count")
    size_t p1_count = 0;
    for (auto& [p, c] : agg1.counts) {
        if (p == 1) p1_count = c;
    }
    // i%3==0 → precinct 1: i=0,3,6,...,99 → 34 records
    ASSERT_EQ(p1_count, 34u)

    TEST("count by fiscal year")
    auto agg2 = engine.count_by_fiscal_year(store);
    ASSERT_EQ(agg2.counts.size(), 2u)  // 2024 and 2025

    TEST("count by fiscal year - FY2024")
    size_t fy24 = 0;
    for (auto& [fy, c] : agg2.counts) {
        if (fy == 2024) fy24 = c;
    }
    ASSERT_EQ(fy24, 50u)
}

// ── Test IndexedSearch ──────────────────────────────────────────────────────

static void test_indexed_search() {
    std::cout << "\n=== IndexedSearch ===" << std::endl;

    data::DataStore store = build_test_store();
    const data::TextPool& pool = store.text_pool();
    search::IndexedSearch engine;

    TEST("build indices")
    engine.build_indices(store);
    ASSERT_GT(engine.build_time_ms(), 0.0)

    TEST("date range Jan 1-7")
    auto r1 = engine.search_by_date_range(store, 20240101, 20240107);
    ASSERT_EQ(r1.count(), 28u)

    TEST("violation code 46")
    auto r2 = engine.search_by_violation_code(store, 46);
    ASSERT_EQ(r2.count(), 10u)

    TEST("violation code 99 (none)")
    auto r3 = engine.search_by_violation_code(store, 99);
    ASSERT_EQ(r3.count(), 0u)

    TEST("plate TEST001 (linear fallback)")
    auto r4 = engine.search_by_plate(store, pool, "TEST001", 7);
    ASSERT_EQ(r4.count(), 5u)

    TEST("state NY (enum 1)")
    auto r5 = engine.search_by_state(store, 1);
    ASSERT_EQ(r5.count(), 70u)

    TEST("state NJ (enum 2)")
    auto r6 = engine.search_by_state(store, 2);
    ASSERT_EQ(r6.count(), 30u)

    TEST("county BK (enum 2)")
    auto r7 = engine.search_by_county(store, core::county::BK);
    ASSERT_EQ(r7.count(), 20u)

    TEST("count by precinct")
    auto agg1 = engine.count_by_precinct(store);
    ASSERT_EQ(agg1.counts.size(), 3u)

    TEST("count by fiscal year")
    auto agg2 = engine.count_by_fiscal_year(store);
    ASSERT_EQ(agg2.counts.size(), 2u)
}

// ── Cross-validate: Linear vs Indexed give same results ─────────────────────

static void test_cross_validation() {
    std::cout << "\n=== Cross-Validation (Linear vs Indexed) ===" << std::endl;

    data::DataStore store = build_test_store();
    const data::TextPool& pool = store.text_pool();
    search::LinearSearch linear;
    search::IndexedSearch indexed;
    indexed.build_indices(store);

    TEST("date range results match")
    auto lr = linear.search_by_date_range(store, 20240101, 20240114);
    auto ir = indexed.search_by_date_range(store, 20240101, 20240114);
    ASSERT_EQ(lr.count(), ir.count())

    TEST("violation code results match")
    auto lr2 = linear.search_by_violation_code(store, 46);
    auto ir2 = indexed.search_by_violation_code(store, 46);
    ASSERT_EQ(lr2.count(), ir2.count())

    TEST("state results match")
    auto lr3 = linear.search_by_state(store, 1);
    auto ir3 = indexed.search_by_state(store, 1);
    ASSERT_EQ(lr3.count(), ir3.count())

    TEST("county results match")
    auto lr4 = linear.search_by_county(store, core::county::QN);
    auto ir4 = indexed.search_by_county(store, core::county::QN);
    ASSERT_EQ(lr4.count(), ir4.count())

    TEST("plate results match")
    auto lr5 = linear.search_by_plate(store, pool, "TEST001", 7);
    auto ir5 = indexed.search_by_plate(store, pool, "TEST001", 7);
    ASSERT_EQ(lr5.count(), ir5.count())
}

// ── Test ParallelSearch ──────────────────────────────────────────────────────

static void test_parallel_search() {
    std::cout << "\n=== ParallelSearch ===" << std::endl;

    data::DataStore store = build_test_store();
    const data::TextPool& pool = store.text_pool();
    search::ParallelSearch engine;

    TEST("date range Jan 1-7")
    auto r1 = engine.search_by_date_range(store, 20240101, 20240107);
    ASSERT_EQ(r1.count(), 28u)

    TEST("date range total_scanned")
    ASSERT_EQ(r1.total_scanned, 100u)

    TEST("date range elapsed > 0")
    ASSERT_GT(r1.elapsed_ms, 0.0)

    TEST("violation code 46")
    auto r2 = engine.search_by_violation_code(store, 46);
    ASSERT_EQ(r2.count(), 10u)

    TEST("violation code 21")
    auto r3 = engine.search_by_violation_code(store, 21);
    ASSERT_EQ(r3.count(), 90u)

    TEST("violation code 99 (none)")
    auto r4 = engine.search_by_violation_code(store, 99);
    ASSERT_EQ(r4.count(), 0u)

    TEST("plate TEST001")
    auto r5 = engine.search_by_plate(store, pool, "TEST001", 7);
    ASSERT_EQ(r5.count(), 5u)

    TEST("plate nonexistent")
    auto r6 = engine.search_by_plate(store, pool, "ZZZZZZ", 6);
    ASSERT_EQ(r6.count(), 0u)

    TEST("state NY (enum 1)")
    auto r7 = engine.search_by_state(store, 1);
    ASSERT_EQ(r7.count(), 70u)

    TEST("state NJ (enum 2)")
    auto r8 = engine.search_by_state(store, 2);
    ASSERT_EQ(r8.count(), 30u)

    TEST("county BK (enum 2)")
    auto r9 = engine.search_by_county(store, core::county::BK);
    ASSERT_EQ(r9.count(), 20u)

    TEST("count by precinct")
    auto agg1 = engine.count_by_precinct(store);
    ASSERT_EQ(agg1.counts.size(), 3u)

    TEST("count by precinct - precinct 1 count")
    size_t p1_count = 0;
    for (auto& [p, c] : agg1.counts) {
        if (p == 1) p1_count = c;
    }
    ASSERT_EQ(p1_count, 34u)

    TEST("count by fiscal year")
    auto agg2 = engine.count_by_fiscal_year(store);
    ASSERT_EQ(agg2.counts.size(), 2u)

    TEST("count by fiscal year - FY2024")
    size_t fy24 = 0;
    for (auto& [fy, c] : agg2.counts) {
        if (fy == 2024) fy24 = c;
    }
    ASSERT_EQ(fy24, 50u)
}

// ── Cross-validate: Linear vs Parallel give same results ────────────────────

static void test_parallel_cross_validation() {
    std::cout << "\n=== Cross-Validation (Linear vs Parallel) ===" << std::endl;

    data::DataStore store = build_test_store();
    const data::TextPool& pool = store.text_pool();
    search::LinearSearch linear;
    search::ParallelSearch parallel;

    TEST("date range results match")
    auto lr = linear.search_by_date_range(store, 20240101, 20240114);
    auto pr = parallel.search_by_date_range(store, 20240101, 20240114);
    ASSERT_EQ(lr.count(), pr.count())

    TEST("violation code results match")
    auto lr2 = linear.search_by_violation_code(store, 46);
    auto pr2 = parallel.search_by_violation_code(store, 46);
    ASSERT_EQ(lr2.count(), pr2.count())

    TEST("state results match")
    auto lr3 = linear.search_by_state(store, 1);
    auto pr3 = parallel.search_by_state(store, 1);
    ASSERT_EQ(lr3.count(), pr3.count())

    TEST("county results match")
    auto lr4 = linear.search_by_county(store, core::county::QN);
    auto pr4 = parallel.search_by_county(store, core::county::QN);
    ASSERT_EQ(lr4.count(), pr4.count())

    TEST("plate results match")
    auto lr5 = linear.search_by_plate(store, pool, "TEST001", 7);
    auto pr5 = parallel.search_by_plate(store, pool, "TEST001", 7);
    ASSERT_EQ(lr5.count(), pr5.count())

    TEST("precinct aggregation matches")
    auto la1 = linear.count_by_precinct(store);
    auto pa1 = parallel.count_by_precinct(store);
    ASSERT_EQ(la1.counts.size(), pa1.counts.size())

    TEST("fiscal year aggregation matches")
    auto la2 = linear.count_by_fiscal_year(store);
    auto pa2 = parallel.count_by_fiscal_year(store);
    ASSERT_EQ(la2.counts.size(), pa2.counts.size())
}

// ── Main ────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "======================================" << std::endl;
    std::cout << "  Parking Violations Search Tests" << std::endl;
    std::cout << "======================================" << std::endl;

    test_linear_search();
    test_indexed_search();
    test_cross_validation();
    test_parallel_search();
    test_parallel_cross_validation();

    std::cout << "\n======================================" << std::endl;
    std::cout << "  Results: " << tests_passed << " passed, "
              << tests_failed << " failed" << std::endl;
    std::cout << "======================================" << std::endl;

    return tests_failed > 0 ? 1 : 0;
}
