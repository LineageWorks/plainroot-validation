#ifndef PLAINROOT_VALIDATION_REPORT_INTERNAL_HPP
#define PLAINROOT_VALIDATION_REPORT_INTERNAL_HPP

#include <plainroot/validation/validation.h>

namespace plainroot_validation_detail {

plainroot_validation_result_v1 require_open_report(
    const plainroot_validation_report *report) noexcept;

}  // namespace plainroot_validation_detail

#endif
