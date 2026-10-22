#ifndef PLAINROOT_VALIDATION_VALIDATION_HPP
#define PLAINROOT_VALIDATION_VALIDATION_HPP

#include <plainroot/validation/validation.h>

#include <cstddef>
#include <cstdint>
#include <cmath>
#include <limits>
#include <new>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace plainroot::validation {

using Result = plainroot_validation_result_v1;
using IssueCode = plainroot_validation_issue_code_v1;
using ParameterValue = std::variant<bool, int64_t, uint64_t, double, std::string>;

struct Parameter {
  std::string name;
  ParameterValue value;
};

struct Issue {
  IssueCode code = PLAINROOT_VALIDATION_ISSUE_REQUIRED;
  std::string field;
  std::vector<Parameter> parameters;
};

struct ReportOptions {
  uint32_t maximum_issues = 0;
  uint32_t maximum_parameters_per_issue = 0;
  uint64_t maximum_text_bytes = 0;
};

struct ParseOptions {
  uint32_t maximum_input_bytes = 0;
  bool trim_ascii_whitespace = false;
  bool boolean_case_insensitive_ascii = false;
  bool boolean_accept_yes_no = false;
  bool boolean_accept_0_1 = false;
};

struct TextOptions {
  uint32_t maximum_input_bytes = 0;
};

struct PatternOptions {
  uint32_t maximum_pattern_bytes = 0;
};

enum class PatternMatchMode : uint32_t {
  Partial = PLAINROOT_VALIDATION_PATTERN_PARTIAL,
  Full = PLAINROOT_VALIDATION_PATTERN_FULL,
};

template <typename T>
struct ParseResult {
  Result result = PLAINROOT_VALIDATION_OK;
  bool parsed = false;
  T value{};
  IssueCode failure_code = PLAINROOT_VALIDATION_ISSUE_NONE;

  explicit operator bool() const noexcept {
    return result == PLAINROOT_VALIDATION_OK && parsed;
  }
};

namespace detail {

template <typename T>
inline constexpr bool is_character_code_unit_v =
    std::is_same_v<std::remove_cv_t<T>, char> ||
    std::is_same_v<std::remove_cv_t<T>, wchar_t> ||
#if defined(__cpp_char8_t)
    std::is_same_v<std::remove_cv_t<T>, char8_t> ||
#endif
    std::is_same_v<std::remove_cv_t<T>, char16_t> ||
    std::is_same_v<std::remove_cv_t<T>, char32_t>;

template <typename T>
inline constexpr bool is_supported_integer_v =
    std::is_integral_v<T> && !std::is_same_v<std::remove_cv_t<T>, bool> &&
    !is_character_code_unit_v<T> && std::numeric_limits<T>::digits <= 64;

template <typename T>
inline constexpr bool is_supported_real_v =
    std::is_same_v<std::remove_cv_t<T>, float> ||
    std::is_same_v<std::remove_cv_t<T>, double>;

template <typename T>
inline constexpr bool is_supported_number_v =
    is_supported_integer_v<T> || is_supported_real_v<T>;

inline plainroot_validation_parse_options_v1 native_parse_options(
    const ParseOptions& options) noexcept {
  plainroot_validation_parse_options_v1 native{};
  native.struct_size = sizeof(native);
  native.maximum_input_bytes = options.maximum_input_bytes;
  native.trim_ascii_whitespace = options.trim_ascii_whitespace ? 1 : 0;
  native.boolean_case_insensitive_ascii =
      options.boolean_case_insensitive_ascii ? 1 : 0;
  native.boolean_accept_yes_no = options.boolean_accept_yes_no ? 1 : 0;
  native.boolean_accept_0_1 = options.boolean_accept_0_1 ? 1 : 0;
  return native;
}

inline plainroot_validation_text_options_v1 native_text_options(
    const TextOptions& options) noexcept {
  plainroot_validation_text_options_v1 native{};
  native.struct_size = sizeof(native);
  native.maximum_input_bytes = options.maximum_input_bytes;
  return native;
}

inline plainroot_validation_pattern_options_v1 native_pattern_options(
    const PatternOptions& options) noexcept {
  plainroot_validation_pattern_options_v1 native{};
  native.struct_size = sizeof(native);
  native.maximum_pattern_bytes = options.maximum_pattern_bytes;
  return native;
}

struct ReportAccess;

}  // namespace detail

