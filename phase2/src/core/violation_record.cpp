#include "core/violation_record.hpp"
#include <cstring>

namespace parking::core {

ViolationRecord::ViolationRecord()
    : summons_number{0},
      issue_date{0},
      street_code1{0}, street_code2{0}, street_code3{0},
      vehicle_expiration_date{0},
      issuer_code{0},
      date_first_observed{0},
      violation_code{0},
      violation_precinct{0},
      issuer_precinct{0},
      law_section{0},
      vehicle_year{0},
      feet_from_curb{0},
      fiscal_year{0},
      registration_state{0},
      plate_type{0},
      issuing_agency{0},
      issuer_squad{0},
      violation_county{0},
      violation_front_opposite{0},
      violation_legal_code{0},
      unregistered_vehicle{0},
      str_offsets{},
      str_lengths{}
{
}

} // namespace parking::core
