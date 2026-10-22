#ifndef PLAINROOT_VALIDATION_VALIDATION_HPP
#define PLAINROOT_VALIDATION_VALIDATION_HPP

#include <plainroot/validation/validation.h>

#include <cstddef>
#include <cstdint>
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
};

}  // namespace plainroot::validation

#endif