inline std::string_view result_name(Result result) noexcept {
  const char *name = plainroot_validation_result_name_v1(result);
  return name ? std::string_view(name) : std::string_view{};
}

inline std::string_view issue_code_name(IssueCode issue_code) noexcept {
  const char *name = plainroot_validation_issue_code_name_v1(issue_code);
  return name ? std::string_view(name) : std::string_view{};
}

class Report {
 public:
  Report() = default;
  ~Report() { reset(); }
  Report(const Report&) = delete;
  Report& operator=(const Report&) = delete;

  Report(Report&& other) noexcept
      : handle_(std::exchange(other.handle_, nullptr)),
        sealed_(std::exchange(other.sealed_, false)) {}

  Report& operator=(Report&& other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, nullptr);
      sealed_ = std::exchange(other.sealed_, false);
    }
    return *this;
  }

  static Result create(Report& output, ReportOptions options = {}) noexcept {
    plainroot_validation_report_options_v1 native_options{};
    native_options.struct_size = sizeof(native_options);
    native_options.maximum_issues = options.maximum_issues;
    native_options.maximum_parameters_per_issue =
        options.maximum_parameters_per_issue;
    native_options.maximum_text_bytes = options.maximum_text_bytes;
    plainroot_validation_report *native_report = nullptr;
    const Result result = plainroot_validation_report_create_v1(
        &native_options, &native_report);
    if (result == PLAINROOT_VALIDATION_OK) {
      output.reset();
      output.handle_ = native_report;
      output.sealed_ = false;
    }
    return result;
  }

  Result add_issue(const Issue& issue) noexcept {
    if (!handle_)
      return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
    if (issue.parameters.size() > PLAINROOT_VALIDATION_MAXIMUM_PARAMETERS_PER_ISSUE)
      return PLAINROOT_VALIDATION_LIMIT_EXCEEDED;

    try {
      std::vector<plainroot_validation_parameter_v1> parameters;
      parameters.reserve(issue.parameters.size());
      for (const auto& parameter : issue.parameters) {
        plainroot_validation_parameter_v1 native{};
        native.struct_size = sizeof(native);
        native.name = parameter.name.data();
        native.name_size = parameter.name.size();
        std::visit(
            [&native](const auto& value) {
              using T = std::decay_t<decltype(value)>;
              if constexpr (std::is_same_v<T, bool>) {
                native.type = PLAINROOT_VALIDATION_PARAMETER_BOOL;
                native.as.boolean = value ? 1 : 0;
              } else if constexpr (std::is_same_v<T, int64_t>) {
                native.type = PLAINROOT_VALIDATION_PARAMETER_I64;
                native.as.i64 = value;
              } else if constexpr (std::is_same_v<T, uint64_t>) {
                native.type = PLAINROOT_VALIDATION_PARAMETER_U64;
                native.as.u64 = value;
              } else if constexpr (std::is_same_v<T, double>) {
                native.type = PLAINROOT_VALIDATION_PARAMETER_F64;
                native.as.f64 = value;
              } else {
                native.type = PLAINROOT_VALIDATION_PARAMETER_TEXT;
                native.as.text.data = value.data();
                native.as.text.size = value.size();
              }
            },
            parameter.value);
        parameters.push_back(native);
      }

      plainroot_validation_issue_v1 native_issue{};
      native_issue.struct_size = sizeof(native_issue);
      native_issue.code = issue.code;
      native_issue.field = issue.field.data();
      native_issue.field_size = issue.field.size();
      native_issue.parameters = parameters.empty() ? nullptr : parameters.data();
      native_issue.parameter_count = parameters.size();
      return plainroot_validation_report_add_issue_v1(handle_, &native_issue);
    } catch (const std::bad_alloc&) {
      return PLAINROOT_VALIDATION_OUT_OF_MEMORY;
    } catch (...) {
      return PLAINROOT_VALIDATION_INTERNAL_ERROR;
    }
  }

  Result seal() noexcept {
    if (!handle_)
      return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
    const Result result = plainroot_validation_report_seal_v1(handle_);
    if (result == PLAINROOT_VALIDATION_OK)
      sealed_ = true;
    return result;
  }

  Result is_valid(bool& output) const noexcept {
    if (!handle_)
      return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
    uint8_t native_output = 0;
    const Result result =
        plainroot_validation_report_is_valid_v1(handle_, &native_output);
    if (result == PLAINROOT_VALIDATION_OK)
      output = native_output != 0;
    return result;
  }

  Result issue_count(std::size_t& output) const noexcept {
    if (!handle_)
      return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
    return plainroot_validation_report_issue_count_v1(handle_, &output);
  }

  Result issue(std::size_t index, Issue& output) const noexcept {
    if (!handle_)
      return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
    try {
      plainroot_validation_issue_view_v1 native_issue{};
      native_issue.struct_size = sizeof(native_issue);
      Result result = plainroot_validation_report_get_issue_v1(
          handle_, index, &native_issue);
      if (result != PLAINROOT_VALIDATION_OK)
        return result;

      Issue copied;
      copied.code = native_issue.code;
      if (native_issue.field_size)
        copied.field.assign(native_issue.field, native_issue.field_size);
      copied.parameters.reserve(native_issue.parameter_count);
      for (std::size_t i = 0; i < native_issue.parameter_count; ++i) {
        plainroot_validation_parameter_view_v1 native_parameter{};
        native_parameter.struct_size = sizeof(native_parameter);
        result = plainroot_validation_report_get_parameter_v1(
            handle_, index, i, &native_parameter);
        if (result != PLAINROOT_VALIDATION_OK)
          return result;
        Parameter parameter;
        parameter.name.assign(native_parameter.name, native_parameter.name_size);
        switch (native_parameter.type) {
          case PLAINROOT_VALIDATION_PARAMETER_BOOL:
            parameter.value = native_parameter.as.boolean != 0;
            break;
          case PLAINROOT_VALIDATION_PARAMETER_I64:
            parameter.value = native_parameter.as.i64;
            break;
          case PLAINROOT_VALIDATION_PARAMETER_U64:
            parameter.value = native_parameter.as.u64;
            break;
          case PLAINROOT_VALIDATION_PARAMETER_F64:
            parameter.value = native_parameter.as.f64;
            break;
          case PLAINROOT_VALIDATION_PARAMETER_TEXT:
            if (native_parameter.as.text.size) {
              parameter.value = std::string(native_parameter.as.text.data,
                                            native_parameter.as.text.size);
            } else {
              parameter.value = std::string{};
            }
            break;
          default:
            return PLAINROOT_VALIDATION_INTERNAL_ERROR;
        }
        copied.parameters.push_back(std::move(parameter));
      }
      output = std::move(copied);
      return PLAINROOT_VALIDATION_OK;
    } catch (const std::bad_alloc&) {
      return PLAINROOT_VALIDATION_OUT_OF_MEMORY;
    } catch (...) {
      return PLAINROOT_VALIDATION_INTERNAL_ERROR;
    }
  }

  bool sealed() const noexcept { return sealed_; }
  explicit operator bool() const noexcept { return handle_ != nullptr; }

  void reset() noexcept {
    if (handle_)
      plainroot_validation_report_release_v1(handle_);
    handle_ = nullptr;
    sealed_ = false;
  }

 private:
  plainroot_validation_report *handle_ = nullptr;
  bool sealed_ = false;
  friend struct detail::ReportAccess;
};

