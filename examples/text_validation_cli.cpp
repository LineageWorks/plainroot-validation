#include <plainroot/validation/validation.hpp>

#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>
#include <variant>

namespace {

using namespace plainroot::validation;

constexpr std::string_view kDisplayNameField = "profile.display_name";
constexpr std::string_view kEmailField = "profile.email";
constexpr std::size_t kMaximumLineBytes = 64 * 1024;

enum class LineReadStatus { Complete, EndOfInput, TooLong, IoError };

LineReadStatus read_line(std::string_view prompt, std::string& value) {
  std::cerr << prompt << std::flush;
  char character = '\0';
  while (std::cin.get(character)) {
    if (character == '\n')
      return LineReadStatus::Complete;
    if (value.size() == kMaximumLineBytes)
      return LineReadStatus::TooLong;
    value.push_back(character);
  }

  if (std::cin.bad())
    return LineReadStatus::IoError;
  return value.empty() ? LineReadStatus::EndOfInput
                       : LineReadStatus::Complete;
}

int read_field(std::string_view prompt, std::string_view field,
               std::string& value) {
  const LineReadStatus status = read_line(prompt, value);
  if (status == LineReadStatus::Complete)
    return 0;
  if (status == LineReadStatus::TooLong) {
    std::cerr << field << ": input-too-long maximum_bytes="
              << kMaximumLineBytes << '\n';
    return 2;
  }

  std::cerr << (status == LineReadStatus::IoError
                    ? "unable to read input\n"
                    : "input ended before all fields were entered\n");
  return 1;
}

int print_result(const ValidationResult& result, std::string_view field) {
  if (result.status() != PLAINROOT_VALIDATION_OK) {
    std::cerr << "validation operation failed: "
              << result_name(result.status()) << '\n';
    return 1;
  }

  if (result.valid()) {
    std::cout << field << ": valid\n";
    return 0;
  }

  std::size_t issue_count = 0;
  if (result.issue_count(issue_count) != PLAINROOT_VALIDATION_OK) {
    std::cerr << "unable to inspect validation result\n";
    return 1;
  }

  for (std::size_t index = 0; index < issue_count; ++index) {
    Issue issue;
    if (result.issue(index, issue) != PLAINROOT_VALIDATION_OK) {
      std::cerr << "unable to read validation issue\n";
      return 1;
    }

    std::cerr << issue.field << ": " << issue_code_name(issue.code);
    for (const auto& parameter : issue.parameters) {
      std::cerr << ' ' << parameter.name << '=';
      std::visit([](const auto& value) { std::cerr << value; },
                 parameter.value);
    }
    std::cerr << '\n';
  }
  return 2;
}

}  // namespace

int main() {
  using namespace plainroot::validation;

  std::string display_name;
  std::string email;
  const int display_name_input_status =
      read_field("Display name: ", kDisplayNameField, display_name);
  if (display_name_input_status != 0)
    return display_name_input_status;
  const int email_input_status =
      read_field("Email address: ", kEmailField, email);
  if (email_input_status != 0)
    return email_input_status;

  const auto display_name_result =
      Validators::Text::with(display_name)
          .isNotBlank()
          .hasLength(1, 80)
          .hasMaximumBytes(256)
          .validate(kDisplayNameField);
  const auto email_result = Validators::Text::with(email)
                                .isNotBlank()
                                .isEmail()
                                .validate(kEmailField);

  const int display_name_status =
      print_result(display_name_result, kDisplayNameField);
  const int email_status = print_result(email_result, kEmailField);
  if (display_name_status == 1 || email_status == 1)
    return 1;
  if (display_name_status == 2 || email_status == 2)
    return 2;
  return 0;
}
