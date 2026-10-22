#include <plainroot/validation/validation.h>

#include "text_internal.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace {

using plainroot_validation_detail::append_invalid_utf8;
using plainroot_validation_detail::append_text_issue;
using plainroot_validation_detail::prepare_text_check;
using plainroot_validation_detail::require_open_report;
using plainroot_validation_detail::text_u64_parameter;

plainroot_validation_result_v1
prepare(plainroot_validation_report *report, const char *field,
        size_t field_size, const char *text, size_t text_size,
        const plainroot_validation_text_options_v1 *options,
        std::string_view& prepared) {
  size_t invalid_offset = 0;
  const auto status =
      prepare_text_check(report, field, field_size, text, text_size, options,
                         prepared, invalid_offset);
  if (status == PLAINROOT_VALIDATION_INVALID_UTF8)
    return append_invalid_utf8(report, field, field_size, invalid_offset);
  return status;
}

bool ascii_atext(unsigned char value) noexcept {
  return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
         (value >= '0' && value <= '9') || value == '!' || value == '#' ||
         value == '$' || value == '%' || value == '&' || value == '\'' ||
         value == '*' || value == '+' || value == '-' || value == '/' ||
         value == '=' || value == '?' || value == '^' || value == '_' ||
         value == '`' || value == '{' || value == '|' || value == '}' ||
         value == '~';
}

bool valid_local_part(std::string_view local) noexcept {
  if (local.empty() || local.size() > 64 || local.front() == '.' ||
      local.back() == '.')
    return false;
  bool previous_dot = false;
  for (const unsigned char value : local) {
    if (value == '.') {
      if (previous_dot)
        return false;
      previous_dot = true;
    } else {
      if (!ascii_atext(value))
        return false;
      previous_dot = false;
    }
  }
  return true;
}

bool ascii_alphanumeric(unsigned char value) noexcept {
  return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
         (value >= '0' && value <= '9');
}

bool valid_domain(std::string_view domain) noexcept {
  if (domain.empty() || domain.size() > 253 || domain.front() == '.' ||
      domain.back() == '.')
    return false;

  size_t label_start = 0;
  size_t label_count = 0;
  for (size_t i = 0; i <= domain.size(); ++i) {
    if (i != domain.size() && domain[i] != '.')
      continue;
    const size_t label_size = i - label_start;
    if (!label_size || label_size > 63 ||
        !ascii_alphanumeric(static_cast<unsigned char>(domain[label_start])) ||
        !ascii_alphanumeric(static_cast<unsigned char>(domain[i - 1])))
      return false;
    for (size_t j = label_start; j < i; ++j) {
      const auto value = static_cast<unsigned char>(domain[j]);
      if (!ascii_alphanumeric(value) && value != '-')
        return false;
    }
    ++label_count;
    label_start = i + 1;
  }
  return label_count >= 2;
}

bool valid_email(std::string_view email) noexcept {
  if (email.empty() || email.size() > 254)
    return false;
  const size_t at = email.find('@');
  if (at == std::string_view::npos ||
      email.find('@', at + 1) != std::string_view::npos)
    return false;
  return valid_local_part(email.substr(0, at)) &&
         valid_domain(email.substr(at + 1));
}

plainroot_validation_result_v1
append_single_issue(plainroot_validation_report *report,
                    plainroot_validation_issue_code_v1 code, const char *field,
                    size_t field_size) {
  return append_text_issue(report, code, field, field_size, nullptr, 0);
}

}  // namespace