namespace detail {

struct ReportAccess {
  static plainroot_validation_report *get(Report& report) noexcept {
    return report.handle_;
  }
};

}  // namespace detail

template <typename T>
  requires detail::is_supported_integer_v<T>
ParseResult<T> parse_integer(std::string_view text,
                             ParseOptions options = {}) noexcept {
  ParseResult<T> output;
  const auto native_options = detail::native_parse_options(options);
  if constexpr (std::is_signed_v<T>) {
    plainroot_validation_parse_i64_result_v1 parsed{};
    parsed.struct_size = sizeof(parsed);
    output.result = plainroot_validation_parse_i64_text_v1(
        text.data(), text.size(), &native_options, &parsed);
    if (output.result != PLAINROOT_VALIDATION_OK)
      return output;
    output.failure_code = parsed.failure_code;
    if (!parsed.parsed)
      return output;
    const int64_t minimum =
        static_cast<int64_t>(std::numeric_limits<T>::min());
    const int64_t maximum =
        static_cast<int64_t>(std::numeric_limits<T>::max());
    if (parsed.value < minimum || parsed.value > maximum) {
      output.failure_code = PLAINROOT_VALIDATION_ISSUE_OUT_OF_RANGE;
      return output;
    }
    output.value = static_cast<T>(parsed.value);
  } else {
    plainroot_validation_parse_u64_result_v1 parsed{};
    parsed.struct_size = sizeof(parsed);
    output.result = plainroot_validation_parse_u64_text_v1(
        text.data(), text.size(), &native_options, &parsed);
    if (output.result != PLAINROOT_VALIDATION_OK)
      return output;
    output.failure_code = parsed.failure_code;
    if (!parsed.parsed)
      return output;
    const uint64_t maximum =
        static_cast<uint64_t>(std::numeric_limits<T>::max());
    if (parsed.value > maximum) {
      output.failure_code = PLAINROOT_VALIDATION_ISSUE_OUT_OF_RANGE;
      return output;
    }
    output.value = static_cast<T>(parsed.value);
  }
  output.parsed = true;
  output.failure_code = PLAINROOT_VALIDATION_ISSUE_NONE;
  return output;
}

