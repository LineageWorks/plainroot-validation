#include <plainroot/validation/validation.h>

#include "text_utils.hpp"
#include "report_internal.hpp"

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace {

struct ParameterData {
  std::string name;
  plainroot_validation_parameter_type_v1 type =
      PLAINROOT_VALIDATION_PARAMETER_BOOL;
  union Value {
    uint8_t boolean;
    int64_t i64;
    uint64_t u64;
    double f64;
    Value() : u64(0) {}
  } value;
  std::string text;
};

struct IssueData {
  plainroot_validation_issue_code_v1 code =
      PLAINROOT_VALIDATION_ISSUE_REQUIRED;
  std::string field;
  std::vector<ParameterData> parameters;
};

struct ReportLimits {
  uint32_t maximum_issues = PLAINROOT_VALIDATION_DEFAULT_MAXIMUM_ISSUES;
  uint32_t maximum_parameters_per_issue =
      PLAINROOT_VALIDATION_DEFAULT_MAXIMUM_PARAMETERS_PER_ISSUE;
  uint64_t maximum_text_bytes =
      PLAINROOT_VALIDATION_DEFAULT_MAXIMUM_TEXT_BYTES;
};

bool checked_add(uint64_t left, uint64_t right, uint64_t& result) noexcept {
  if (right > std::numeric_limits<uint64_t>::max() - left)
    return false;
  result = left + right;
  return true;
}

bool known_issue_code(plainroot_validation_issue_code_v1 code) noexcept {
  return code >= PLAINROOT_VALIDATION_ISSUE_REQUIRED &&
         code <= PLAINROOT_VALIDATION_ISSUE_BLANK;
}

bool known_parameter_type(
    plainroot_validation_parameter_type_v1 type) noexcept {
  return type >= PLAINROOT_VALIDATION_PARAMETER_BOOL &&
         type <= PLAINROOT_VALIDATION_PARAMETER_TEXT;
}

bool valid_report_options(const plainroot_validation_report_options_v1 *options,
                          ReportLimits& limits) noexcept {
  if (!options)
    return true;
  if (options->struct_size < sizeof(uint32_t))
    return false;
  if (options->struct_size >=
      offsetof(plainroot_validation_report_options_v1, maximum_issues) +
          sizeof(options->maximum_issues)) {
    if (options->maximum_issues)
      limits.maximum_issues = options->maximum_issues;
  }
  if (options->struct_size >=
      offsetof(plainroot_validation_report_options_v1,
               maximum_parameters_per_issue) +
          sizeof(options->maximum_parameters_per_issue)) {
    if (options->maximum_parameters_per_issue)
      limits.maximum_parameters_per_issue =
          options->maximum_parameters_per_issue;
  }
  if (options->struct_size >=
      offsetof(plainroot_validation_report_options_v1, maximum_text_bytes) +
          sizeof(options->maximum_text_bytes)) {
    if (options->maximum_text_bytes)
      limits.maximum_text_bytes = options->maximum_text_bytes;
  }
  return limits.maximum_issues <= PLAINROOT_VALIDATION_MAXIMUM_ISSUES &&
         limits.maximum_parameters_per_issue <=
             PLAINROOT_VALIDATION_MAXIMUM_PARAMETERS_PER_ISSUE &&
         limits.maximum_text_bytes <= PLAINROOT_VALIDATION_MAXIMUM_TEXT_BYTES;
}

plainroot_validation_result_v1 add_parameter(
    const plainroot_validation_parameter_v1& input, ParameterData& output,
    uint64_t& text_bytes) {
  if (input.struct_size < sizeof(input) || !input.name || !input.name_size)
    return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
  if (!known_parameter_type(input.type))
    return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
  if (!plainroot_validation_detail::valid_utf8(input.name, input.name_size,
                                                true))
    return PLAINROOT_VALIDATION_INVALID_UTF8;

  uint64_t next_bytes = 0;
  if (!checked_add(text_bytes, input.name_size, next_bytes))
    return PLAINROOT_VALIDATION_LIMIT_EXCEEDED;
  output.name.assign(input.name, input.name_size);
  output.type = input.type;
  text_bytes = next_bytes;

  switch (input.type) {
    case PLAINROOT_VALIDATION_PARAMETER_BOOL:
      if (input.as.boolean > 1)
        return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
      output.value.boolean = input.as.boolean;
      return PLAINROOT_VALIDATION_OK;
    case PLAINROOT_VALIDATION_PARAMETER_I64:
      output.value.i64 = input.as.i64;
      return PLAINROOT_VALIDATION_OK;
    case PLAINROOT_VALIDATION_PARAMETER_U64:
      output.value.u64 = input.as.u64;
      return PLAINROOT_VALIDATION_OK;
    case PLAINROOT_VALIDATION_PARAMETER_F64:
      if (!std::isfinite(input.as.f64))
        return PLAINROOT_VALIDATION_NON_FINITE_NUMBER;
      output.value.f64 = input.as.f64;
      return PLAINROOT_VALIDATION_OK;
    case PLAINROOT_VALIDATION_PARAMETER_TEXT:
      if (input.as.text.size && !input.as.text.data)
        return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
      if (!plainroot_validation_detail::valid_utf8(
              input.as.text.data, input.as.text.size, false))
        return PLAINROOT_VALIDATION_INVALID_UTF8;
      if (!checked_add(text_bytes, input.as.text.size, next_bytes))
        return PLAINROOT_VALIDATION_LIMIT_EXCEEDED;
      output.text.assign(input.as.text.data ? input.as.text.data : "",
                         input.as.text.size);
      text_bytes = next_bytes;
      return PLAINROOT_VALIDATION_OK;
  }
  return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
}

}  // namespace

