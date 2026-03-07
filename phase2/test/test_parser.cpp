#include <iostream>
#include <cstring>
#include <cassert>
#include <cstdio>

#include "core/violation_record.hpp"
#include "core/field_types.hpp"
#include "core/text_ref.hpp"
#include "io/csv_parser.hpp"

using namespace parking::io;
using namespace parking::core;

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    std::cout << "  TEST: " << name << " ... "; \
    tests_passed++;

#define PASS() std::cout << "PASSED" << std::endl;

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) { \
        std::cout << "FAILED (expected " << (b) << ", got " << (a) << ")" << std::endl; \
        tests_failed++; tests_passed--; \
    } else { PASS(); }

#define ASSERT_STREQ(a, b) \
    if (std::strcmp((a), (b)) != 0) { \
        std::cout << "FAILED (expected \"" << (b) << "\", got \"" << (a) << "\")" << std::endl; \
        tests_failed++; tests_passed--; \
    } else { PASS(); }

#define ASSERT_TRUE(a) \
    if (!(a)) { \
        std::cout << "FAILED (expected true)" << std::endl; \
        tests_failed++; tests_passed--; \
    } else { PASS(); }

#define ASSERT_FALSE(a) \
    if ((a)) { \
        std::cout << "FAILED (expected false)" << std::endl; \
        tests_failed++; tests_passed--; \
    } else { PASS(); }

// ── Test ViolationRecord ────────────────────────────────────────────────────

void test_record_size() {
    std::cout << "\n=== ViolationRecord ===" << std::endl;

    TEST("sizeof(ViolationRecord)")
    size_t sz = sizeof(ViolationRecord);
    std::cout << "size=" << sz << " bytes ... ";
    // With TextPool approach: ~168 bytes (numeric+enum+str_offsets+str_lengths+vtable)
    if (sz >= 100 && sz <= 250) {
        PASS();
    } else {
        std::cout << "UNEXPECTED SIZE" << std::endl;
        tests_failed++; tests_passed--;
    }
}

void test_record_zero_init() {
    TEST("zero-initialization")
    ViolationRecord rec;
    ASSERT_EQ(rec.summons_number, 0UL)

    TEST("zero-init str_lengths[SF_PLATE_ID]")
    ASSERT_EQ(rec.str_lengths[SF_PLATE_ID], 0)

    TEST("zero-init str_offsets[SF_PLATE_ID]")
    ASSERT_EQ(rec.str_offsets[SF_PLATE_ID], 0u)

    TEST("zero-init violation_code")
    ASSERT_EQ(rec.violation_code, 0)

    TEST("zero-init fiscal_year")
    ASSERT_EQ(rec.fiscal_year, 0)

    TEST("zero-init violation_county")
    ASSERT_EQ(rec.violation_county, 0)
}

// ── Test CsvParser::parse_line ──────────────────────────────────────────────

void test_parse_simple_quoted() {
    std::cout << "\n=== CsvParser::parse_line ===" << std::endl;

    // Simple quoted fields
    const char* line = "\"ABC\",\"DEF\",\"GHI\"";
    int len = std::strlen(line);
    FieldView fields[MAX_FIELDS];

    TEST("simple quoted - field count")
    int n = CsvParser::parse_line(line, len, fields);
    ASSERT_EQ(n, 3)

    TEST("simple quoted - field 0")
    char buf[32];
    CsvParser::to_char_field(fields[0].data, fields[0].length, buf, sizeof(buf));
    ASSERT_STREQ(buf, "ABC")

    TEST("simple quoted - field 2")
    CsvParser::to_char_field(fields[2].data, fields[2].length, buf, sizeof(buf));
    ASSERT_STREQ(buf, "GHI")
}

void test_parse_empty_fields() {
    // Empty fields: "A",,,"D"
    const char* line = "\"A\",,,\"D\"";
    int len = std::strlen(line);
    FieldView fields[MAX_FIELDS];

    TEST("empty fields - field count")
    int n = CsvParser::parse_line(line, len, fields);
    ASSERT_EQ(n, 4)

    TEST("empty fields - field 1 empty")
    ASSERT_EQ(fields[1].length, 0)

    TEST("empty fields - field 2 empty")
    ASSERT_EQ(fields[2].length, 0)

    TEST("empty fields - field 3 is D")
    char buf[32];
    CsvParser::to_char_field(fields[3].data, fields[3].length, buf, sizeof(buf));
    ASSERT_STREQ(buf, "D")
}