template <typename T>
  requires detail::is_supported_real_v<T>
ParseResult<T> parse_real(std::string_view text,
                          ParseOptions options = {}) noexcept {
  ParseResult<T> output;
  const auto native_options = detail::native_parse_options(options);
  plainroot_validation_parse_f64_result_v1 parsed{};
  parsed.struct_size = sizeof(parsed);
  output.result = plainroot_validation_parse_f64_text_v1(
      text.data(), text.size(), &native_options, &parsed);
  if (output.result != PLAINROOT_VALIDATION_OK)
    return output;
  output.failure_code = parsed.failure_code;
  if (!parsed.parsed)
    return output;

  const T converted = static_cast<T>(parsed.value);
  if (!std::isfinite(converted) ||
      (parsed.value != 0.0 && converted == static_cast<T>(0))) {
    output.failure_code = PLAINROOT_VALIDATION_ISSUE_OUT_OF_RANGE;
    return output;
  }
  output.value = converted;
  output.parsed = true;
  output.failure_code = PLAINROOT_VALIDATION_ISSUE_NONE;
  return output;
}

inline ParseResult<bool> parse_boolean(std::string_view text,
                                       ParseOptions options = {}) noexcept {
  ParseResult<bool> output;
  const auto native_options = detail::native_parse_options(options);
  plainroot_validation_parse_bool_result_v1 parsed{};
  parsed.struct_size = sizeof(parsed);
  output.result = plainroot_validation_parse_bool_text_v1(
      text.data(), text.size(), &native_options, &parsed);
  if (output.result == PLAINROOT_VALIDATION_OK) {
    output.parsed = parsed.parsed != 0;
    output.value = parsed.value != 0;
    output.failure_code = parsed.failure_code;
  }
  return output;
}

template <typename T>
  requires detail::is_supported_number_v<T>
class RangeValidator {
 public:
  RangeValidator(T minimum, T maximum) noexcept
      : minimum_(minimum), maximum_(maximum) {}

  Result validate(T value, Report& report,
                  std::string_view field = {}) const noexcept {
    auto *native_report = detail::ReportAccess::get(report);
    if (!native_report)
      return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
    if constexpr (detail::is_supported_integer_v<T>) {
      if constexpr (std::is_signed_v<T>) {
        return plainroot_validation_validate_range_i64_v1(
            native_report, field.data(), field.size(),
            static_cast<int64_t>(value), static_cast<int64_t>(minimum_),
            static_cast<int64_t>(maximum_));
      } else {
        return plainroot_validation_validate_range_u64_v1(
            native_report, field.data(), field.size(),
            static_cast<uint64_t>(value), static_cast<uint64_t>(minimum_),
            static_cast<uint64_t>(maximum_));
      }
    } else {
      return plainroot_validation_validate_range_f64_v1(
          native_report, field.data(), field.size(), static_cast<double>(value),
          static_cast<double>(minimum_), static_cast<double>(maximum_));
    }
  }

 private:
  T minimum_;
  T maximum_;
};

template <typename T>
  requires detail::is_supported_real_v<T>
class FiniteNumberValidator {
 public:
  Result validate(T value, Report& report,
                  std::string_view field = {}) const noexcept {
    auto *native_report = detail::ReportAccess::get(report);
    if (!native_report)
      return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
    return plainroot_validation_validate_finite_f64_v1(
        native_report, field.data(), field.size(), static_cast<double>(value));
  }
};

class RequiredTextValidator {
 public:
  Result validate(std::string_view text, Report& report,
                  std::string_view field = {},
                  TextOptions options = {}) const noexcept {
    auto *native_report = detail::ReportAccess::get(report);
    if (!native_report)
      return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
    const auto native_options = detail::native_text_options(options);
    return plainroot_validation_validate_required_text_v1(
        native_report, field.data(), field.size(), text.data(), text.size(),
        &native_options);
  }
};

