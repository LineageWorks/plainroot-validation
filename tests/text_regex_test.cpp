#include <plainroot/validation/validation.hpp>

#include <iostream>
#include <string>
#include <string_view>
#include <type_traits>

using namespace plainroot::validation;

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      std::cerr << "check failed at line " << __LINE__ << ": " #condition      \
                << '\n';                                                       \
      return 1;                                                                \
    }                                                                          \
  } while (false)

static_assert(!std::is_copy_constructible_v<CompiledPattern>);
static_assert(std::is_move_constructible_v<CompiledPattern>);

int test_text_rules() {
  Report report;
  CHECK(Report::create(report) == PLAINROOT_VALIDATION_OK);

  CHECK(RequiredTextValidator{}.validate("", report, "name") ==
        PLAINROOT_VALIDATION_OK);
  CHECK(RequiredTextValidator{}.validate(" ", report, "name") ==
        PLAINROOT_VALIDATION_OK);
  CHECK(NonblankTextValidator{}.validate(" \t\n\xc2\xa0\xe3\x80\x80", report,
                                         "name") == PLAINROOT_VALIDATION_OK);
  CHECK(NonblankTextValidator{}.validate("", report, "name") ==
        PLAINROOT_VALIDATION_OK);
  CHECK(NonblankTextValidator{}.validate("\xe2\x80\x8b", report, "name") ==
        PLAINROOT_VALIDATION_OK);

  CHECK((TextLengthValidator{2, 2}.validate("é🙂", report, "title")) ==
        PLAINROOT_VALIDATION_OK);
  CHECK((TextLengthValidator{2, 2}.validate("é", report, "title")) ==
        PLAINROOT_VALIDATION_OK);
  CHECK((TextLengthValidator{0, 0}.validate("", report, "title")) ==
        PLAINROOT_VALIDATION_OK);
  CHECK((TextLengthValidator{2, 1}.validate("x", report, "invalid-rule")) ==
        PLAINROOT_VALIDATION_INVALID_RULE);
  CHECK((TextLengthValidator{2, 2}.validate("x", report, "short")) ==
        PLAINROOT_VALIDATION_OK);
  CHECK((TextLengthValidator{0, 1}.validate("ab", report, "long")) ==
        PLAINROOT_VALIDATION_OK);

  CHECK((TextByteLimitValidator{1}.validate("é", report, "byte-limit")) ==
        PLAINROOT_VALIDATION_OK);
  CHECK((TextByteLimitValidator{2}.validate("é", report, "byte-limit")) ==
        PLAINROOT_VALIDATION_OK);

  CHECK(EmailValidator{}.validate("person.name+tag@example-domain.com", report,
                                  "email") == PLAINROOT_VALIDATION_OK);
  CHECK(EmailValidator{}.validate("a..b@example.com", report, "email") ==
        PLAINROOT_VALIDATION_OK);
  CHECK(EmailValidator{}.validate("person@localhost", report, "email") ==
        PLAINROOT_VALIDATION_OK);
  CHECK(EmailValidator{}.validate("é@example.com", report, "email") ==
        PLAINROOT_VALIDATION_OK);

  const char invalid_utf8[] = {static_cast<char>(0xc3), '('};
  CHECK(NonblankTextValidator{}.validate(
            std::string_view(invalid_utf8, sizeof(invalid_utf8)), report,
            "invalid-utf8") == PLAINROOT_VALIDATION_OK);

  TextOptions small_input;
  small_input.maximum_input_bytes = 3;
  CHECK(RequiredTextValidator{}.validate("four", report, "limited",
                                         small_input) ==
        PLAINROOT_VALIDATION_LIMIT_EXCEEDED);
  TextOptions oversized_limit;
  oversized_limit.maximum_input_bytes =
      PLAINROOT_VALIDATION_MAXIMUM_INPUT_BYTES + 1;
  CHECK(RequiredTextValidator{}.validate("x", report, "bad-limit",
                                         oversized_limit) ==
        PLAINROOT_VALIDATION_INVALID_ARGUMENT);
  const std::string above_default_input(
      PLAINROOT_VALIDATION_DEFAULT_MAXIMUM_INPUT_BYTES + 1, 'x');
  CHECK(RequiredTextValidator{}.validate(above_default_input, report,
                                         "default-limit") ==
        PLAINROOT_VALIDATION_LIMIT_EXCEEDED);

  CHECK(report.seal() == PLAINROOT_VALIDATION_OK);
  std::size_t count = 0;
  CHECK(report.issue_count(count) == PLAINROOT_VALIDATION_OK);
  CHECK(count == 10);

  Issue issue;
  CHECK(report.issue(0, issue) == PLAINROOT_VALIDATION_OK);
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_REQUIRED);
  CHECK(issue.field == "name");
  CHECK(report.issue(1, issue) == PLAINROOT_VALIDATION_OK);
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_BLANK);
  CHECK(report.issue(2, issue) == PLAINROOT_VALIDATION_OK);
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_BLANK);
  CHECK(report.issue(3, issue) == PLAINROOT_VALIDATION_OK);
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_TOO_SHORT);
  CHECK(issue.parameters.size() == 1);
  CHECK(issue.parameters[0].name == "minimum");
  CHECK(std::get<uint64_t>(issue.parameters[0].value) == 2);
  CHECK(report.issue(4, issue) == PLAINROOT_VALIDATION_OK);
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_TOO_LONG);
  CHECK(issue.parameters[0].name == "maximum");
  CHECK(report.issue(5, issue) == PLAINROOT_VALIDATION_OK);
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_TOO_LONG);
  CHECK(issue.parameters[0].name == "maximum_bytes");
  CHECK(report.issue(6, issue) == PLAINROOT_VALIDATION_OK);
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_INVALID_EMAIL);
  CHECK(report.issue(7, issue) == PLAINROOT_VALIDATION_OK);
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_INVALID_EMAIL);
  CHECK(report.issue(8, issue) == PLAINROOT_VALIDATION_OK);
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_INVALID_EMAIL);
  CHECK(report.issue(9, issue) == PLAINROOT_VALIDATION_OK);
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_INVALID_UTF8);
  CHECK(issue.parameters[0].name == "offset");
  CHECK(std::get<uint64_t>(issue.parameters[0].value) == 0);
  return 0;
}

