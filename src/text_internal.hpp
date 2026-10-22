#ifndef PLAINROOT_VALIDATION_TEXT_INTERNAL_HPP
#define PLAINROOT_VALIDATION_TEXT_INTERNAL_HPP

#include <plainroot/validation/validation.h>

#include "report_internal.hpp"
#include "text_utils.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace plainroot_validation_detail {

struct TextSettings {
  uint32_t maximum_input_bytes =
      PLAINROOT_VALIDATION_DEFAULT_MAXIMUM_INPUT_BYTES;
};

inline bool
read_text_settings(const plainroot_validation_text_options_v1 *options,
                   TextSettings& settings) noexcept {
  if (!options)
    return true;
  if (options->struct_size < sizeof(uint32_t))
    return false;
  if (options->struct_size >=
          offsetof(plainroot_validation_text_options_v1, maximum_input_bytes) +
              sizeof(options->maximum_input_bytes) &&
      options->maximum_input_bytes) {
    settings.maximum_input_bytes = options->maximum_input_bytes;
  }
  return settings.maximum_input_bytes <=
         PLAINROOT_VALIDATION_MAXIMUM_INPUT_BYTES;
}

inline plainroot_validation_result_v1
validate_text_field(const char *field, size_t field_size) noexcept {
  if (field_size && !field)
    return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
  if (!valid_utf8(field, field_size, true))
    return PLAINROOT_VALIDATION_INVALID_UTF8;
  return PLAINROOT_VALIDATION_OK;
}

inline plainroot_validation_result_v1
prepare_text_check(plainroot_validation_report *report, const char *field,
                   size_t field_size, const char *text, size_t text_size,
                   const plainroot_validation_text_options_v1 *options,
                   std::string_view& prepared,
                   size_t& invalid_offset) noexcept {
  const auto report_status = require_open_report(report);
  if (report_status != PLAINROOT_VALIDATION_OK)
    return report_status;
  const auto field_status = validate_text_field(field, field_size);
  if (field_status != PLAINROOT_VALIDATION_OK)
    return field_status;
  TextSettings settings;
  if (!read_text_settings(options, settings))
    return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
  if (text_size && !text)
    return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
  if (text_size > settings.maximum_input_bytes)
    return PLAINROOT_VALIDATION_LIMIT_EXCEEDED;
  prepared = std::string_view(text ? text : "", text_size);
  if (!valid_utf8(text, text_size, false, &invalid_offset))
    return PLAINROOT_VALIDATION_INVALID_UTF8;
  return PLAINROOT_VALIDATION_OK;
}

inline plainroot_validation_parameter_v1
text_u64_parameter(const char *name, size_t name_size,
                   uint64_t value) noexcept {
  plainroot_validation_parameter_v1 parameter{};
  parameter.struct_size = sizeof(parameter);
  parameter.name = name;
  parameter.name_size = name_size;
  parameter.type = PLAINROOT_VALIDATION_PARAMETER_U64;
  parameter.as.u64 = value;
  return parameter;
}

inline plainroot_validation_result_v1
append_text_issue(plainroot_validation_report *report,
                  plainroot_validation_issue_code_v1 code, const char *field,
                  size_t field_size,
                  const plainroot_validation_parameter_v1 *parameters,
                  size_t parameter_count) {
  plainroot_validation_issue_v1 issue{};
  issue.struct_size = sizeof(issue);
  issue.code = code;
  issue.field = field;
  issue.field_size = field_size;
  issue.parameters = parameters;
  issue.parameter_count = parameter_count;
  return plainroot_validation_report_add_issue_v1(report, &issue);
}

inline plainroot_validation_result_v1
append_invalid_utf8(plainroot_validation_report *report, const char *field,
                    size_t field_size, size_t invalid_offset) {
  const auto parameter = text_u64_parameter(
      "offset", sizeof("offset") - 1, static_cast<uint64_t>(invalid_offset));
  return append_text_issue(report, PLAINROOT_VALIDATION_ISSUE_INVALID_UTF8,
                           field, field_size, &parameter, 1);
}

}  // namespace plainroot_validation_detail

#endif