class NonblankTextValidator {
 public:
  Result validate(std::string_view text, Report& report,
                  std::string_view field = {},
                  TextOptions options = {}) const noexcept {
    auto *native_report = detail::ReportAccess::get(report);
    if (!native_report)
      return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
    const auto native_options = detail::native_text_options(options);
    return plainroot_validation_validate_nonblank_text_v1(
        native_report, field.data(), field.size(), text.data(), text.size(),
        &native_options);
  }
};

class TextLengthValidator {
 public:
  TextLengthValidator(uint32_t minimum, uint32_t maximum) noexcept
      : minimum_(minimum), maximum_(maximum) {}

  Result validate(std::string_view text, Report& report,
                  std::string_view field = {},
                  TextOptions options = {}) const noexcept {
    auto *native_report = detail::ReportAccess::get(report);
    if (!native_report)
      return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
    const auto native_options = detail::native_text_options(options);
    return plainroot_validation_validate_text_length_v1(
        native_report, field.data(), field.size(), text.data(), text.size(),
        minimum_, maximum_, &native_options);
  }

 private:
  uint32_t minimum_;
  uint32_t maximum_;
};

class TextByteLimitValidator {
 public:
  explicit TextByteLimitValidator(uint32_t maximum_bytes) noexcept
      : maximum_bytes_(maximum_bytes) {}

  Result validate(std::string_view text, Report& report,
                  std::string_view field = {},
                  TextOptions options = {}) const noexcept {
    auto *native_report = detail::ReportAccess::get(report);
    if (!native_report)
      return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
    const auto native_options = detail::native_text_options(options);
    return plainroot_validation_validate_text_max_bytes_v1(
        native_report, field.data(), field.size(), text.data(), text.size(),
        maximum_bytes_, &native_options);
  }

 private:
  uint32_t maximum_bytes_;
};

class EmailValidator {
 public:
  Result validate(std::string_view text, Report& report,
                  std::string_view field = {},
                  TextOptions options = {}) const noexcept {
    auto *native_report = detail::ReportAccess::get(report);
    if (!native_report)
      return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
    const auto native_options = detail::native_text_options(options);
    return plainroot_validation_validate_email_text_v1(
        native_report, field.data(), field.size(), text.data(), text.size(),
        &native_options);
  }
};

class CompiledPattern {
 public:
  CompiledPattern() = default;
  ~CompiledPattern() { reset(); }
  CompiledPattern(const CompiledPattern&) = delete;
  CompiledPattern& operator=(const CompiledPattern&) = delete;

  CompiledPattern(CompiledPattern&& other) noexcept
      : handle_(std::exchange(other.handle_, nullptr)) {}

  CompiledPattern& operator=(CompiledPattern&& other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }

  static Result compile(std::string_view pattern, CompiledPattern& output,
                        PatternOptions options = {}) noexcept {
    const auto native_options = detail::native_pattern_options(options);
    plainroot_validation_pattern *native_pattern = nullptr;
    const auto result = plainroot_validation_pattern_compile_v1(
        pattern.data(), pattern.size(), &native_options, &native_pattern);
    if (result != PLAINROOT_VALIDATION_OK)
      return result;
    output.reset();
    output.handle_ = native_pattern;
    return PLAINROOT_VALIDATION_OK;
  }

  Result validate(std::string_view text, Report& report,
                  std::string_view field = {},
                  PatternMatchMode mode = PatternMatchMode::Full,
                  TextOptions options = {}) const noexcept {
    auto *native_report = detail::ReportAccess::get(report);
    if (!native_report || !handle_)
      return PLAINROOT_VALIDATION_INVALID_ARGUMENT;
    const auto native_options = detail::native_text_options(options);
    return plainroot_validation_validate_pattern_v1(
        native_report, field.data(), field.size(), text.data(), text.size(),
        &native_options, handle_,
        static_cast<plainroot_validation_pattern_match_mode_v1>(mode));
  }

  void reset() noexcept {
    if (handle_)
      plainroot_validation_pattern_release_v1(handle_);
    handle_ = nullptr;
  }

  explicit operator bool() const noexcept { return handle_ != nullptr; }

 private:
  plainroot_validation_pattern *handle_ = nullptr;
};

}  // namespace plainroot::validation

#endif
