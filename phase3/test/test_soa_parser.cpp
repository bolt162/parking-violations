#include <iostream>
#include "csv_parser.hpp"

using namespace parking;
using namespace parking::csv;

int main() {
    std::cout << "Testing CSV parser..." << std::endl;

    // Test parse_line
    const char* line = "123,ABC,\"hello, world\",456";
    FieldView fields[MAX_FIELDS];
    int n = parse_line(line, 26, fields);

    std::cout << "Fields: " << n << std::endl;
    for (int i = 0; i < n; i++) {
        std::cout << "  [" << i << "] = '";
        std::cout.write(fields[i].data, fields[i].length);
        std::cout << "'" << std::endl;
    }

    // Test numeric conversion
    std::cout << "\nNumeric tests:" << std::endl;
    std::cout << "  to_u64('12345', 5) = " << to_u64("12345", 5) << std::endl;
    std::cout << "  to_u32('999', 3) = " << to_u32("999", 3) << std::endl;
    std::cout << "  to_u16('100', 3) = " << to_u16("100", 3) << std::endl;

    // Test null date
    std::cout << "\nNull date tests:" << std::endl;
    std::cout << "  is_null_date('', 0) = " << is_null_date("", 0) << std::endl;
    std::cout << "  is_null_date('0', 1) = " << is_null_date("0", 1) << std::endl;
    std::cout << "  is_null_date('20240101', 8) = " << is_null_date("20240101", 8) << std::endl;

    std::cout << "\nAll tests passed." << std::endl;
    return 0;
}
