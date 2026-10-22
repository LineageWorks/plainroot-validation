#include <plainroot/validation/validation.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <variant>

namespace {

int report_error(plainroot::validation::Result result) {
  std::cerr << "validation report error: "
            << plainroot::validation::result_name(result) << '\n';
  return 1;
}

}  // namespace

int main() {
  using namespace plainroot::validation;

  Report report;
  Result result = Report::create(report);
  if (result != PLAINROOT_VALIDATION_OK)
    return report_error(result);

  Issue issue;
  issue.code = PLAINROOT_VALIDATION_ISSUE_OUT_OF_RANGE;
  issue.field = "profile.age";
  issue.parameters = {
      {"minimum", int64_t{18}},
      {"maximum", int64_t{120}},
  };
  result = report.add_issue(issue);
  if (result != PLAINROOT_VALIDATION_OK)
    return report_error(result);
  result = report.seal();
  if (result != PLAINROOT_VALIDATION_OK)
    return report_error(result);

  bool valid = false;
  if ((result = report.is_valid(valid)) != PLAINROOT_VALIDATION_OK)
    return report_error(result);
  std::size_t count = 0;
  if ((result = report.issue_count(count)) != PLAINROOT_VALIDATION_OK)
    return report_error(result);

  std::cout << (valid ? "valid" : "invalid") << " (" << count
            << (count == 1 ? " issue)\n" : " issues)\n");
  for (std::size_t i = 0; i < count; ++i) {
    Issue item;
    if ((result = report.issue(i, item)) != PLAINROOT_VALIDATION_OK)
      return report_error(result);
    std::cout << "- " << issue_code_name(item.code);
    if (!item.field.empty())
      std::cout << " field=" << item.field;
    for (const auto& parameter : item.parameters) {
      std::cout << ' ' << parameter.name << '=';
      std::visit([](const auto& value) { std::cout << value; },
                 parameter.value);
    }
    std::cout << '\n';
  }
  return 0;
}