struct plainroot_validation_report {
  std::atomic<uint32_t> references{1};
  ReportLimits limits;
  uint64_t text_bytes = 0;
  bool sealed = false;
  std::vector<IssueData> issues;
};

namespace plainroot_validation_detail {

plainroot_validation_result_v1 require_open_report(
    const plainroot_validation_report *report) noexcept {
  if (!report)
    return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
  return report->sealed ? PLAINROOT_VALIDATION_SEALED : PLAINROOT_VALIDATION_OK;
}

}  // namespace plainroot_validation_detail

extern "C" {

uint32_t plainroot_validation_abi_version_v1(void) { return 1; }

const char *plainroot_validation_result_name_v1(
    plainroot_validation_result_v1 result) {
  switch (result) {
    case PLAINROOT_VALIDATION_OK:
      return "ok";
    case PLAINROOT_VALIDATION_INVALID_ARGUMENT:
      return "invalid-argument";
    case PLAINROOT_VALIDATION_OUT_OF_MEMORY:
      return "out-of-memory";
    case PLAINROOT_VALIDATION_INTERNAL_ERROR:
      return "internal-error";
    case PLAINROOT_VALIDATION_INVALID_UTF8:
      return "invalid-utf8";
    case PLAINROOT_VALIDATION_NON_FINITE_NUMBER:
      return "non-finite-number";
    case PLAINROOT_VALIDATION_LIMIT_EXCEEDED:
      return "limit-exceeded";
    case PLAINROOT_VALIDATION_SEALED:
      return "sealed";
    case PLAINROOT_VALIDATION_INDEX_OUT_OF_RANGE:
      return "index-out-of-range";
    case PLAINROOT_VALIDATION_INVALID_ISSUE_CODE:
      return "invalid-issue-code";
    case PLAINROOT_VALIDATION_INVALID_RULE:
      return "invalid-rule";
    case PLAINROOT_VALIDATION_NOT_SEALED:
      return "not-sealed";
  }
  return "unknown-result";
}

const char *plainroot_validation_issue_code_name_v1(
    plainroot_validation_issue_code_v1 issue_code) {
  switch (issue_code) {
    case PLAINROOT_VALIDATION_ISSUE_NONE:
      return "none";
    case PLAINROOT_VALIDATION_ISSUE_REQUIRED:
      return "required";
    case PLAINROOT_VALIDATION_ISSUE_INVALID_UTF8:
      return "invalid-utf8";
    case PLAINROOT_VALIDATION_ISSUE_OUT_OF_RANGE:
      return "out-of-range";
    case PLAINROOT_VALIDATION_ISSUE_INVALID_INTEGER:
      return "invalid-integer";
    case PLAINROOT_VALIDATION_ISSUE_INVALID_UNSIGNED_INTEGER:
      return "invalid-unsigned-integer";
    case PLAINROOT_VALIDATION_ISSUE_INVALID_REAL:
      return "invalid-real";
    case PLAINROOT_VALIDATION_ISSUE_INVALID_BOOLEAN:
      return "invalid-boolean";
    case PLAINROOT_VALIDATION_ISSUE_TOO_SHORT:
      return "too-short";
    case PLAINROOT_VALIDATION_ISSUE_TOO_LONG:
      return "too-long";
    case PLAINROOT_VALIDATION_ISSUE_PATTERN_MISMATCH:
      return "pattern-mismatch";
    case PLAINROOT_VALIDATION_ISSUE_INVALID_EMAIL:
      return "invalid-email";
    case PLAINROOT_VALIDATION_ISSUE_NON_FINITE_NUMBER:
      return "non-finite-number";
    case PLAINROOT_VALIDATION_ISSUE_BLANK:
      return "blank";
  }
  return "unknown-issue";
}

plainroot_validation_result_v1 plainroot_validation_report_create_v1(
    const plainroot_validation_report_options_v1 *options,
    plainroot_validation_report **output) {
  if (!output)
    return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
  *output = nullptr;
  try {
    ReportLimits limits;
    if (!valid_report_options(options, limits))
      return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
    auto *report = new plainroot_validation_report;
    report->limits = limits;
    *output = report;
    return PLAINROOT_VALIDATION_OK;
  } catch (const std::bad_alloc&) {
    return PLAINROOT_VALIDATION_OUT_OF_MEMORY;
  } catch (...) {
    return PLAINROOT_VALIDATION_INTERNAL_ERROR;
  }
}

plainroot_validation_result_v1 plainroot_validation_report_retain_v1(
    plainroot_validation_report *report) {
  if (!report)
    return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
  uint32_t count = report->references.load(std::memory_order_relaxed);
  while (true) {
    if (!count || count == std::numeric_limits<uint32_t>::max())
      return PLAINROOT_VALIDATION_LIMIT_EXCEEDED;
    if (report->references.compare_exchange_weak(
            count, count + 1, std::memory_order_relaxed,
            std::memory_order_relaxed))
      return PLAINROOT_VALIDATION_OK;
  }
}

void plainroot_validation_report_release_v1(
    plainroot_validation_report *report) {
  if (!report)
    return;
  if (report->references.fetch_sub(1, std::memory_order_acq_rel) == 1)
    delete report;
}

plainroot_validation_result_v1 plainroot_validation_report_add_issue_v1(
    plainroot_validation_report *report,
    const plainroot_validation_issue_v1 *issue) {
  if (!report || !issue || issue->struct_size < sizeof(*issue))
    return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
  if (report->sealed)
    return PLAINROOT_VALIDATION_SEALED;
  if (!known_issue_code(issue->code))
    return PLAINROOT_VALIDATION_INVALID_ISSUE_CODE;
  if (issue->field_size && !issue->field)
    return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
  if (issue->parameter_count && !issue->parameters)
    return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
  if (report->issues.size() >= report->limits.maximum_issues ||
      issue->parameter_count > report->limits.maximum_parameters_per_issue)
    return PLAINROOT_VALIDATION_LIMIT_EXCEEDED;

  uint64_t new_text_bytes = report->text_bytes;
  if (!checked_add(new_text_bytes, issue->field_size, new_text_bytes) ||
      new_text_bytes > report->limits.maximum_text_bytes)
    return PLAINROOT_VALIDATION_LIMIT_EXCEEDED;

  try {
    IssueData copied;
    copied.code = issue->code;
    if (issue->field_size) {
      if (!plainroot_validation_detail::valid_utf8(
              issue->field, issue->field_size, true))
        return PLAINROOT_VALIDATION_INVALID_UTF8;
      copied.field.assign(issue->field, issue->field_size);
    }
    copied.parameters.reserve(issue->parameter_count);
    for (size_t i = 0; i < issue->parameter_count; ++i) {
      ParameterData parameter;
      const auto result = add_parameter(issue->parameters[i], parameter,
                                        new_text_bytes);
      if (result != PLAINROOT_VALIDATION_OK)
        return result;
      if (new_text_bytes > report->limits.maximum_text_bytes)
        return PLAINROOT_VALIDATION_LIMIT_EXCEEDED;
      copied.parameters.push_back(std::move(parameter));
    }
    report->issues.push_back(std::move(copied));
    report->text_bytes = new_text_bytes;
    return PLAINROOT_VALIDATION_OK;
  } catch (const std::bad_alloc&) {
    return PLAINROOT_VALIDATION_OUT_OF_MEMORY;
  } catch (...) {
    return PLAINROOT_VALIDATION_INTERNAL_ERROR;
  }
}

plainroot_validation_result_v1 plainroot_validation_report_seal_v1(
    plainroot_validation_report *report) {
  if (!report)
    return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
  report->sealed = true;
  return PLAINROOT_VALIDATION_OK;
}

plainroot_validation_result_v1 plainroot_validation_report_is_valid_v1(
    const plainroot_validation_report *report, uint8_t *is_valid) {
  if (!report || !is_valid)
    return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
  if (!report->sealed)
    return PLAINROOT_VALIDATION_NOT_SEALED;
  *is_valid = report->issues.empty() ? 1 : 0;
  return PLAINROOT_VALIDATION_OK;
}

plainroot_validation_result_v1 plainroot_validation_report_issue_count_v1(
    const plainroot_validation_report *report, size_t *issue_count) {
  if (!report || !issue_count)
    return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
  if (!report->sealed)
    return PLAINROOT_VALIDATION_NOT_SEALED;
  *issue_count = report->issues.size();
  return PLAINROOT_VALIDATION_OK;
}

plainroot_validation_result_v1 plainroot_validation_report_get_issue_v1(
    const plainroot_validation_report *report, size_t issue_index,
    plainroot_validation_issue_view_v1 *output) {
  if (!report || !output || output->struct_size < sizeof(*output))
    return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
  if (!report->sealed)
    return PLAINROOT_VALIDATION_NOT_SEALED;
  if (issue_index >= report->issues.size())
    return PLAINROOT_VALIDATION_INDEX_OUT_OF_RANGE;
  const IssueData& issue = report->issues[issue_index];
  output->code = issue.code;
  output->field = issue.field.data();
  output->field_size = issue.field.size();
  output->parameter_count = issue.parameters.size();
  return PLAINROOT_VALIDATION_OK;
}

plainroot_validation_result_v1 plainroot_validation_report_get_parameter_v1(
    const plainroot_validation_report *report, size_t issue_index,
    size_t parameter_index,
    plainroot_validation_parameter_view_v1 *output) {
  if (!report || !output || output->struct_size < sizeof(*output))
    return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
  if (!report->sealed)
    return PLAINROOT_VALIDATION_NOT_SEALED;
  if (issue_index >= report->issues.size())
    return PLAINROOT_VALIDATION_INDEX_OUT_OF_RANGE;
  const IssueData& issue = report->issues[issue_index];
  if (parameter_index >= issue.parameters.size())
    return PLAINROOT_VALIDATION_INDEX_OUT_OF_RANGE;
  const ParameterData& parameter = issue.parameters[parameter_index];
  output->type = parameter.type;
  output->name = parameter.name.data();
  output->name_size = parameter.name.size();
  switch (parameter.type) {
    case PLAINROOT_VALIDATION_PARAMETER_BOOL:
      output->as.boolean = parameter.value.boolean;
      break;
    case PLAINROOT_VALIDATION_PARAMETER_I64:
      output->as.i64 = parameter.value.i64;
      break;
    case PLAINROOT_VALIDATION_PARAMETER_U64:
      output->as.u64 = parameter.value.u64;
      break;
    case PLAINROOT_VALIDATION_PARAMETER_F64:
      output->as.f64 = parameter.value.f64;
      break;
    case PLAINROOT_VALIDATION_PARAMETER_TEXT:
      output->as.text.data = parameter.text.data();
      output->as.text.size = parameter.text.size();
      break;
  }
  return PLAINROOT_VALIDATION_OK;
}

}  // extern "C"