int test_patterns() {
  CompiledPattern pattern;
  CHECK(CompiledPattern::compile(R"(^[A-Za-z][A-Za-z0-9_]{1,15}$)", pattern) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(static_cast<bool>(pattern));

  Report report;
  CHECK(Report::create(report) == PLAINROOT_VALIDATION_OK);
  CHECK(pattern.validate("User_01", report, "username") ==
        PLAINROOT_VALIDATION_OK);
  CHECK(pattern.validate("1bad", report, "username") ==
        PLAINROOT_VALIDATION_OK);
  CompiledPattern unanchored;
  CHECK(CompiledPattern::compile(R"([A-Za-z][A-Za-z0-9_]{1,15})", unanchored) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(unanchored.validate("prefix-User_01-suffix", report, "username",
                            PatternMatchMode::Partial) ==
        PLAINROOT_VALIDATION_OK);

  CompiledPattern digits;
  CHECK(CompiledPattern::compile(R"(\d+)", digits) == PLAINROOT_VALIDATION_OK);
  CHECK(digits.validate("room 42", report, "room", PatternMatchMode::Partial) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(digits.validate("room 42", report, "room", PatternMatchMode::Full) ==
        PLAINROOT_VALIDATION_OK);

  CHECK(CompiledPattern::compile("a(?=b)", pattern) ==
        PLAINROOT_VALIDATION_INVALID_RULE);
  CHECK(CompiledPattern::compile(R"((a)\1)", pattern) ==
        PLAINROOT_VALIDATION_INVALID_RULE);
  const char invalid_pattern[] = {static_cast<char>(0xff)};
  CHECK(CompiledPattern::compile(
            std::string_view(invalid_pattern, sizeof(invalid_pattern)),
            pattern) == PLAINROOT_VALIDATION_INVALID_UTF8);

  PatternOptions small_pattern;
  small_pattern.maximum_pattern_bytes = 2;
  CHECK(CompiledPattern::compile("abc", pattern, small_pattern) ==
        PLAINROOT_VALIDATION_LIMIT_EXCEEDED);
  const std::string above_default_pattern(
      PLAINROOT_VALIDATION_DEFAULT_MAXIMUM_PATTERN_BYTES + 1, 'a');
  CHECK(CompiledPattern::compile(above_default_pattern, pattern) ==
        PLAINROOT_VALIDATION_LIMIT_EXCEEDED);
  PatternOptions too_large;
  too_large.maximum_pattern_bytes =
      PLAINROOT_VALIDATION_MAXIMUM_PATTERN_BYTES + 1;
  CHECK(CompiledPattern::compile("a", pattern, too_large) ==
        PLAINROOT_VALIDATION_INVALID_ARGUMENT);

  CompiledPattern unicode;
  CHECK(CompiledPattern::compile("^🍺+$", unicode) == PLAINROOT_VALIDATION_OK);
  CHECK(unicode.validate("🍺🍺", report, "toast") == PLAINROOT_VALIDATION_OK);

  CompiledPattern bounded;
  CHECK(CompiledPattern::compile(R"(^(a+)+$)", bounded) ==
        PLAINROOT_VALIDATION_OK);
  const std::string near_match(32 * 1024, 'a');
  const std::string adversarial = near_match + '!';
  CHECK(bounded.validate(adversarial, report, "bounded") ==
        PLAINROOT_VALIDATION_OK);

  CHECK(report.seal() == PLAINROOT_VALIDATION_OK);
  std::size_t count = 0;
  CHECK(report.issue_count(count) == PLAINROOT_VALIDATION_OK);
  CHECK(count == 3);
  Issue issue;
  CHECK(report.issue(0, issue) == PLAINROOT_VALIDATION_OK);
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_PATTERN_MISMATCH);
  CHECK(issue.field == "username");
  CHECK(report.issue(1, issue) == PLAINROOT_VALIDATION_OK);
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_PATTERN_MISMATCH);
  CHECK(issue.field == "room");
  CHECK(report.issue(2, issue) == PLAINROOT_VALIDATION_OK);
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_PATTERN_MISMATCH);
  CHECK(issue.field == "bounded");

  CompiledPattern moved(std::move(unicode));
  CHECK(static_cast<bool>(moved));
  CHECK(!static_cast<bool>(unicode));
  return 0;
}

int main() {
  if (test_text_rules() != 0)
    return 1;
  if (test_patterns() != 0)
    return 1;
  return 0;
}
