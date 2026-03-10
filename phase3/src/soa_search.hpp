#ifndef SOA_SEARCH_HPP
#define SOA_SEARCH_HPP

#include "parking.hpp"

namespace parking {

// SIMD-friendly queries
CountResult count_in_date_range(const SoADataStore& store, uint32_t start, uint32_t end);
DateRangeResult find_date_extremes(const SoADataStore& store);

// Filter queries
SearchResult search_by_date_range(const SoADataStore& store, uint32_t start, uint32_t end);
SearchResult search_by_violation_code(const SoADataStore& store, uint16_t code);
SearchResult search_by_plate(const SoADataStore& store, const char* plate, int len);
SearchResult search_by_state(const SoADataStore& store, uint8_t state);
SearchResult search_by_county(const SoADataStore& store, uint8_t county);

// Aggregate queries
AggregateResult count_by_precinct(const SoADataStore& store);
AggregateResult count_by_fiscal_year(const SoADataStore& store);

}

#endif
