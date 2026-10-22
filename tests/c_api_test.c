#include <plainroot/validation/validation.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #condition); \
      return 1;                                                                \
    }                                                                          \
  } while (0)

int main(void) {
  CHECK(plainroot_validation_abi_version_v1() == 1);
  CHECK(strcmp(plainroot_validation_result_name_v1((int32_t)99),
               "unknown-result") == 0);
  CHECK(strcmp(plainroot_validation_issue_code_name_v1((int32_t)99),
               "unknown-issue") == 0);
  CHECK(strcmp(plainroot_validation_issue_code_name_v1(
                   PLAINROOT_VALIDATION_ISSUE_OUT_OF_RANGE),
               "out-of-range") == 0);

  plainroot_validation_issue_v1 invalid_issue = {0};
  invalid_issue.struct_size = sizeof(invalid_issue);
  invalid_issue.code = (plainroot_validation_issue_code_v1)99;
  plainroot_validation_report *invalid_code_report = NULL;
  CHECK(plainroot_validation_report_create_v1(NULL, &invalid_code_report) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(plainroot_validation_report_add_issue_v1(
            invalid_code_report, &invalid_issue) ==
        PLAINROOT_VALIDATION_INVALID_ISSUE_CODE);
  plainroot_validation_report_release_v1(invalid_code_report);

  plainroot_validation_parameter_v1 invalid_parameter = {0};
  invalid_parameter.struct_size = sizeof(invalid_parameter);
  invalid_parameter.name = "unknown";
  invalid_parameter.name_size = sizeof("unknown") - 1;
  invalid_parameter.type = (plainroot_validation_parameter_type_v1)99;
  invalid_issue.code = PLAINROOT_VALIDATION_ISSUE_REQUIRED;
  invalid_issue.parameters = &invalid_parameter;
  invalid_issue.parameter_count = 1;
  CHECK(plainroot_validation_report_create_v1(NULL, &invalid_code_report) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(plainroot_validation_report_add_issue_v1(
            invalid_code_report, &invalid_issue) ==
        PLAINROOT_VALIDATION_INVALID_ARGUMENT);
  plainroot_validation_report_release_v1(invalid_code_report);

  plainroot_validation_report *report = NULL;
  CHECK(plainroot_validation_report_create_v1(NULL, &report) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(report != NULL);

  CHECK(plainroot_validation_validate_range_i64_v1(report, "age", 3, 11, 0,
                                                    10) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(plainroot_validation_validate_range_i64_v1(report, "age", 3, 4, 10,
                                                    0) ==
        PLAINROOT_VALIDATION_INVALID_RULE);
  CHECK(plainroot_validation_validate_range_u64_v1(report, "count", 5, 5, 0,
                                                    10) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(plainroot_validation_validate_range_u64_v1(report, "count", 5, 11, 0,
                                                    10) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(plainroot_validation_validate_range_f64_v1(
            report, "ratio", 5, 0.5, 0.0, 1.0) == PLAINROOT_VALIDATION_OK);
  CHECK(plainroot_validation_validate_range_f64_v1(
            report, "ratio", 5, 2.0, 0.0, 1.0) == PLAINROOT_VALIDATION_OK);
  CHECK(plainroot_validation_validate_range_f64_v1(
            report, "ratio", 5, NAN, 0.0, 1.0) == PLAINROOT_VALIDATION_OK);
  CHECK(plainroot_validation_validate_finite_f64_v1(report, "ratio", 5, 0.5) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(plainroot_validation_validate_finite_f64_v1(report, "ratio", 5, INFINITY) ==
        PLAINROOT_VALIDATION_OK);

  plainroot_validation_parse_i64_result_v1 parsed_i64 = {0};
  parsed_i64.struct_size = sizeof(parsed_i64);
  CHECK(plainroot_validation_parse_i64_text_v1("+42", 3, NULL, &parsed_i64) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(parsed_i64.parsed && parsed_i64.value == 42);
  CHECK(parsed_i64.failure_code == PLAINROOT_VALIDATION_ISSUE_NONE);

  plainroot_validation_parse_u64_result_v1 parsed_u64 = {0};
  parsed_u64.struct_size = sizeof(parsed_u64);
  CHECK(plainroot_validation_parse_u64_text_v1("-1", 2, NULL, &parsed_u64) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(!parsed_u64.parsed && parsed_u64.value == 0);
  CHECK(parsed_u64.failure_code ==
        PLAINROOT_VALIDATION_ISSUE_INVALID_UNSIGNED_INTEGER);

  plainroot_validation_parse_f64_result_v1 parsed_f64 = {0};
  parsed_f64.struct_size = sizeof(parsed_f64);
  CHECK(plainroot_validation_parse_f64_text_v1("-.5e2", 5, NULL,
                                                &parsed_f64) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(parsed_f64.parsed && parsed_f64.value == -50.0);

  plainroot_validation_parse_bool_result_v1 parsed_bool = {0};
  parsed_bool.struct_size = sizeof(parsed_bool);
  CHECK(plainroot_validation_parse_bool_text_v1("TRUE", 4, NULL,
                                                 &parsed_bool) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(!parsed_bool.parsed);
  CHECK(parsed_bool.failure_code ==
        PLAINROOT_VALIDATION_ISSUE_INVALID_BOOLEAN);

  plainroot_validation_parse_options_v1 options = {0};
  options.struct_size = sizeof(options);
  options.boolean_case_insensitive_ascii = 1;
  options.boolean_accept_yes_no = 1;
  options.boolean_accept_0_1 = 1;
  CHECK(plainroot_validation_parse_bool_text_v1("YES", 3, &options,
                                                 &parsed_bool) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(parsed_bool.parsed && parsed_bool.value == 1);

  plainroot_validation_parse_options_v1 parse_options = {0};
  parse_options.struct_size = sizeof(parse_options);
  parse_options.trim_ascii_whitespace = 1;
  CHECK(plainroot_validation_parse_i64_text_v1(" \t42\r\n", 6,
                                                &parse_options,
                                                &parsed_i64) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(parsed_i64.parsed && parsed_i64.value == 42);
  parse_options.maximum_input_bytes = 3;
  CHECK(plainroot_validation_parse_i64_text_v1("1234", 4, &parse_options,
                                                &parsed_i64) ==
        PLAINROOT_VALIDATION_LIMIT_EXCEEDED);
  CHECK(!parsed_i64.parsed && parsed_i64.value == 0);
  parse_options.maximum_input_bytes = 0;
  parse_options.trim_ascii_whitespace = 2;
  CHECK(plainroot_validation_parse_i64_text_v1("1", 1, &parse_options,
                                                &parsed_i64) ==
        PLAINROOT_VALIDATION_INVALID_ARGUMENT);

  CHECK(plainroot_validation_report_seal_v1(report) == PLAINROOT_VALIDATION_OK);
  uint8_t is_valid = 1;
  CHECK(plainroot_validation_report_is_valid_v1(report, &is_valid) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(!is_valid);
  size_t issue_count = 0;
  CHECK(plainroot_validation_report_issue_count_v1(report, &issue_count) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(issue_count == 5);

  plainroot_validation_issue_view_v1 issue = {0};
  issue.struct_size = sizeof(issue);
  CHECK(plainroot_validation_report_get_issue_v1(report, 0, &issue) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_OUT_OF_RANGE);
  CHECK(issue.field_size == 3 && memcmp(issue.field, "age", 3) == 0);
  CHECK(issue.parameter_count == 2);

  plainroot_validation_parameter_view_v1 parameter = {0};
  parameter.struct_size = sizeof(parameter);
  CHECK(plainroot_validation_report_get_parameter_v1(report, 0, 0,
                                                      &parameter) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(parameter.type == PLAINROOT_VALIDATION_PARAMETER_I64);
  CHECK(parameter.name_size == 7 && memcmp(parameter.name, "minimum", 7) == 0);
  CHECK(parameter.as.i64 == 0);

  issue = (plainroot_validation_issue_view_v1){0};
  issue.struct_size = sizeof(issue);
  CHECK(plainroot_validation_report_get_issue_v1(report, 1, &issue) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_OUT_OF_RANGE);
  parameter = (plainroot_validation_parameter_view_v1){0};
  parameter.struct_size = sizeof(parameter);
  CHECK(plainroot_validation_report_get_parameter_v1(report, 1, 1,
                                                      &parameter) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(parameter.type == PLAINROOT_VALIDATION_PARAMETER_U64);
  CHECK(parameter.as.u64 == 10);

  issue = (plainroot_validation_issue_view_v1){0};
  issue.struct_size = sizeof(issue);
  CHECK(plainroot_validation_report_get_issue_v1(report, 2, &issue) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_OUT_OF_RANGE);
  parameter = (plainroot_validation_parameter_view_v1){0};
  parameter.struct_size = sizeof(parameter);
  CHECK(plainroot_validation_report_get_parameter_v1(report, 2, 0,
                                                      &parameter) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(parameter.type == PLAINROOT_VALIDATION_PARAMETER_F64);
  CHECK(parameter.as.f64 == 0.0);

  issue = (plainroot_validation_issue_view_v1){0};
  issue.struct_size = sizeof(issue);
  CHECK(plainroot_validation_report_get_issue_v1(report, 3, &issue) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_NON_FINITE_NUMBER);
  issue = (plainroot_validation_issue_view_v1){0};
  issue.struct_size = sizeof(issue);
  CHECK(plainroot_validation_report_get_issue_v1(report, 4, &issue) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_NON_FINITE_NUMBER);

  CHECK(plainroot_validation_report_retain_v1(report) ==
        PLAINROOT_VALIDATION_OK);
  plainroot_validation_report_release_v1(report);
  issue_count = 0;
  CHECK(plainroot_validation_report_issue_count_v1(report, &issue_count) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(issue_count == 5);
  plainroot_validation_report_release_v1(report);

  plainroot_validation_report_options_v1 limited_options = {0};
  limited_options.struct_size = sizeof(limited_options);
  limited_options.maximum_issues = 1;
  plainroot_validation_report *limited_report = NULL;
  CHECK(plainroot_validation_report_create_v1(&limited_options,
                                               &limited_report) ==
        PLAINROOT_VALIDATION_OK);
  plainroot_validation_issue_v1 required_issue = {0};
  required_issue.struct_size = sizeof(required_issue);
  required_issue.code = PLAINROOT_VALIDATION_ISSUE_REQUIRED;
  CHECK(plainroot_validation_report_add_issue_v1(limited_report,
                                                 &required_issue) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(plainroot_validation_report_add_issue_v1(limited_report,
                                                 &required_issue) ==
        PLAINROOT_VALIDATION_LIMIT_EXCEEDED);
  CHECK(plainroot_validation_report_seal_v1(limited_report) ==
        PLAINROOT_VALIDATION_OK);
  issue_count = 0;
  CHECK(plainroot_validation_report_issue_count_v1(limited_report,
                                                   &issue_count) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(issue_count == 1);
  plainroot_validation_report_release_v1(limited_report);
  return 0;
}
