#include <plainroot/validation/validation.h>

int main(void) {
  plainroot_validation_parse_i64_result_v1 parsed = {0};
  parsed.struct_size = sizeof(parsed);
  if (plainroot_validation_parse_i64_text_v1("42", 2, NULL, &parsed) !=
          PLAINROOT_VALIDATION_OK ||
      !parsed.parsed || parsed.value != 42) {
    return 1;
  }

  plainroot_validation_report *report = NULL;
  if (plainroot_validation_report_create_v1(NULL, &report) !=
      PLAINROOT_VALIDATION_OK) {
    return 2;
  }
  if (plainroot_validation_validate_range_i64_v1(
          report, "profile.age", sizeof("profile.age") - 1, parsed.value,
          0, 130) != PLAINROOT_VALIDATION_OK) {
    plainroot_validation_report_release_v1(report);
    return 3;
  }
  if (plainroot_validation_report_seal_v1(report) !=
      PLAINROOT_VALIDATION_OK) {
    plainroot_validation_report_release_v1(report);
    return 4;
  }

  uint8_t valid = 0;
  const plainroot_validation_result_v1 status =
      plainroot_validation_report_is_valid_v1(report, &valid);
  plainroot_validation_report_release_v1(report);
  return status == PLAINROOT_VALIDATION_OK && valid ? 0 : 5;
}
