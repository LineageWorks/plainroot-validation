#include <plainroot/validation/validation.h>

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  const char *text = size ? reinterpret_cast<const char *>(data) : "";
  const uint8_t seed = size ? data[0] : 0;
  plainroot_validation_parse_options_v1 options{};
  options.struct_size = sizeof(options);
  options.maximum_input_bytes = 4096;
  options.trim_ascii_whitespace = static_cast<uint8_t>(seed & 1u);
  options.boolean_case_insensitive_ascii = static_cast<uint8_t>((seed >> 1) & 1u);
  options.boolean_accept_yes_no = static_cast<uint8_t>((seed >> 2) & 1u);
  options.boolean_accept_0_1 = static_cast<uint8_t>((seed >> 3) & 1u);

  plainroot_validation_parse_i64_result_v1 signed_result{};
  signed_result.struct_size = sizeof(signed_result);
  plainroot_validation_parse_u64_result_v1 unsigned_result{};
  unsigned_result.struct_size = sizeof(unsigned_result);
  plainroot_validation_parse_f64_result_v1 real_result{};
  real_result.struct_size = sizeof(real_result);
  plainroot_validation_parse_bool_result_v1 boolean_result{};
  boolean_result.struct_size = sizeof(boolean_result);

  (void)plainroot_validation_parse_i64_text_v1(
      text, size, &options, &signed_result);
  (void)plainroot_validation_parse_u64_text_v1(
      text, size, &options, &unsigned_result);
  (void)plainroot_validation_parse_f64_text_v1(
      text, size, &options, &real_result);
  (void)plainroot_validation_parse_bool_text_v1(
      text, size, &options, &boolean_result);
  return 0;
}
