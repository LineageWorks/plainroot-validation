#include <plainroot/validation/validation.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <variant>

namespace {

using namespace plainroot::validation;

constexpr std::string_view kAgeField = "profile.age";

int report_error(Result result) {
  std::cerr << "validation operation failed: " << result_name(result) << '\n';
  return 1;
}

void print_issue(const Issue& issue) {
  std::cerr << issue_code_name(issue.code);
  if (!issue.field.empty())
    std::cerr << " field=" << issue.field;
  for (const auto& parameter : issue.parameters) {
    std::cerr << ' ' << parameter.name << '=';
    std::visit([](const auto& value) { std::cerr << value; }, parameter.value);
  }
  std::cerr << '\n';
}

int print_result(const ValidationResult& result) {
  if (result.status() != PLAINROOT_VALIDATION_OK)
    return report_error(result.status());

  std::size_t count = 0;
  const Result status = result.issue_count(count);
  if (status != PLAINROOT_VALIDATION_OK)
    return report_error(status);
  for (std::size_t i = 0; i < count; ++i) {
    Issue issue;
    const Result issue_status = result.issue(i, issue);
    if (issue_status != PLAINROOT_VALIDATION_OK)
      return report_error(issue_status);
    print_issue(issue);
  }
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  using namespace plainroot::validation;

  if (argc != 2) {
    std::cerr << "usage: plainroot-validation-cli AGE\n";
    return 64;
  }

  ParseOptions parse_options;
  parse_options.trim_ascii_whitespace = true;
  const auto parsed_age = parse_integer<int64_t>(argv[1], parse_options);
  if (parsed_age.result != PLAINROOT_VALIDATION_OK)
    return report_error(parsed_age.result);
  if (!parsed_age) {
    std::cerr << issue_code_name(parsed_age.failure_code)
              << " field=" << kAgeField << '\n';
    return 2;
  }

  const auto age_result = Validators::Numbers::with(parsed_age.value)
                              .isInRange(int64_t{0}, int64_t{130})
                              .validate(kAgeField);
  if (age_result.status() != PLAINROOT_VALIDATION_OK)
    return report_error(age_result.status());
  if (!age_result.valid())
    return print_result(age_result);

  std::cout << "valid profile.age=" << parsed_age.value << '\n';
  return 0;
}
