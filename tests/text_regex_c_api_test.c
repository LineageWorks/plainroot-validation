#include <plainroot/validation/validation.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #condition);  \
      return 1;                                                                \
    }                                                                          \
  } while (0)

static int get_issue(plainroot_validation_report *report, size_t index,
                     plainroot_validation_issue_view_v1 *issue) {
  memset(issue, 0, sizeof(*issue));
  issue->struct_size = sizeof(*issue);
  return plainroot_validation_report_get_issue_v1(report, index, issue) ==
         PLAINROOT_VALIDATION_OK;
}

static int get_parameter(plainroot_validation_report *report,
                         size_t issue_index, size_t parameter_index,
                         plainroot_validation_parameter_view_v1 *parameter) {
  memset(parameter, 0, sizeof(*parameter));
  parameter->struct_size = sizeof(*parameter);
  return plainroot_validation_report_get_parameter_v1(
             report, issue_index, parameter_index, parameter) ==
         PLAINROOT_VALIDATION_OK;
}

int main(void) {
  CHECK(strcmp(plainroot_validation_issue_code_name_v1(
                   PLAINROOT_VALIDATION_ISSUE_BLANK),
               "blank") == 0);
  CHECK(plainroot_validation_validate_text_length_v1(
            NULL, "field", 5, "x", 1, 2, 1, NULL) ==
        PLAINROOT_VALIDATION_INVALID_ARGUMENT);

  plainroot_validation_report *report = NULL;
  CHECK(plainroot_validation_report_create_v1(NULL, &report) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(plainroot_validation_validate_required_text_v1(
            report, "name", 4, "", 0, NULL) == PLAINROOT_VALIDATION_OK);
  CHECK(plainroot_validation_validate_nonblank_text_v1(
            report, "name", 4, "\xc2\xa0\xe3\x80\x80", 5, NULL) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(plainroot_validation_validate_nonblank_text_v1(
            report, "name", 4, "", 0, NULL) == PLAINROOT_VALIDATION_OK);
  CHECK(plainroot_validation_validate_nonblank_text_v1(
            report, "name", 4, "\xe2\x80\x8b", 3, NULL) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(plainroot_validation_validate_text_length_v1(report, "label", 5, "é🙂",
                                                     6, 2, 2, NULL) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(plainroot_validation_validate_text_length_v1(
            report, "short", 5, "x", 1, 2, 4, NULL) == PLAINROOT_VALIDATION_OK);
  CHECK(plainroot_validation_validate_text_length_v1(report, "bad-rule", 8, "x",
                                                     1, 5, 4, NULL) ==
        PLAINROOT_VALIDATION_INVALID_RULE);
  CHECK(plainroot_validation_validate_text_max_bytes_v1(
            report, "bytes", 5, "é", 2, 1, NULL) == PLAINROOT_VALIDATION_OK);
  CHECK(plainroot_validation_validate_email_text_v1(
            report, "email", 5, "a..b@example.com", 16, NULL) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(plainroot_validation_validate_email_text_v1(
            report, "email", 5, "person@example.com", 18, NULL) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(plainroot_validation_validate_email_text_v1(
            report, "email", 5, "person@localhost", 16, NULL) ==
        PLAINROOT_VALIDATION_OK);

  plainroot_validation_text_options_v1 text_options = {0};
  text_options.struct_size = sizeof(text_options);
  text_options.maximum_input_bytes = 3;
  CHECK(plainroot_validation_validate_required_text_v1(
            report, "limit", 5, "four", 4, &text_options) ==
        PLAINROOT_VALIDATION_LIMIT_EXCEEDED);

  const char malformed[] = {(char)0xc3, '('};
  CHECK(plainroot_validation_validate_nonblank_text_v1(
            report, "utf8", 4, malformed, sizeof(malformed), NULL) ==
        PLAINROOT_VALIDATION_OK);

  CHECK(plainroot_validation_report_seal_v1(report) == PLAINROOT_VALIDATION_OK);
  size_t issue_count = 0;
  CHECK(plainroot_validation_report_issue_count_v1(report, &issue_count) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(issue_count == 8);
  plainroot_validation_issue_view_v1 issue;
  CHECK(get_issue(report, 0, &issue));
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_REQUIRED);
  CHECK(get_issue(report, 1, &issue));
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_BLANK);
  CHECK(get_issue(report, 2, &issue));
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_BLANK);
  CHECK(get_issue(report, 3, &issue));
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_TOO_SHORT);
  plainroot_validation_parameter_view_v1 parameter;
  CHECK(get_parameter(report, 3, 0, &parameter));
  CHECK(parameter.type == PLAINROOT_VALIDATION_PARAMETER_U64);
  CHECK(parameter.as.u64 == 2);
  CHECK(get_issue(report, 4, &issue));
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_TOO_LONG);
  CHECK(get_parameter(report, 4, 0, &parameter));
  CHECK(parameter.as.u64 == 1);
  CHECK(get_issue(report, 5, &issue));
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_INVALID_EMAIL);
  CHECK(get_issue(report, 6, &issue));
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_INVALID_EMAIL);
  CHECK(get_issue(report, 7, &issue));
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_INVALID_UTF8);
  CHECK(get_parameter(report, 7, 0, &parameter));
  CHECK(strcmp(parameter.name, "offset") == 0);
  CHECK(parameter.as.u64 == 0);
  plainroot_validation_report_release_v1(report);

  plainroot_validation_pattern *pattern = NULL;
  plainroot_validation_pattern *unanchored_pattern = NULL;
  static const char pattern_source[] = "^[A-Za-z][A-Za-z0-9_]{1,15}$";
  CHECK(plainroot_validation_pattern_compile_v1(
            pattern_source, sizeof(pattern_source) - 1, NULL, &pattern) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(pattern != NULL);
  static const char partial_pattern_source[] = "[A-Za-z][A-Za-z0-9_]{1,15}";
  CHECK(plainroot_validation_pattern_compile_v1(
            partial_pattern_source, sizeof(partial_pattern_source) - 1, NULL,
            &unanchored_pattern) == PLAINROOT_VALIDATION_OK);

  CHECK(plainroot_validation_report_create_v1(NULL, &report) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(plainroot_validation_validate_pattern_v1(
            report, "username", 8, "user_1", 6, NULL, pattern,
            PLAINROOT_VALIDATION_PATTERN_FULL) == PLAINROOT_VALIDATION_OK);
  CHECK(plainroot_validation_validate_pattern_v1(
            report, "username", 8, "1bad", 4, NULL, pattern,
            PLAINROOT_VALIDATION_PATTERN_FULL) == PLAINROOT_VALIDATION_OK);
  CHECK(plainroot_validation_validate_pattern_v1(
            report, "username", 8, "prefix-user_1-suffix", 20, NULL,
            unanchored_pattern,
            PLAINROOT_VALIDATION_PATTERN_PARTIAL) == PLAINROOT_VALIDATION_OK);
  CHECK(plainroot_validation_validate_pattern_v1(
            report, "username", 8, "user_1", 6, NULL, pattern,
            (plainroot_validation_pattern_match_mode_v1)7) ==
        PLAINROOT_VALIDATION_INVALID_ARGUMENT);
  CHECK(plainroot_validation_report_seal_v1(report) == PLAINROOT_VALIDATION_OK);
  CHECK(plainroot_validation_report_issue_count_v1(report, &issue_count) ==
        PLAINROOT_VALIDATION_OK);
  CHECK(issue_count == 1);
  CHECK(get_issue(report, 0, &issue));
  CHECK(issue.code == PLAINROOT_VALIDATION_ISSUE_PATTERN_MISMATCH);
  plainroot_validation_report_release_v1(report);
  plainroot_validation_pattern_release_v1(pattern);
  plainroot_validation_pattern_release_v1(unanchored_pattern);

  CHECK(plainroot_validation_pattern_compile_v1("a(?=b)", 6, NULL, &pattern) ==
        PLAINROOT_VALIDATION_INVALID_RULE);
  CHECK(pattern == NULL);
  CHECK(plainroot_validation_pattern_compile_v1(NULL, 1, NULL, &pattern) ==
        PLAINROOT_VALIDATION_INVALID_ARGUMENT);
  CHECK(plainroot_validation_pattern_compile_v1("a", 1, NULL, NULL) ==
        PLAINROOT_VALIDATION_INVALID_ARGUMENT);
  const char invalid_pattern[] = {(char)0xff};
  CHECK(plainroot_validation_pattern_compile_v1(
            invalid_pattern, sizeof(invalid_pattern), NULL, &pattern) ==
        PLAINROOT_VALIDATION_INVALID_UTF8);

  plainroot_validation_pattern_options_v1 pattern_options = {0};
  pattern_options.struct_size = sizeof(pattern_options);
  pattern_options.maximum_pattern_bytes = 2;
  CHECK(plainroot_validation_pattern_compile_v1("abc", 3, &pattern_options,
                                                &pattern) ==
        PLAINROOT_VALIDATION_LIMIT_EXCEEDED);
  pattern_options.maximum_pattern_bytes =
      PLAINROOT_VALIDATION_MAXIMUM_PATTERN_BYTES + 1;
  CHECK(plainroot_validation_pattern_compile_v1("a", 1, &pattern_options,
                                                &pattern) ==
        PLAINROOT_VALIDATION_INVALID_ARGUMENT);
  return 0;
}
