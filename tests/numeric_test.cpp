#include <plainroot/validation/validation.hpp>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string_view>
#include <type_traits>

using namespace plainroot::validation;

static_assert(detail::is_supported_integer_v<int8_t>);
static_assert(detail::is_supported_integer_v<uint64_t>);
static_assert(!detail::is_supported_integer_v<bool>);
static_assert(!detail::is_supported_integer_v<char>);
static_assert(detail::is_supported_real_v<float>);
static_assert(detail::is_supported_real_v<double>);
static_assert(!detail::is_supported_real_v<long double>);

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      std::cerr << "check failed at line " << __LINE__ << ": " #condition     \
                << '\n';                                                       \
      return 1;                                                                \
    }                                                                          \
  } while (false)

int test_ranges() {
  Report report;
  CHECK(Report::create(report) == PLAINROOT_VALIDATION_OK);

  const RangeValidator<int> range{0, 10};
  CHECK(range.validate(0, report, "age") == PLAINROOT_VALIDATION_OK);
  CHECK(range.validate(10, report, "age") == PLAINROOT_VALIDATION_OK);
  CHECK(range.validate(11, report, "age") == PLAINROOT_VALIDATION_OK);
  CHECK(range.validate(-1, report, "age") == PLAINROOT_VALIDATION_OK);

  const RangeValidator<int> inverted{10, 0};
  CHECK(inverted.validate(5, report, "age") ==
        PLAINROOT_VALIDATION_INVALID_RULE);

  const RangeValidator<uint64_t> unsigned_range{0, UINT64_MAX};
  CHECK(unsigned_range.validate(UINT64_MAX, report, "count") ==
        PLAINROOT_VALIDATION_OK);

  const RangeValidator<double> real_range{-1.0, 1.0};
  CHECK(real_range.validate(0.25, report, "ratio") ==
        PLAINROOT_VALIDATION_OK);
  CHECK(real_range.validate(2.0, report, "ratio") ==
        PLAINROOT_VALIDATION_OK);
  CHECK(real_range.validate(std::numeric_limits<double>::infinity(), report,
                            "ratio") == PLAINROOT_VALIDATION_OK);
  CHECK((RangeValidator<double>{std::numeric_limits<double>::quiet_NaN(), 1.0})
            .validate(0.0, report, "ratio") ==
        PLAINROOT_VALIDATION_INVALID_RULE);

  CHECK((RangeValidator<int8_t>{-2, 2}).validate(int8_t{2}, report) ==
        PLAINROOT_VALIDATION_OK);
  CHECK((FiniteNumberValidator<float>{}).validate(0.5f, report) ==
        PLAINROOT_VALIDATION_OK);
  CHECK((FiniteNumberValidator<float>{})
            .validate(std::numeric_limits<float>::quiet_NaN(), report) ==
        PLAINROOT_VALIDATION_OK);

  CHECK(report.seal() == PLAINROOT_VALIDATION_OK);
  bool is_valid = true;
  CHECK(report.is_valid(is_valid) == PLAINROOT_VALIDATION_OK);
  CHECK(!is_valid);
  std::size_t count = 0;
  CHECK(report.issue_count(count) == PLAINROOT_VALIDATION_OK);
  CHECK(count == 5);

  Issue issue;
  CHECK(report.issue(0, issue) == PLAINROOT_VALIDATION_OK);
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_OUT_OF_RANGE);
  CHECK(issue.field == "age");
  CHECK(issue.parameters.size() == 2);
  CHECK(issue.parameters[0].name == "minimum");
  CHECK(std::get<int64_t>(issue.parameters[0].value) == 0);
  CHECK(std::get<int64_t>(issue.parameters[1].value) == 10);

  CHECK(report.issue(3, issue) == PLAINROOT_VALIDATION_OK);
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_NON_FINITE_NUMBER);
  CHECK(issue.parameters.empty());
  CHECK(range.validate(5, report) == PLAINROOT_VALIDATION_SEALED);
  return 0;
}

