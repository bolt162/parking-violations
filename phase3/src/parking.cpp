#include "parking.hpp"
#include "soa_csv_reader.hpp"
#include "soa_search.hpp"
#include <cstring>

namespace parking {

size_t ParkingAPI::load(const std::string& filepath) {
    SoACsvReader reader;
    return reader.read(filepath, store_);
}

SearchResult ParkingAPI::find_by_date_range(uint32_t start_date, uint32_t end_date) {
    return search_by_date_range(store_, start_date, end_date);
}

SearchResult ParkingAPI::find_by_violation_code(uint16_t code) {
    return search_by_violation_code(store_, code);
}

SearchResult ParkingAPI::find_by_plate(const char* plate_id) {
    return search_by_plate(store_, plate_id, std::strlen(plate_id));
}

SearchResult ParkingAPI::find_by_state(uint8_t state_enum) {
    return search_by_state(store_, state_enum);
}

SearchResult ParkingAPI::find_by_county(uint8_t county_enum) {
    return search_by_county(store_, county_enum);
}

AggregateResult ParkingAPI::count_by_precinct() {
    return parking::count_by_precinct(store_);
}

AggregateResult ParkingAPI::count_by_fiscal_year() {
    return parking::count_by_fiscal_year(store_);
}

} // namespace parking
