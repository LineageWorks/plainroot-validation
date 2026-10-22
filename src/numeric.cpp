#include <plainroot/validation/validation.h>

#include "report_internal.hpp"
#include "text_utils.hpp"

#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <system_error>

namespace {

struct ParseSettings {
  uint32_t maximum_input_bytes =
      PLAINROOT_VALIDATION_DEFAULT_MAXIMUM_INPUT_BYTES;
  bool trim_ascii_whitespace = false;
  bool boolean_case_insensitive_ascii = false;
  bool boolean_accept_yes_no = false;
  bool boolean_accept_0_1 = false;
};

template <typename T>
bool has_field(const T *options, size_t offset, size_t field_size) noexcept {
  return options && options->struct_size >= offset + field_size;
}

bool read_parse_settings(const plainroot_validation_parse_options_v1 *options,
                         ParseSettings& output) noexcept {
  if (!options)
    return true;
  if (options->struct_size < sizeof(uint32_t))
    return false;

  if (has_field(options,
                offsetof(plainroot_validation_parse_options_v1,
                         maximum_input_bytes),
                sizeof(options->maximum_input_bytes)) &&
      options->maximum_input_bytes) {
    output.maximum_input_bytes = options->maximum_input_bytes;
  }
  if (has_field(options,
                offsetof(plainroot_validation_parse_options_v1,
                         trim_ascii_whitespace),
                sizeof(options->trim_ascii_whitespace))) {
    if (options->trim_ascii_whitespace > 1)
      return false;
    output.trim_ascii_whitespace = options->trim_ascii_whitespace != 0;
  }
  if (has_field(options,
                offsetof(plainroot_validation_parse_options_v1,
                         boolean_case_insensitive_ascii),
                sizeof(options->boolean_case_insensitive_ascii))) {
    if (options->boolean_case_insensitive_ascii > 1)
      return false;
    output.boolean_case_insensitive_ascii =
        options->boolean_case_insensitive_ascii != 0;
  }
  if (has_field(options,
                offsetof(plainroot_validation_parse_options_v1,
                         boolean_accept_yes_no),
                sizeof(options->boolean_accept_yes_no))) {
    if (options->boolean_accept_yes_no > 1)
      return false;
    output.boolean_accept_yes_no = options->boolean_accept_yes_no != 0;
  }
  if (has_field(options,
                offsetof(plainroot_validation_parse_options_v1,
                         boolean_accept_0_1),
                sizeof(options->boolean_accept_0_1))) {
    if (options->boolean_accept_0_1 > 1)
      return false;
    output.boolean_accept_0_1 = options->boolean_accept_0_1 != 0;
  }
  return output.maximum_input_bytes <= PLAINROOT_VALIDATION_MAXIMUM_INPUT_BYTES;
}

plainroot_validation_result_v1 prepare_text(
    const char *text, size_t text_size,
    const plainroot_validation_parse_options_v1 *options,
    ParseSettings& settings, std::string_view& prepared,
    plainroot_validation_issue_code_v1& failure_code) noexcept {
  failure_code = PLAINROOT_VALIDATION_ISSUE_NONE;
  if (!read_parse_settings(options, settings))
    return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
  if (text_size && !text)
    return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
  if (text_size > settings.maximum_input_bytes)
    return PLAINROOT_VALIDATION_LIMIT_EXCEEDED;

  prepared = std::string_view(text ? text : "", text_size);
  if (!plainroot_validation_detail::valid_utf8(text, text_size, false)) {
    failure_code = PLAINROOT_VALIDATION_ISSUE_INVALID_UTF8;
    return PLAINROOT_VALIDATION_OK;
  }

  if (settings.trim_ascii_whitespace) {
    while (!prepared.empty() && plainroot_validation_detail::ascii_whitespace(
                                    static_cast<unsigned char>(prepared.front())))
      prepared.remove_prefix(1);
    while (!prepared.empty() && plainroot_validation_detail::ascii_whitespace(
                                    static_cast<unsigned char>(prepared.back())))
      prepared.remove_suffix(1);
  }
  return PLAINROOT_VALIDATION_OK;
}

bool signed_integer_syntax(std::string_view text) noexcept {
  if (text.empty())
    return false;
  size_t index = (text.front() == '+' || text.front() == '-') ? 1 : 0;
  if (index == text.size())
    return false;
  for (; index < text.size(); ++index) {
    if (text[index] < '0' || text[index] > '9')
      return false;
  }
  return true;
}

bool unsigned_integer_syntax(std::string_view text) noexcept {
  if (text.empty())
    return false;
  const size_t index = text.front() == '+' ? 1 : 0;
  if (index == text.size())
    return false;
  for (size_t i = index; i < text.size(); ++i) {
    if (text[i] < '0' || text[i] > '9')
      return false;
  }
  return true;
}

bool real_syntax(std::string_view text) noexcept {
  if (text.empty())
    return false;
  size_t index = (text.front() == '+' || text.front() == '-') ? 1 : 0;
  size_t integer_digits = 0;
  while (index < text.size() && text[index] >= '0' && text[index] <= '9') {
    ++integer_digits;
    ++index;
  }

  size_t fractional_digits = 0;
  if (index < text.size() && text[index] == '.') {
    ++index;
    while (index < text.size() && text[index] >= '0' && text[index] <= '9') {
      ++fractional_digits;
      ++index;
    }
  }
  if (integer_digits == 0 && fractional_digits == 0)
    return false;

  if (index < text.size() && (text[index] == 'e' || text[index] == 'E')) {
    ++index;
    if (index < text.size() && (text[index] == '+' || text[index] == '-'))
      ++index;
    const size_t exponent_start = index;
    while (index < text.size() && text[index] >= '0' && text[index] <= '9')
      ++index;
    if (index == exponent_start)
      return false;
  }
  return index == text.size();
}

std::string_view remove_plus(std::string_view text) noexcept {
  if (!text.empty() && text.front() == '+')
    text.remove_prefix(1);
  return text;
}

plainroot_validation_result_v1 append_issue(
    plainroot_validation_report *report,
    plainroot_validation_issue_code_v1 code, const char *field,
    size_t field_size, const plainroot_validation_parameter_v1 *parameters,
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

plainroot_validation_result_v1 validate_field(const char *field,
                                               size_t field_size) noexcept {
  if (field_size && !field)
    return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
  if (!plainroot_validation_detail::valid_utf8(field, field_size, true))
    return PLAINROOT_VALIDATION_INVALID_UTF8;
  return PLAINROOT_VALIDATION_OK;
}

plainroot_validation_result_v1 prepare_check(
    plainroot_validation_report *report, const char *field,
    size_t field_size) noexcept {
  const auto report_result =
      plainroot_validation_detail::require_open_report(report);
  if (report_result != PLAINROOT_VALIDATION_OK)
    return report_result;
  return validate_field(field, field_size);
}

plainroot_validation_parameter_v1 i64_parameter(const char *name,
                                                 int64_t value) noexcept {
  plainroot_validation_parameter_v1 parameter{};
  parameter.struct_size = sizeof(parameter);
  parameter.name = name;
  parameter.name_size = std::char_traits<char>::length(name);
  parameter.type = PLAINROOT_VALIDATION_PARAMETER_I64;
  parameter.as.i64 = value;
  return parameter;
}

plainroot_validation_parameter_v1 u64_parameter(const char *name,
                                                 uint64_t value) noexcept {
  plainroot_validation_parameter_v1 parameter{};
  parameter.struct_size = sizeof(parameter);
  parameter.name = name;
  parameter.name_size = std::char_traits<char>::length(name);
  parameter.type = PLAINROOT_VALIDATION_PARAMETER_U64;
  parameter.as.u64 = value;
  return parameter;
}

plainroot_validation_parameter_v1 f64_parameter(const char *name,
                                                 double value) noexcept {
  plainroot_validation_parameter_v1 parameter{};
  parameter.struct_size = sizeof(parameter);
  parameter.name = name;
  parameter.name_size = std::char_traits<char>::length(name);
  parameter.type = PLAINROOT_VALIDATION_PARAMETER_F64;
  parameter.as.f64 = value;
  return parameter;
}

bool ascii_equal(std::string_view input, std::string_view expected,
                 bool case_insensitive) noexcept {
  if (input.size() != expected.size())
    return false;
  for (size_t i = 0; i < input.size(); ++i) {
    unsigned char actual = static_cast<unsigned char>(input[i]);
    unsigned char wanted = static_cast<unsigned char>(expected[i]);
    if (case_insensitive) {
      if (actual >= 'A' && actual <= 'Z')
        actual = static_cast<unsigned char>(actual - 'A' + 'a');
      if (wanted >= 'A' && wanted <= 'Z')
        wanted = static_cast<unsigned char>(wanted - 'A' + 'a');
    }
    if (actual != wanted)
      return false;
  }
  return true;
}

}  // namespace

extern "C" {

plainroot_validation_result_v1 plainroot_validation_validate_range_i64_v1(
    plainroot_validation_report *report, const char *field, size_t field_size,
    int64_t value, int64_t minimum, int64_t maximum) {
  const auto ready = prepare_check(report, field, field_size);
  if (ready != PLAINROOT_VALIDATION_OK)
    return ready;
  if (minimum > maximum)
    return PLAINROOT_VALIDATION_INVALID_RULE;
  if (value >= minimum && value <= maximum)
    return PLAINROOT_VALIDATION_OK;
  const plainroot_validation_parameter_v1 parameters[] = {
      i64_parameter("minimum", minimum), i64_parameter("maximum", maximum)};
  return append_issue(report, PLAINROOT_VALIDATION_ISSUE_OUT_OF_RANGE, field,
                      field_size, parameters, 2);
}

plainroot_validation_result_v1 plainroot_validation_validate_range_u64_v1(
    plainroot_validation_report *report, const char *field, size_t field_size,
    uint64_t value, uint64_t minimum, uint64_t maximum) {
  const auto ready = prepare_check(report, field, field_size);
  if (ready != PLAINROOT_VALIDATION_OK)
    return ready;
  if (minimum > maximum)
    return PLAINROOT_VALIDATION_INVALID_RULE;
  if (value >= minimum && value <= maximum)
    return PLAINROOT_VALIDATION_OK;
  const plainroot_validation_parameter_v1 parameters[] = {
      u64_parameter("minimum", minimum), u64_parameter("maximum", maximum)};
  return append_issue(report, PLAINROOT_VALIDATION_ISSUE_OUT_OF_RANGE, field,
                      field_size, parameters, 2);
}

plainroot_validation_result_v1 plainroot_validation_validate_range_f64_v1(
    plainroot_validation_report *report, const char *field, size_t field_size,
    double value, double minimum, double maximum) {
  const auto ready = prepare_check(report, field, field_size);
  if (ready != PLAINROOT_VALIDATION_OK)
    return ready;
  if (!std::isfinite(minimum) || !std::isfinite(maximum) || minimum > maximum)
    return PLAINROOT_VALIDATION_INVALID_RULE;
  if (!std::isfinite(value)) {
    return append_issue(report,
                        PLAINROOT_VALIDATION_ISSUE_NON_FINITE_NUMBER, field,
                        field_size, nullptr, 0);
  }
  if (value >= minimum && value <= maximum)
    return PLAINROOT_VALIDATION_OK;
  const plainroot_validation_parameter_v1 parameters[] = {
      f64_parameter("minimum", minimum), f64_parameter("maximum", maximum)};
  return append_issue(report, PLAINROOT_VALIDATION_ISSUE_OUT_OF_RANGE, field,
                      field_size, parameters, 2);
}

plainroot_validation_result_v1 plainroot_validation_validate_finite_f64_v1(
    plainroot_validation_report *report, const char *field, size_t field_size,
    double value) {
  const auto ready = prepare_check(report, field, field_size);
  if (ready != PLAINROOT_VALIDATION_OK)
    return ready;
  if (std::isfinite(value))
    return PLAINROOT_VALIDATION_OK;
  return append_issue(report, PLAINROOT_VALIDATION_ISSUE_NON_FINITE_NUMBER,
                      field, field_size, nullptr, 0);
}

plainroot_validation_result_v1 plainroot_validation_parse_i64_text_v1(
    const char *text, size_t text_size,
    const plainroot_validation_parse_options_v1 *options,
    plainroot_validation_parse_i64_result_v1 *output) {
  if (!output || output->struct_size < sizeof(*output))
    return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
  output->parsed = 0;
  output->value = 0;
  output->failure_code = PLAINROOT_VALIDATION_ISSUE_NONE;

  ParseSettings settings;
  std::string_view prepared;
  auto status = prepare_text(text, text_size, options, settings, prepared,
                             output->failure_code);
  if (status != PLAINROOT_VALIDATION_OK ||
      output->failure_code != PLAINROOT_VALIDATION_ISSUE_NONE)
    return status;
  if (!signed_integer_syntax(prepared)) {
    output->failure_code = PLAINROOT_VALIDATION_ISSUE_INVALID_INTEGER;
    return PLAINROOT_VALIDATION_OK;
  }

  const std::string_view parse_text = remove_plus(prepared);
  int64_t value = 0;
  const auto converted = std::from_chars(
      parse_text.data(), parse_text.data() + parse_text.size(), value, 10);
  if (converted.ec == std::errc::result_out_of_range) {
    output->failure_code = PLAINROOT_VALIDATION_ISSUE_OUT_OF_RANGE;
    return PLAINROOT_VALIDATION_OK;
  }
  if (converted.ec != std::errc{} ||
      converted.ptr != parse_text.data() + parse_text.size()) {
    output->failure_code = PLAINROOT_VALIDATION_ISSUE_INVALID_INTEGER;
    return PLAINROOT_VALIDATION_OK;
  }
  output->value = value;
  output->parsed = 1;
  return PLAINROOT_VALIDATION_OK;
}

plainroot_validation_result_v1 plainroot_validation_parse_u64_text_v1(
    const char *text, size_t text_size,
    const plainroot_validation_parse_options_v1 *options,
    plainroot_validation_parse_u64_result_v1 *output) {
  if (!output || output->struct_size < sizeof(*output))
    return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
  output->parsed = 0;
  output->value = 0;
  output->failure_code = PLAINROOT_VALIDATION_ISSUE_NONE;

  ParseSettings settings;
  std::string_view prepared;
  auto status = prepare_text(text, text_size, options, settings, prepared,
                             output->failure_code);
  if (status != PLAINROOT_VALIDATION_OK ||
      output->failure_code != PLAINROOT_VALIDATION_ISSUE_NONE)
    return status;
  if (!unsigned_integer_syntax(prepared)) {
    output->failure_code =
        PLAINROOT_VALIDATION_ISSUE_INVALID_UNSIGNED_INTEGER;
    return PLAINROOT_VALIDATION_OK;
  }

  const std::string_view parse_text = remove_plus(prepared);
  uint64_t value = 0;
  const auto converted = std::from_chars(
      parse_text.data(), parse_text.data() + parse_text.size(), value, 10);
  if (converted.ec == std::errc::result_out_of_range) {
    output->failure_code = PLAINROOT_VALIDATION_ISSUE_OUT_OF_RANGE;
    return PLAINROOT_VALIDATION_OK;
  }
  if (converted.ec != std::errc{} ||
      converted.ptr != parse_text.data() + parse_text.size()) {
    output->failure_code =
        PLAINROOT_VALIDATION_ISSUE_INVALID_UNSIGNED_INTEGER;
    return PLAINROOT_VALIDATION_OK;
  }
  output->value = value;
  output->parsed = 1;
  return PLAINROOT_VALIDATION_OK;
}

plainroot_validation_result_v1 plainroot_validation_parse_f64_text_v1(
    const char *text, size_t text_size,
    const plainroot_validation_parse_options_v1 *options,
    plainroot_validation_parse_f64_result_v1 *output) {
  if (!output || output->struct_size < sizeof(*output))
    return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
  output->parsed = 0;
  output->value = 0.0;
  output->failure_code = PLAINROOT_VALIDATION_ISSUE_NONE;

  ParseSettings settings;
  std::string_view prepared;
  auto status = prepare_text(text, text_size, options, settings, prepared,
                             output->failure_code);
  if (status != PLAINROOT_VALIDATION_OK ||
      output->failure_code != PLAINROOT_VALIDATION_ISSUE_NONE)
    return status;
  if (!real_syntax(prepared)) {
    output->failure_code = PLAINROOT_VALIDATION_ISSUE_INVALID_REAL;
    return PLAINROOT_VALIDATION_OK;
  }

  const std::string_view parse_text = remove_plus(prepared);
  double value = 0.0;
  const auto converted = std::from_chars(
      parse_text.data(), parse_text.data() + parse_text.size(), value,
      std::chars_format::general);
  if (converted.ec == std::errc::result_out_of_range) {
    output->failure_code = PLAINROOT_VALIDATION_ISSUE_OUT_OF_RANGE;
    return PLAINROOT_VALIDATION_OK;
  }
  if (converted.ec != std::errc{} ||
      converted.ptr != parse_text.data() + parse_text.size()) {
    output->failure_code = PLAINROOT_VALIDATION_ISSUE_INVALID_REAL;
    return PLAINROOT_VALIDATION_OK;
  }
  if (!std::isfinite(value)) {
    output->failure_code = PLAINROOT_VALIDATION_ISSUE_OUT_OF_RANGE;
    return PLAINROOT_VALIDATION_OK;
  }
  output->value = value;
  output->parsed = 1;
  return PLAINROOT_VALIDATION_OK;
}

plainroot_validation_result_v1 plainroot_validation_parse_bool_text_v1(
    const char *text, size_t text_size,
    const plainroot_validation_parse_options_v1 *options,
    plainroot_validation_parse_bool_result_v1 *output) {
  if (!output || output->struct_size < sizeof(*output))
    return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
  output->parsed = 0;
  output->value = 0;
  output->failure_code = PLAINROOT_VALIDATION_ISSUE_NONE;

  ParseSettings settings;
  std::string_view prepared;
  auto status = prepare_text(text, text_size, options, settings, prepared,
                             output->failure_code);
  if (status != PLAINROOT_VALIDATION_OK ||
      output->failure_code != PLAINROOT_VALIDATION_ISSUE_NONE)
    return status;

  const bool insensitive = settings.boolean_case_insensitive_ascii;
  if (ascii_equal(prepared, "true", insensitive)) {
    output->parsed = 1;
    output->value = 1;
  } else if (ascii_equal(prepared, "false", insensitive)) {
    output->parsed = 1;
    output->value = 0;
  } else if (settings.boolean_accept_yes_no &&
             ascii_equal(prepared, "yes", insensitive)) {
    output->parsed = 1;
    output->value = 1;
  } else if (settings.boolean_accept_yes_no &&
             ascii_equal(prepared, "no", insensitive)) {
    output->parsed = 1;
    output->value = 0;
  } else if (settings.boolean_accept_0_1 && prepared == "1") {
    output->parsed = 1;
    output->value = 1;
  } else if (settings.boolean_accept_0_1 && prepared == "0") {
    output->parsed = 1;
    output->value = 0;
  } else {
    output->failure_code = PLAINROOT_VALIDATION_ISSUE_INVALID_BOOLEAN;
  }
  return PLAINROOT_VALIDATION_OK;
}

}  // extern "C"
