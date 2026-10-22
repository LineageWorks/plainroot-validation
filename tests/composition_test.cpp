#include <plainroot/validation/validation.hpp>

#include <cmath>
#include <iostream>
#include <limits>
#include <string>

using namespace plainroot::validation;

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      std::cerr << "check failed at line " << __LINE__ << ": " #condition      \
                << '\n';                                                       \
      return 1;                                                                \
    }                                                                          \
  } while (false)

int test_numeric_composition() {
  const auto base = Validators::Numbers::with(15);
  const auto first = base.isInRange(0, 10);
  const auto both = first.isInRange(20, 30);

  CHECK(base.validate().valid());
  CHECK(first.validate().status() == PLAINROOT_VALIDATION_OK);
  CHECK(!first.validate().valid());

  const auto result = both.validate("profile.age");
  CHECK(result.status() == PLAINROOT_VALIDATION_OK);
  CHECK(!result.valid());
  std::size_t count = 0;
  CHECK(result.issue_count(count) == PLAINROOT_VALIDATION_OK);
  CHECK(count == 2);

  Issue issue;
  CHECK(result.issue(0, issue) == PLAINROOT_VALIDATION_OK);
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_OUT_OF_RANGE);
  CHECK(issue.field == "profile.age");
  CHECK(std::get<int64_t>(issue.parameters[0].value) == 0);
  CHECK(std::get<int64_t>(issue.parameters[1].value) == 10);
  CHECK(result.issue(1, issue) == PLAINROOT_VALIDATION_OK);
  CHECK(std::get<int64_t>(issue.parameters[0].value) == 20);
  CHECK(std::get<int64_t>(issue.parameters[1].value) == 30);

  Report caller_report;
  CHECK(Report::create(caller_report) == PLAINROOT_VALIDATION_OK);
  CHECK(both.validate(caller_report, "profile.age") ==
        PLAINROOT_VALIDATION_OK);
  CHECK(caller_report.seal() == PLAINROOT_VALIDATION_OK);
  CHECK(caller_report.issue_count(count) == PLAINROOT_VALIDATION_OK);
  CHECK(count == 2);

  const auto bad_rule = Validators::Numbers::with(5)
                            .isInRange(10, 0)
                            .isInRange(10, 20)
                            .validate();
  CHECK(bad_rule.status() == PLAINROOT_VALIDATION_INVALID_RULE);
  CHECK(!bad_rule.valid());
  CHECK(bad_rule.issue_count(count) == PLAINROOT_VALIDATION_OK);
  CHECK(count == 0);

  const auto parsed = Validators::Numbers::parseInteger<int>("42");
  CHECK(parsed.parsed && parsed.value == 42);
  const auto parsed_real = Validators::Numbers::parseReal<double>("1.25");
  CHECK(parsed_real.parsed && parsed_real.value == 1.25);
  const auto parsed_boolean = Validators::Numbers::parseBoolean("true");
  CHECK(parsed_boolean.parsed && parsed_boolean.value);
  const auto malformed = Validators::Numbers::parseInteger<int>("42x");
  CHECK(!malformed.parsed);
  CHECK(malformed.failure_code == PLAINROOT_VALIDATION_ISSUE_INVALID_INTEGER);
  bool dependent_constraint_ran = false;
  if (parsed)
    dependent_constraint_ran = Validators::Numbers::with(parsed.value)
                                   .isInRange(0, 100)
                                   .validate()
                                   .valid();
  CHECK(dependent_constraint_ran);
  dependent_constraint_ran = false;
  if (malformed)
    dependent_constraint_ran = Validators::Numbers::with(malformed.value)
                                   .isInRange(0, 100)
                                   .validate()
                                   .valid();
  CHECK(!dependent_constraint_ran);

  const double infinity = std::numeric_limits<double>::infinity();
  const auto finite_result = Validators::Numbers::with(infinity)
                                 .isInRange(-1.0, 1.0)
                                 .isFinite()
                                 .validate();
  CHECK(finite_result.status() == PLAINROOT_VALIDATION_OK);
  CHECK(finite_result.issue_count(count) == PLAINROOT_VALIDATION_OK);
  CHECK(count == 2);
  return 0;
}

int test_text_composition_and_snapshot() {
  std::string source = "a";
  const auto text_rules = Validators::Text::with(source)
                              .isNotEmpty()
                              .hasLength(2, 8)
                              .isEmail();
  source.clear();

  const auto result = text_rules.validate("account.email");
  CHECK(result.status() == PLAINROOT_VALIDATION_OK);
  CHECK(!result.valid());
  std::size_t count = 0;
  CHECK(result.issue_count(count) == PLAINROOT_VALIDATION_OK);
  CHECK(count == 2);
  Issue issue;
  CHECK(result.issue(0, issue) == PLAINROOT_VALIDATION_OK);
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_TOO_SHORT);
  CHECK(issue.field == "account.email");
  CHECK(result.issue(1, issue) == PLAINROOT_VALIDATION_OK);
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_INVALID_EMAIL);

  Report primitive;
  CHECK(Report::create(primitive) == PLAINROOT_VALIDATION_OK);
  CHECK((TextLengthValidator{2, 8}.validate("a", primitive, "field")) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(EmailValidator{}.validate("a", primitive, "field") ==
        PLAINROOT_VALIDATION_OK);
  CHECK(primitive.seal() == PLAINROOT_VALIDATION_OK);
  CHECK(primitive.issue_count(count) == PLAINROOT_VALIDATION_OK);
  CHECK(count == 2);

  const auto rebuilt = Validators::Text::with("a")
                           .hasLength(2, 8)
                           .isEmail()
                           .validate("field");
  CHECK(rebuilt.issue_count(count) == PLAINROOT_VALIDATION_OK);
  CHECK(count == 2);
  CHECK(rebuilt.issue(0, issue) == PLAINROOT_VALIDATION_OK);
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_TOO_SHORT);
  CHECK(rebuilt.issue(1, issue) == PLAINROOT_VALIDATION_OK);
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_INVALID_EMAIL);

  const auto valid_text = Validators::Text::with("é")
                              .isNotBlank()
                              .hasLength(1, 1)
                              .hasMaximumBytes(2)
                              .validate();
  CHECK(valid_text.status() == PLAINROOT_VALIDATION_OK);
  CHECK(valid_text.valid());
  return 0;
}

int test_compiled_pattern_reuse() {
  CompiledPattern pattern;
  CHECK(CompiledPattern::compile(R"(^[a-z]+@[a-z]+\.com$)", pattern) ==
        PLAINROOT_VALIDATION_OK);

  const auto rules = Validators::Text::with("user@example.com")
                         .isNotEmpty()
                         .matches(pattern);
  pattern.reset();

  const auto result = rules.validate();
  CHECK(result.status() == PLAINROOT_VALIDATION_OK);
  CHECK(result.valid());

  CompiledPattern digits;
  CHECK(CompiledPattern::compile(R"(\d+)", digits) ==
        PLAINROOT_VALIDATION_OK);
  const auto partial = Validators::Text::with("prefix:42:suffix")
                           .matches(digits, PatternMatchMode::Partial)
                           .validate();
  CHECK(partial.status() == PLAINROOT_VALIDATION_OK);
  CHECK(partial.valid());
  return 0;
}

int main() {
  if (test_numeric_composition() != 0)
    return 1;
  if (test_text_composition_and_snapshot() != 0)
    return 1;
  if (test_compiled_pattern_reuse() != 0)
    return 1;
  return 0;
}