void test_parse_real_fy2025_row() {
    // Actual row from FY2025 (simplified — first 10 fields)
    const char* line =
        "\"9139716661\",\"LKS7820\",\"NY\",\"PAS\",\"20240702\","
        "\"38\",\"SUBN\",\"TOYOT\",\"T\",\"26790\"";
    int len = std::strlen(line);
    FieldView fields[MAX_FIELDS];

    TEST("real FY2025 row - field count")
    int n = CsvParser::parse_line(line, len, fields);
    ASSERT_EQ(n, 10)

    TEST("real FY2025 row - summons number")
    uint64_t summons = CsvParser::to_uint64(fields[0].data, fields[0].length);
    ASSERT_EQ(summons, 9139716661ULL)

    TEST("real FY2025 row - plate ID")
    char plate[11];
    CsvParser::to_char_field(fields[1].data, fields[1].length, plate, sizeof(plate));
    ASSERT_STREQ(plate, "LKS7820")

    TEST("real FY2025 row - state")
    uint8_t state = state_to_enum(fields[2].data, fields[2].length);
    ASSERT_EQ(state, 1)  // NY = 1

    TEST("real FY2025 row - plate type")
    uint8_t pt = plate_type_to_enum(fields[3].data, fields[3].length);
    ASSERT_EQ(pt, 1)  // PAS = 1

    TEST("real FY2025 row - issue date")
    uint32_t date = CsvParser::to_uint32(fields[4].data, fields[4].length);
    ASSERT_EQ(date, 20240702U)

    TEST("real FY2025 row - violation code")
    uint16_t code = CsvParser::to_uint16(fields[5].data, fields[5].length);
    ASSERT_EQ(code, 38)

    TEST("real FY2025 row - vehicle body type")
    char body[5];
    CsvParser::to_char_field(fields[6].data, fields[6].length, body, sizeof(body));
    ASSERT_STREQ(body, "SUBN")

    TEST("real FY2025 row - vehicle make")
    char make[6];
    CsvParser::to_char_field(fields[7].data, fields[7].length, make, sizeof(make));
    ASSERT_STREQ(make, "TOYOT")

    TEST("real FY2025 row - issuing agency")
    uint8_t ag = agency_to_enum(fields[8].data, fields[8].length);
    ASSERT_EQ(ag, agency::T)
}

void test_parse_full_44_column_row() {
    // A full 44-column row from the merged CSV (all fields quoted)
    const char* line =
        "\"9139716661\",\"LKS7820\",\"NY\",\"PAS\",\"20240702\","
        "\"38\",\"SUBN\",\"TOYOT\",\"T\",\"26790\","
        "\"28590\",\"12810\",\"20260312\",\"0005\",\"005\","
        "\"005\",\"363518\",\"T105\",\"J\",\"0209P\","
        "\"\",\"MN\",\"F\",\"42A\",\"Mott St\","
        "\"\",\"\",\"408\",\"H1\",\"\","
        "\"YYYYYY\",\"0730A\",\"1000P\",\"WH\",\"\","
        "\"2024\",\"100038\",\"0\",\"071\",\"38-Failure to Dsplay Meter Rec\","
        "\"\",\"\",\"\",\"2025\"";
    int len = std::strlen(line);
    FieldView fields[MAX_FIELDS];

    TEST("full 44-col row - field count")
    int n = CsvParser::parse_line(line, len, fields);
    ASSERT_EQ(n, 44)

    TEST("full 44-col row - county enum")
    uint8_t cty = county_to_enum(fields[21].data, fields[21].length);
    ASSERT_EQ(cty, county::MN)

    TEST("full 44-col row - front/opposite enum")
    uint8_t fo = front_opposite_to_enum(fields[22].data, fields[22].length);
    ASSERT_EQ(fo, front_opposite::F)

    TEST("full 44-col row - street name")
    char street[21];
    CsvParser::to_char_field(fields[24].data, fields[24].length, street, sizeof(street));
    ASSERT_STREQ(street, "Mott St")

    TEST("full 44-col row - vehicle year")
    uint16_t year = CsvParser::to_uint16(fields[35].data, fields[35].length);
    ASSERT_EQ(year, 2024)

    TEST("full 44-col row - fiscal year")
    uint16_t fy = CsvParser::to_uint16(fields[43].data, fields[43].length);
    ASSERT_EQ(fy, 2025)

    TEST("full 44-col row - violation description")
    char desc[66];
    CsvParser::to_char_field(fields[39].data, fields[39].length, desc, sizeof(desc));
    ASSERT_STREQ(desc, "38-Failure to Dsplay Meter Rec")

    TEST("full 44-col row - issuer squad")
    uint8_t sq = squad_to_enum(fields[18].data, fields[18].length);
    ASSERT_EQ(sq, 10)  // J is the 10th code (1-based)
}

// ── Test type conversions ───────────────────────────────────────────────────

