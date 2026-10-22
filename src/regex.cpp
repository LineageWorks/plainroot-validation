#include <plainroot/validation/validation.h>

#include "report_internal.hpp"
#include "text_internal.hpp"

#include <re2/re2.h>

#include <absl/strings/string_view.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>

struct plainroot_validation_pattern {
  plainroot_validation_pattern(absl::string_view pattern,
                               const RE2::Options& options)
      : expression(pattern, options) {}

  RE2 expression;
};

namespace {

struct PatternSettings {
  uint32_t maximum_pattern_bytes =
      PLAINROOT_VALIDATION_DEFAULT_MAXIMUM_PATTERN_BYTES;
};

bool read_pattern_settings(
    const plainroot_validation_pattern_options_v1 *options,
    PatternSettings& settings) noexcept {
  if (!options)
    return true;
  if (options->struct_size < sizeof(uint32_t))
    return false;
  if (options->struct_size >= offsetof(plainroot_validation_pattern_options_v1,
                                       maximum_pattern_bytes) +
                                  sizeof(options->maximum_pattern_bytes) &&
      options->maximum_pattern_bytes) {
    settings.maximum_pattern_bytes = options->maximum_pattern_bytes;
  }
  return settings.maximum_pattern_bytes <=
         PLAINROOT_VALIDATION_MAXIMUM_PATTERN_BYTES;
}

plainroot_validation_result_v1
append_mismatch(plainroot_validation_report *report, const char *field,
                size_t field_size) {
  plainroot_validation_issue_v1 issue{};
  issue.struct_size = sizeof(issue);
  issue.code = PLAINROOT_VALIDATION_ISSUE_PATTERN_MISMATCH;
  issue.field = field;
  issue.field_size = field_size;
  return plainroot_validation_report_add_issue_v1(report, &issue);
}

}  // namespace

extern "C" {

plainroot_validation_result_v1 plainroot_validation_pattern_compile_v1(
    const char *pattern, size_t pattern_size,
    const plainroot_validation_pattern_options_v1 *options,
    plainroot_validation_pattern **output) {
  if (!output)
    return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
  *output = nullptr;
  if (pattern_size && !pattern)
    return PLAINROOT_VALIDATION_INVALID_ARGUMENT;

  PatternSettings settings;
  if (!read_pattern_settings(options, settings))
    return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
  if (pattern_size > settings.maximum_pattern_bytes)
    return PLAINROOT_VALIDATION_LIMIT_EXCEEDED;
  if (!plainroot_validation_detail::valid_utf8(pattern, pattern_size, false))
    return PLAINROOT_VALIDATION_INVALID_UTF8;

  try {
    RE2::Options re2_options;
    re2_options.set_encoding(RE2::Options::EncodingUTF8);
    re2_options.set_log_errors(false);
    re2_options.set_max_mem(PLAINROOT_VALIDATION_REGEX_MAXIMUM_MEMORY_BYTES);
    re2_options.set_never_capture(true);

    const auto view = absl::string_view(pattern ? pattern : "", pattern_size);
    auto candidate =
        std::make_unique<plainroot_validation_pattern>(view, re2_options);
    if (candidate->expression.error_code() == RE2::ErrorPatternTooLarge)
      return PLAINROOT_VALIDATION_LIMIT_EXCEEDED;
    if (candidate->expression.error_code() == RE2::ErrorInternal)
      return PLAINROOT_VALIDATION_INTERNAL_ERROR;
    if (!candidate->expression.ok())
      return PLAINROOT_VALIDATION_INVALID_RULE;
    *output = candidate.release();
    return PLAINROOT_VALIDATION_OK;
  } catch (const std::bad_alloc&) {
    return PLAINROOT_VALIDATION_OUT_OF_MEMORY;
  } catch (...) {
    return PLAINROOT_VALIDATION_INTERNAL_ERROR;
  }
}

void plainroot_validation_pattern_release_v1(
    plainroot_validation_pattern *pattern) {
  delete pattern;
}

plainroot_validation_result_v1 plainroot_validation_validate_pattern_v1(
    plainroot_validation_report *report, const char *field, size_t field_size,
    const char *text, size_t text_size,
    const plainroot_validation_text_options_v1 *text_options,
    const plainroot_validation_pattern *pattern,
    plainroot_validation_pattern_match_mode_v1 mode) {
  if (!pattern || (mode != PLAINROOT_VALIDATION_PATTERN_PARTIAL &&
                   mode != PLAINROOT_VALIDATION_PATTERN_FULL))
    return PLAINROOT_VALIDATION_INVALID_ARGUMENT;

  std::string_view prepared;
  size_t invalid_offset = 0;
  const auto status = plainroot_validation_detail::prepare_text_check(
      report, field, field_size, text, text_size, text_options, prepared,
      invalid_offset);
  if (status == PLAINROOT_VALIDATION_INVALID_UTF8)
    return plainroot_validation_detail::append_invalid_utf8(
        report, field, field_size, invalid_offset);
  if (status != PLAINROOT_VALIDATION_OK)
    return status;

  try {
    const auto input = absl::string_view(prepared.data(), prepared.size());
    const bool matched = mode == PLAINROOT_VALIDATION_PATTERN_FULL
                             ? RE2::FullMatch(input, pattern->expression)
                             : RE2::PartialMatch(input, pattern->expression);
    if (matched)
      return PLAINROOT_VALIDATION_OK;
    return append_mismatch(report, field, field_size);
  } catch (const std::bad_alloc&) {
    return PLAINROOT_VALIDATION_OUT_OF_MEMORY;
  } catch (...) {
    return PLAINROOT_VALIDATION_INTERNAL_ERROR;
  }
}

}  // extern "C"
