#include "soa_store.hpp"

namespace parking {

void SoADataStore::reserve(size_t n) {
    issue_dates.reserve(n);
    violation_codes.reserve(n);
    violation_precincts.reserve(n);
    fiscal_years.reserve(n);
    registration_states.reserve(n);
    violation_counties.reserve(n);

    plate_offsets.reserve(n);
    plate_lengths.reserve(n);

    summons_numbers.reserve(n);
    street_code1.reserve(n);
    street_code2.reserve(n);
    street_code3.reserve(n);
    vehicle_expiration_dates.reserve(n);
    issuer_codes.reserve(n);
    date_first_observed.reserve(n);
    issuer_precincts.reserve(n);
    law_sections.reserve(n);
    vehicle_years.reserve(n);
    feet_from_curb.reserve(n);

    plate_types.reserve(n);
    issuing_agencies.reserve(n);
    issuer_squads.reserve(n);
    violation_front_opposites.reserve(n);
    violation_legal_codes.reserve(n);
    unregistered_vehicles.reserve(n);

    for (int i = 0; i < NUM_STR_FIELDS; ++i) {
        str_offsets[i].reserve(n);
        str_lengths[i].reserve(n);
    }

    text_pool.reserve(n * 70);
}

void SoADataStore::resize(size_t n) {
    issue_dates.resize(n);
    violation_codes.resize(n);
    violation_precincts.resize(n);
    fiscal_years.resize(n);
    registration_states.resize(n);
    violation_counties.resize(n);

    plate_offsets.resize(n);
    plate_lengths.resize(n);

    summons_numbers.resize(n);
    street_code1.resize(n);
    street_code2.resize(n);
    street_code3.resize(n);
    vehicle_expiration_dates.resize(n);
    issuer_codes.resize(n);
    date_first_observed.resize(n);
    issuer_precincts.resize(n);
    law_sections.resize(n);
    vehicle_years.resize(n);
    feet_from_curb.resize(n);

    plate_types.resize(n);
    issuing_agencies.resize(n);
    issuer_squads.resize(n);
    violation_front_opposites.resize(n);
    violation_legal_codes.resize(n);
    unregistered_vehicles.resize(n);

    for (int i = 0; i < NUM_STR_FIELDS; ++i) {
        str_offsets[i].resize(n);
        str_lengths[i].resize(n);
    }

    record_count = n;
}

}