void test_type_conversions() {
    std::cout << "\n=== Type Conversions ===" << std::endl;

    TEST("to_uint64 normal")
    ASSERT_EQ(CsvParser::to_uint64("9139716661", 10), 9139716661ULL)

    TEST("to_uint64 empty")
    ASSERT_EQ(CsvParser::to_uint64("", 0), 0ULL)

    TEST("to_uint32 date")
    ASSERT_EQ(CsvParser::to_uint32("20240702", 8), 20240702U)

    TEST("to_uint32 empty")
    ASSERT_EQ(CsvParser::to_uint32("", 0), 0U)

    TEST("to_uint16 code")
    ASSERT_EQ(CsvParser::to_uint16("38", 2), 38)

    TEST("to_uint16 zero")
    ASSERT_EQ(CsvParser::to_uint16("0", 1), 0)

    TEST("to_char_field normal")
    char buf[6];
    CsvParser::to_char_field("TOYOT", 5, buf, sizeof(buf));
    ASSERT_STREQ(buf, "TOYOT")

    TEST("to_char_field truncation")
    char small[4];
    CsvParser::to_char_field("ABCDEF", 6, small, sizeof(small));
    ASSERT_STREQ(small, "ABC")

    TEST("to_char_field empty")
    char empty[4];
    CsvParser::to_char_field("", 0, empty, sizeof(empty));
    ASSERT_STREQ(empty, "")
}

// ── Test null detection ─────────────────────────────────────────────────────

void test_null_detection() {
    std::cout << "\n=== Null Detection ===" << std::endl;

    TEST("is_null_date empty")
    ASSERT_TRUE(CsvParser::is_null_date("", 0))

    TEST("is_null_date '0'")
    ASSERT_TRUE(CsvParser::is_null_date("0", 1))

    TEST("is_null_date '00000000'")
    ASSERT_TRUE(CsvParser::is_null_date("00000000", 8))

    TEST("is_null_date '88888888'")
    ASSERT_TRUE(CsvParser::is_null_date("88888888", 8))

    TEST("is_null_date valid date")
    ASSERT_FALSE(CsvParser::is_null_date("20240702", 8))

    TEST("is_null_date '123'")
    ASSERT_FALSE(CsvParser::is_null_date("123", 3))
}

// ── Test enum lookups ───────────────────────────────────────────────────────

void test_enum_lookups() {
    std::cout << "\n=== Enum Lookups ===" << std::endl;

    TEST("county MN")
    ASSERT_EQ(county_to_enum("MN", 2), county::MN)

    TEST("county BK")
    ASSERT_EQ(county_to_enum("BK", 2), county::BK)

    TEST("county QN")
    ASSERT_EQ(county_to_enum("QN", 2), county::QN)

    TEST("county BX")
    ASSERT_EQ(county_to_enum("BX", 2), county::BX)

    TEST("county SI")
    ASSERT_EQ(county_to_enum("SI", 2), county::SI)

    TEST("county empty")
    ASSERT_EQ(county_to_enum("", 0), county::UNKNOWN)

    TEST("agency T")
    ASSERT_EQ(agency_to_enum("T", 1), agency::T)

    TEST("agency V")
    ASSERT_EQ(agency_to_enum("V", 1), agency::V)

    TEST("agency empty")
    ASSERT_EQ(agency_to_enum("", 0), agency::UNKNOWN)

    TEST("state NY")
    ASSERT_EQ(state_to_enum("NY", 2), 1)

    TEST("state NJ")
    ASSERT_EQ(state_to_enum("NJ", 2), 2)

    TEST("state PA")
    ASSERT_EQ(state_to_enum("PA", 2), 3)

    TEST("state unknown XX")
    ASSERT_EQ(state_to_enum("XX", 2), 0)

    TEST("plate_type PAS")
    ASSERT_EQ(plate_type_to_enum("PAS", 3), 1)

    TEST("plate_type COM")
    ASSERT_EQ(plate_type_to_enum("COM", 3), 2)

    TEST("plate_type empty")
    ASSERT_EQ(plate_type_to_enum("", 0), 0)

    TEST("front_opposite F")
    ASSERT_EQ(front_opposite_to_enum("F", 1), front_opposite::F)

    TEST("front_opposite O")
    ASSERT_EQ(front_opposite_to_enum("O", 1), front_opposite::O)

    TEST("legal_code T")
    ASSERT_EQ(legal_code_to_enum("T", 1), legal_code::T)

    TEST("legal_code empty")
    ASSERT_EQ(legal_code_to_enum("", 0), legal_code::UNKNOWN)

    TEST("squad J")
    ASSERT_EQ(squad_to_enum("J", 1), 10)

    TEST("squad AA")
    ASSERT_EQ(squad_to_enum("AA", 2), 27)

    TEST("squad empty")
    ASSERT_EQ(squad_to_enum("", 0), 0)
}

// ── Main ────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "======================================" << std::endl;
    std::cout << "  Parking Violations Parser Tests" << std::endl;
    std::cout << "======================================" << std::endl;

    test_record_size();
    test_record_zero_init();
    test_parse_simple_quoted();
    test_parse_empty_fields();
    test_parse_real_fy2025_row();
    test_parse_full_44_column_row();
    test_type_conversions();
    test_null_detection();
    test_enum_lookups();

    std::cout << "\n======================================" << std::endl;
    std::cout << "  Results: " << tests_passed << " passed, "
              << tests_failed << " failed" << std::endl;
    std::cout << "======================================" << std::endl;

    return tests_failed > 0 ? 1 : 0;
}