int test_parsing() {
  auto signed_value = parse_integer<int>("+42");
  CHECK(signed_value.result == PLAINROOT_VALIDATION_OK);
  CHECK(signed_value.parsed && signed_value.value == 42);
  CHECK(signed_value.failure_code == PLAINROOT_VALIDATION_ISSUE_NONE);

  auto negative = parse_integer<int64_t>("-9223372036854775808");
  CHECK(negative.parsed);
  CHECK(negative.value == std::numeric_limits<int64_t>::min());

  auto syntax_error = parse_integer<int>("12tail");
  CHECK(syntax_error.result == PLAINROOT_VALIDATION_OK);
  CHECK(!syntax_error.parsed && syntax_error.value == 0);
  CHECK(syntax_error.failure_code ==
        PLAINROOT_VALIDATION_ISSUE_INVALID_INTEGER);

  auto signed_overflow = parse_integer<int64_t>("9223372036854775808");
  CHECK(!signed_overflow.parsed);
  CHECK(signed_overflow.failure_code ==
        PLAINROOT_VALIDATION_ISSUE_OUT_OF_RANGE);

  auto narrow_overflow = parse_integer<int8_t>("128");
  CHECK(!narrow_overflow.parsed);
  CHECK(narrow_overflow.failure_code ==
        PLAINROOT_VALIDATION_ISSUE_OUT_OF_RANGE);

  auto unsigned_value = parse_integer<uint64_t>("+18446744073709551615");
  CHECK(unsigned_value.parsed);
  CHECK(unsigned_value.value == std::numeric_limits<uint64_t>::max());
  auto negative_unsigned = parse_integer<unsigned>("-0");
  CHECK(!negative_unsigned.parsed);
  CHECK(negative_unsigned.failure_code ==
        PLAINROOT_VALIDATION_ISSUE_INVALID_UNSIGNED_INTEGER);

  auto whitespace = parse_integer<int>(" 12 ");
  CHECK(!whitespace.parsed);
  CHECK(whitespace.failure_code ==
        PLAINROOT_VALIDATION_ISSUE_INVALID_INTEGER);
  ParseOptions trim;
  trim.trim_ascii_whitespace = true;
  auto trimmed = parse_integer<int>(" \t12\r\n", trim);
  CHECK(trimmed.parsed && trimmed.value == 12);

  const char invalid_utf8_bytes[] = {static_cast<char>(0xc3), '('};
  auto invalid_utf8 = parse_integer<int>(
      std::string_view(invalid_utf8_bytes, sizeof(invalid_utf8_bytes)));
  CHECK(!invalid_utf8.parsed);
  CHECK(invalid_utf8.failure_code == PLAINROOT_VALIDATION_ISSUE_INVALID_UTF8);

  auto arabic_digits = parse_integer<int>("١٢");
  CHECK(!arabic_digits.parsed);
  CHECK(arabic_digits.failure_code ==
        PLAINROOT_VALIDATION_ISSUE_INVALID_INTEGER);

  ParseOptions limited;
  limited.maximum_input_bytes = 3;
  auto too_long = parse_integer<int>("1234", limited);
  CHECK(too_long.result == PLAINROOT_VALIDATION_LIMIT_EXCEEDED);
  CHECK(!too_long.parsed);
  CHECK(too_long.failure_code == PLAINROOT_VALIDATION_ISSUE_NONE);

  auto half = parse_real<float>(".5");
  CHECK(half.parsed && half.value == 0.5f);
  auto exponent = parse_real<double>("1.e+2");
  CHECK(exponent.parsed && exponent.value == 100.0);
  auto negative_real = parse_real<double>("-.5e2");
  CHECK(negative_real.parsed && negative_real.value == -50.0);

  for (const std::string_view bad_real : {"nan", "Infinity", "0x1p2", "1,5",
                                          "1e", ".", "--1", "1x"}) {
    const auto parsed = parse_real<double>(bad_real);
    CHECK(!parsed.parsed);
    CHECK(parsed.failure_code == PLAINROOT_VALIDATION_ISSUE_INVALID_REAL);
  }
  for (const std::string_view out_of_range : {"1e309", "1e-999"}) {
    const auto parsed = parse_real<double>(out_of_range);
    CHECK(!parsed.parsed);
    CHECK(parsed.failure_code == PLAINROOT_VALIDATION_ISSUE_OUT_OF_RANGE);
  }
  auto float_underflow = parse_real<float>("1e-50");
  CHECK(!float_underflow.parsed);
  CHECK(float_underflow.failure_code ==
        PLAINROOT_VALIDATION_ISSUE_OUT_OF_RANGE);
  auto float_subnormal = parse_real<float>("1.40129846e-45");
  CHECK(float_subnormal.parsed && float_subnormal.value > 0.0f);

  auto boolean = parse_boolean("true");
  CHECK(boolean.parsed && boolean.value);
  auto false_boolean = parse_boolean("false");
  CHECK(false_boolean.parsed && !false_boolean.value);
  auto uppercase = parse_boolean("TRUE");
  CHECK(!uppercase.parsed);
  CHECK(uppercase.failure_code == PLAINROOT_VALIDATION_ISSUE_INVALID_BOOLEAN);

  ParseOptions boolean_options;
  boolean_options.boolean_case_insensitive_ascii = true;
  boolean_options.boolean_accept_yes_no = true;
  boolean_options.boolean_accept_0_1 = true;
  const auto mixed_case_true = parse_boolean("TrUe", boolean_options);
  CHECK(mixed_case_true.parsed && mixed_case_true.value);
  const auto yes_boolean = parse_boolean("yes", boolean_options);
  CHECK(yes_boolean.parsed && yes_boolean.value);
  const auto no_boolean = parse_boolean("NO", boolean_options);
  CHECK(no_boolean.parsed && !no_boolean.value);
  const auto one_boolean = parse_boolean("1", boolean_options);
  CHECK(one_boolean.parsed && one_boolean.value);
  const auto zero_boolean = parse_boolean("0", boolean_options);
  CHECK(zero_boolean.parsed && !zero_boolean.value);
  return 0;
}

int main() {
  if (test_ranges() != 0)
    return 1;
  if (test_parsing() != 0)
    return 1;
  return 0;
}