extern "C" {

plainroot_validation_result_v1 plainroot_validation_validate_required_text_v1(
    plainroot_validation_report *report, const char *field, size_t field_size,
    const char *text, size_t text_size,
    const plainroot_validation_text_options_v1 *options) {
  std::string_view prepared;
  const auto status =
      prepare(report, field, field_size, text, text_size, options, prepared);
  if (status != PLAINROOT_VALIDATION_OK)
    return status;
  if (!prepared.empty())
    return PLAINROOT_VALIDATION_OK;
  return append_single_issue(report, PLAINROOT_VALIDATION_ISSUE_REQUIRED, field,
                             field_size);
}

plainroot_validation_result_v1 plainroot_validation_validate_nonblank_text_v1(
    plainroot_validation_report *report, const char *field, size_t field_size,
    const char *text, size_t text_size,
    const plainroot_validation_text_options_v1 *options) {
  std::string_view prepared;
  const auto status =
      prepare(report, field, field_size, text, text_size, options, prepared);
  if (status != PLAINROOT_VALIDATION_OK)
    return status;
  if (plainroot_validation_detail::contains_non_whitespace_scalar(
          prepared.data(), prepared.size()))
    return PLAINROOT_VALIDATION_OK;
  return append_single_issue(report, PLAINROOT_VALIDATION_ISSUE_BLANK, field,
                             field_size);
}

plainroot_validation_result_v1 plainroot_validation_validate_text_length_v1(
    plainroot_validation_report *report, const char *field, size_t field_size,
    const char *text, size_t text_size, uint32_t minimum, uint32_t maximum,
    const plainroot_validation_text_options_v1 *options) {
  const auto report_status = require_open_report(report);
  if (report_status != PLAINROOT_VALIDATION_OK)
    return report_status;
  if (minimum > maximum)
    return PLAINROOT_VALIDATION_INVALID_RULE;
  std::string_view prepared;
  const auto status =
      prepare(report, field, field_size, text, text_size, options, prepared);
  if (status != PLAINROOT_VALIDATION_OK)
    return status;

  uint64_t scalar_count = 0;
  if (!plainroot_validation_detail::count_utf8_scalars(
          prepared.data(), prepared.size(), scalar_count))
    return PLAINROOT_VALIDATION_INTERNAL_ERROR;
  if (scalar_count < minimum) {
    const auto parameter =
        text_u64_parameter("minimum", sizeof("minimum") - 1, minimum);
    return append_text_issue(report, PLAINROOT_VALIDATION_ISSUE_TOO_SHORT,
                             field, field_size, &parameter, 1);
  }
  if (scalar_count > maximum) {
    const auto parameter =
        text_u64_parameter("maximum", sizeof("maximum") - 1, maximum);
    return append_text_issue(report, PLAINROOT_VALIDATION_ISSUE_TOO_LONG, field,
                             field_size, &parameter, 1);
  }
  return PLAINROOT_VALIDATION_OK;
}

plainroot_validation_result_v1 plainroot_validation_validate_text_max_bytes_v1(
    plainroot_validation_report *report, const char *field, size_t field_size,
    const char *text, size_t text_size, uint32_t maximum_bytes,
    const plainroot_validation_text_options_v1 *options) {
  std::string_view prepared;
  const auto status =
      prepare(report, field, field_size, text, text_size, options, prepared);
  if (status != PLAINROOT_VALIDATION_OK)
    return status;
  if (prepared.size() <= maximum_bytes)
    return PLAINROOT_VALIDATION_OK;
  const auto parameter = text_u64_parameter(
      "maximum_bytes", sizeof("maximum_bytes") - 1, maximum_bytes);
  return append_text_issue(report, PLAINROOT_VALIDATION_ISSUE_TOO_LONG, field,
                           field_size, &parameter, 1);
}

plainroot_validation_result_v1 plainroot_validation_validate_email_text_v1(
    plainroot_validation_report *report, const char *field, size_t field_size,
    const char *text, size_t text_size,
    const plainroot_validation_text_options_v1 *options) {
  std::string_view prepared;
  const auto status =
      prepare(report, field, field_size, text, text_size, options, prepared);
  if (status != PLAINROOT_VALIDATION_OK)
    return status;
  if (valid_email(prepared))
    return PLAINROOT_VALIDATION_OK;
  return append_single_issue(report, PLAINROOT_VALIDATION_ISSUE_INVALID_EMAIL,
                             field, field_size);
}

}  // extern "C"
