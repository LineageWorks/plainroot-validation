#include <plainroot/validation/validation.h>

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  const size_t split = size / 2;
  const char *bytes = size ? reinterpret_cast<const char *>(data) : "";
  const char *pattern_text = bytes;
  const char *input_text = bytes + split;
  const size_t pattern_size = split;
  const size_t input_size = size - split;

  plainroot_validation_text_options_v1 text_options{};
  text_options.struct_size = sizeof(text_options);
  text_options.maximum_input_bytes = 4096;

  plainroot_validation_report_options_v1 report_options{};
  report_options.struct_size = sizeof(report_options);
  report_options.maximum_issues = 8;
  plainroot_validation_report *report = nullptr;
  if (plainroot_validation_report_create_v1(&report_options, &report) !=
      PLAINROOT_VALIDATION_OK) {
    return 0;
  }

  (void)plainroot_validation_validate_required_text_v1(
      report, "fuzz", 4, input_text, input_size, &text_options);
  (void)plainroot_validation_validate_nonblank_text_v1(
      report, "fuzz", 4, input_text, input_size, &text_options);
  (void)plainroot_validation_validate_text_length_v1(
      report, "fuzz", 4, input_text, input_size, 0, 1024, &text_options);
  (void)plainroot_validation_validate_text_max_bytes_v1(
      report, "fuzz", 4, input_text, input_size, 1024, &text_options);
  (void)plainroot_validation_validate_email_text_v1(
      report, "fuzz", 4, input_text, input_size, &text_options);

  plainroot_validation_pattern_options_v1 pattern_options{};
  pattern_options.struct_size = sizeof(pattern_options);
  pattern_options.maximum_pattern_bytes = 2048;
  plainroot_validation_pattern *pattern = nullptr;
  if (plainroot_validation_pattern_compile_v1(
          pattern_text, pattern_size, &pattern_options, &pattern) ==
      PLAINROOT_VALIDATION_OK) {
    (void)plainroot_validation_validate_pattern_v1(
        report, "fuzz", 4, input_text, input_size, &text_options, pattern,
        PLAINROOT_VALIDATION_PATTERN_FULL);
    (void)plainroot_validation_validate_pattern_v1(
        report, "fuzz", 4, input_text, input_size, &text_options, pattern,
        PLAINROOT_VALIDATION_PATTERN_PARTIAL);
    plainroot_validation_pattern_release_v1(pattern);
  }

  plainroot_validation_report_release_v1(report);
  return 0;
}
