#ifndef PLAINROOT_VALIDATION_VALIDATION_H
#define PLAINROOT_VALIDATION_VALIDATION_H

#include <plainroot/validation/export.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PLAINROOT_VALIDATION_DEFAULT_MAXIMUM_ISSUES 64u
#define PLAINROOT_VALIDATION_MAXIMUM_ISSUES 256u
#define PLAINROOT_VALIDATION_DEFAULT_MAXIMUM_PARAMETERS_PER_ISSUE 8u
#define PLAINROOT_VALIDATION_MAXIMUM_PARAMETERS_PER_ISSUE 16u
#define PLAINROOT_VALIDATION_DEFAULT_MAXIMUM_TEXT_BYTES (1024u * 1024u)
#define PLAINROOT_VALIDATION_MAXIMUM_TEXT_BYTES (16u * 1024u * 1024u)
#define PLAINROOT_VALIDATION_DEFAULT_MAXIMUM_INPUT_BYTES (64u * 1024u)
#define PLAINROOT_VALIDATION_MAXIMUM_INPUT_BYTES (1024u * 1024u)
#define PLAINROOT_VALIDATION_DEFAULT_MAXIMUM_PATTERN_BYTES (4u * 1024u)
#define PLAINROOT_VALIDATION_MAXIMUM_PATTERN_BYTES (64u * 1024u)
#define PLAINROOT_VALIDATION_REGEX_MAXIMUM_MEMORY_BYTES (8u * 1024u * 1024u)

#ifdef __cplusplus
#define PLAINROOT_VALIDATION_ENUM(name) enum name : int32_t
#else
#define PLAINROOT_VALIDATION_ENUM(name) enum name
#endif

typedef PLAINROOT_VALIDATION_ENUM(plainroot_validation_result_v1) {
  PLAINROOT_VALIDATION_OK = 0,
  PLAINROOT_VALIDATION_INVALID_ARGUMENT = 1,
  PLAINROOT_VALIDATION_OUT_OF_MEMORY = 2,
  PLAINROOT_VALIDATION_INTERNAL_ERROR = 3,
  PLAINROOT_VALIDATION_INVALID_UTF8 = 4,
  PLAINROOT_VALIDATION_NON_FINITE_NUMBER = 5,
  PLAINROOT_VALIDATION_LIMIT_EXCEEDED = 6,
  PLAINROOT_VALIDATION_SEALED = 7,
  PLAINROOT_VALIDATION_INDEX_OUT_OF_RANGE = 8,
  PLAINROOT_VALIDATION_INVALID_ISSUE_CODE = 9,
  PLAINROOT_VALIDATION_INVALID_RULE = 10,
  PLAINROOT_VALIDATION_NOT_SEALED = 11,
} plainroot_validation_result_v1;

typedef PLAINROOT_VALIDATION_ENUM(plainroot_validation_issue_code_v1) {
  PLAINROOT_VALIDATION_ISSUE_NONE = 0,
  PLAINROOT_VALIDATION_ISSUE_REQUIRED = 1,
  PLAINROOT_VALIDATION_ISSUE_INVALID_UTF8 = 2,
  PLAINROOT_VALIDATION_ISSUE_OUT_OF_RANGE = 3,
  PLAINROOT_VALIDATION_ISSUE_INVALID_INTEGER = 4,
  PLAINROOT_VALIDATION_ISSUE_INVALID_UNSIGNED_INTEGER = 5,
  PLAINROOT_VALIDATION_ISSUE_INVALID_REAL = 6,
  PLAINROOT_VALIDATION_ISSUE_INVALID_BOOLEAN = 7,
  PLAINROOT_VALIDATION_ISSUE_TOO_SHORT = 8,
  PLAINROOT_VALIDATION_ISSUE_TOO_LONG = 9,
  PLAINROOT_VALIDATION_ISSUE_PATTERN_MISMATCH = 10,
  PLAINROOT_VALIDATION_ISSUE_INVALID_EMAIL = 11,
  PLAINROOT_VALIDATION_ISSUE_NON_FINITE_NUMBER = 12,
  PLAINROOT_VALIDATION_ISSUE_BLANK = 13,
} plainroot_validation_issue_code_v1;

typedef PLAINROOT_VALIDATION_ENUM(plainroot_validation_parameter_type_v1) {
  PLAINROOT_VALIDATION_PARAMETER_BOOL = 1,
  PLAINROOT_VALIDATION_PARAMETER_I64 = 2,
  PLAINROOT_VALIDATION_PARAMETER_U64 = 3,
  PLAINROOT_VALIDATION_PARAMETER_F64 = 4,
  PLAINROOT_VALIDATION_PARAMETER_TEXT = 5,
} plainroot_validation_parameter_type_v1;

typedef struct plainroot_validation_parameter_v1 {
  uint32_t struct_size;
  const char *name;
  size_t name_size;
  plainroot_validation_parameter_type_v1 type;
  union {
    uint8_t boolean;
    int64_t i64;
    uint64_t u64;
    double f64;
    struct {
      const char *data;
      size_t size;
    } text;
  } as;
} plainroot_validation_parameter_v1;

typedef struct plainroot_validation_issue_v1 {
  uint32_t struct_size;
  plainroot_validation_issue_code_v1 code;
  /* Optional context, UTF-8 without U+0000. The report copies the bytes. */
  const char *field;
  size_t field_size;
  const plainroot_validation_parameter_v1 *parameters;
  size_t parameter_count;
} plainroot_validation_issue_v1;

typedef struct plainroot_validation_report_options_v1 {
  uint32_t struct_size;
  /* Zero selects the documented default. */
  uint32_t maximum_issues;
  uint32_t maximum_parameters_per_issue;
  uint64_t maximum_text_bytes;
} plainroot_validation_report_options_v1;

typedef struct plainroot_validation_parameter_view_v1 {
  uint32_t struct_size;
  plainroot_validation_parameter_type_v1 type;
  const char *name;
  size_t name_size;
  union {
    uint8_t boolean;
    int64_t i64;
    uint64_t u64;
    double f64;
    struct {
      const char *data;
      size_t size;
    } text;
  } as;
} plainroot_validation_parameter_view_v1;

typedef struct plainroot_validation_issue_view_v1 {
  uint32_t struct_size;
  plainroot_validation_issue_code_v1 code;
  const char *field;
  size_t field_size;
  size_t parameter_count;
} plainroot_validation_issue_view_v1;

typedef struct plainroot_validation_report plainroot_validation_report;
typedef struct plainroot_validation_pattern plainroot_validation_pattern;

typedef struct plainroot_validation_parse_options_v1 {
  uint32_t struct_size;
  /* Zero selects the documented default. */
  uint32_t maximum_input_bytes;
  uint8_t trim_ascii_whitespace;
  uint8_t boolean_case_insensitive_ascii;
  uint8_t boolean_accept_yes_no;
  uint8_t boolean_accept_0_1;
} plainroot_validation_parse_options_v1;

typedef struct plainroot_validation_parse_i64_result_v1 {
  uint32_t struct_size;
  uint8_t parsed;
  int64_t value;
  /* NONE on success; otherwise describes invalid user input. */
  plainroot_validation_issue_code_v1 failure_code;
} plainroot_validation_parse_i64_result_v1;

typedef struct plainroot_validation_parse_u64_result_v1 {
  uint32_t struct_size;
  uint8_t parsed;
  uint64_t value;
  plainroot_validation_issue_code_v1 failure_code;
} plainroot_validation_parse_u64_result_v1;

typedef struct plainroot_validation_parse_f64_result_v1 {
  uint32_t struct_size;
  uint8_t parsed;
  double value;
  plainroot_validation_issue_code_v1 failure_code;
} plainroot_validation_parse_f64_result_v1;

typedef struct plainroot_validation_parse_bool_result_v1 {
  uint32_t struct_size;
  uint8_t parsed;
  uint8_t value;
  plainroot_validation_issue_code_v1 failure_code;
} plainroot_validation_parse_bool_result_v1;

typedef struct plainroot_validation_text_options_v1 {
  uint32_t struct_size;
  /* Zero selects the documented default. */
  uint32_t maximum_input_bytes;
} plainroot_validation_text_options_v1;

typedef struct plainroot_validation_pattern_options_v1 {
  uint32_t struct_size;
  /* Zero selects the documented default. */
  uint32_t maximum_pattern_bytes;
} plainroot_validation_pattern_options_v1;

typedef PLAINROOT_VALIDATION_ENUM(plainroot_validation_pattern_match_mode_v1) {
  PLAINROOT_VALIDATION_PATTERN_PARTIAL = 0,
  PLAINROOT_VALIDATION_PATTERN_FULL = 1,
} plainroot_validation_pattern_match_mode_v1;

#undef PLAINROOT_VALIDATION_ENUM

/* Keep enum layout fixed for the public C ABI and unknown-value checks. */
#ifdef __cplusplus
static_assert(sizeof(plainroot_validation_result_v1) == sizeof(int32_t));
static_assert(sizeof(plainroot_validation_issue_code_v1) == sizeof(int32_t));
static_assert(sizeof(plainroot_validation_parameter_type_v1) ==
              sizeof(int32_t));
static_assert(sizeof(plainroot_validation_pattern_match_mode_v1) ==
              sizeof(int32_t));
#else
_Static_assert(sizeof(plainroot_validation_result_v1) == sizeof(int32_t),
               "validation result enum must use the 32-bit ABI");
_Static_assert(sizeof(plainroot_validation_issue_code_v1) == sizeof(int32_t),
               "validation issue enum must use the 32-bit ABI");
_Static_assert(sizeof(plainroot_validation_parameter_type_v1) ==
                   sizeof(int32_t),
               "validation parameter enum must use the 32-bit ABI");
_Static_assert(sizeof(plainroot_validation_pattern_match_mode_v1) ==
                   sizeof(int32_t),
               "validation match-mode enum must use the 32-bit ABI");
#endif

PLAINROOT_VALIDATION_API uint32_t
plainroot_validation_abi_version_v1(void);
PLAINROOT_VALIDATION_API const char *
plainroot_validation_result_name_v1(plainroot_validation_result_v1 result);
PLAINROOT_VALIDATION_API const char *
plainroot_validation_issue_code_name_v1(
    plainroot_validation_issue_code_v1 issue_code);

/* These checks append an issue when the value is invalid, and return OK when
 * the check itself ran successfully. Range bounds are inclusive. Bounds are
 * rule data; invalid bounds return INVALID_RULE without changing the report. */
PLAINROOT_VALIDATION_API plainroot_validation_result_v1
plainroot_validation_validate_range_i64_v1(
    plainroot_validation_report *report, const char *field, size_t field_size,
    int64_t value, int64_t minimum, int64_t maximum);
PLAINROOT_VALIDATION_API plainroot_validation_result_v1
plainroot_validation_validate_range_u64_v1(
    plainroot_validation_report *report, const char *field, size_t field_size,
    uint64_t value, uint64_t minimum, uint64_t maximum);
PLAINROOT_VALIDATION_API plainroot_validation_result_v1
plainroot_validation_validate_range_f64_v1(
    plainroot_validation_report *report, const char *field, size_t field_size,
    double value, double minimum, double maximum);
PLAINROOT_VALIDATION_API plainroot_validation_result_v1
plainroot_validation_validate_finite_f64_v1(
    plainroot_validation_report *report, const char *field, size_t field_size,
    double value);

/* Parsers are locale-independent. On malformed/out-of-range text they return
 * OK with parsed=0 and a failure_code; on operational errors they return an
 * error status. Result value is zeroed whenever parsing fails. */
PLAINROOT_VALIDATION_API plainroot_validation_result_v1
plainroot_validation_parse_i64_text_v1(
    const char *text, size_t text_size,
    const plainroot_validation_parse_options_v1 *options,
    plainroot_validation_parse_i64_result_v1 *output);
PLAINROOT_VALIDATION_API plainroot_validation_result_v1
plainroot_validation_parse_u64_text_v1(
    const char *text, size_t text_size,
    const plainroot_validation_parse_options_v1 *options,
    plainroot_validation_parse_u64_result_v1 *output);
PLAINROOT_VALIDATION_API plainroot_validation_result_v1
plainroot_validation_parse_f64_text_v1(
    const char *text, size_t text_size,
    const plainroot_validation_parse_options_v1 *options,
    plainroot_validation_parse_f64_result_v1 *output);
PLAINROOT_VALIDATION_API plainroot_validation_result_v1
plainroot_validation_parse_bool_text_v1(
    const char *text, size_t text_size,
    const plainroot_validation_parse_options_v1 *options,
    plainroot_validation_parse_bool_result_v1 *output);

/* Text checks append an issue for invalid text, including malformed UTF-8, and
 * return OK if the check ran. Human text length counts Unicode scalar values,
 * not bytes or grapheme clusters. Zero options select a 64 KiB input limit;
 * the hard limit is 1 MiB. Nonblank uses Unicode 17.0 White_Space. */
PLAINROOT_VALIDATION_API plainroot_validation_result_v1
plainroot_validation_validate_required_text_v1(
    plainroot_validation_report *report, const char *field, size_t field_size,
    const char *text, size_t text_size,
    const plainroot_validation_text_options_v1 *options);
PLAINROOT_VALIDATION_API plainroot_validation_result_v1
plainroot_validation_validate_nonblank_text_v1(
    plainroot_validation_report *report, const char *field, size_t field_size,
    const char *text, size_t text_size,
    const plainroot_validation_text_options_v1 *options);
PLAINROOT_VALIDATION_API plainroot_validation_result_v1
plainroot_validation_validate_text_length_v1(
    plainroot_validation_report *report, const char *field, size_t field_size,
    const char *text, size_t text_size, uint32_t minimum,
    uint32_t maximum, const plainroot_validation_text_options_v1 *options);
PLAINROOT_VALIDATION_API plainroot_validation_result_v1
plainroot_validation_validate_text_max_bytes_v1(
    plainroot_validation_report *report, const char *field, size_t field_size,
    const char *text, size_t text_size, uint32_t maximum_bytes,
    const plainroot_validation_text_options_v1 *options);
PLAINROOT_VALIDATION_API plainroot_validation_result_v1
plainroot_validation_validate_email_text_v1(
    plainroot_validation_report *report, const char *field, size_t field_size,
    const char *text, size_t text_size,
    const plainroot_validation_text_options_v1 *options);

/* Regex support uses RE2 in UTF-8 mode. Patterns are data, not executable
 * code. The default pattern limit is 4 KiB; the hard pattern limit is 64 KiB.
 * RE2's compiled program and cache budget is 8 MiB. Inputs use the text byte
 * limits above. Full mode matches the whole input; partial mode finds a
 * matching substring. Invalid UTF-8 returns INVALID_UTF8; malformed or
 * unsupported regex syntax returns INVALID_RULE. */
PLAINROOT_VALIDATION_API plainroot_validation_result_v1
plainroot_validation_pattern_compile_v1(
    const char *pattern, size_t pattern_size,
    const plainroot_validation_pattern_options_v1 *options,
    plainroot_validation_pattern **output);
PLAINROOT_VALIDATION_API void
plainroot_validation_pattern_release_v1(
    plainroot_validation_pattern *pattern);
PLAINROOT_VALIDATION_API plainroot_validation_result_v1
plainroot_validation_validate_pattern_v1(
    plainroot_validation_report *report, const char *field, size_t field_size,
    const char *text, size_t text_size,
    const plainroot_validation_text_options_v1 *text_options,
    const plainroot_validation_pattern *pattern,
    plainroot_validation_pattern_match_mode_v1 mode);

/* Zero option fields select defaults. Unknown future option fields are
 * ignored according to struct_size. The output handle is owned by the caller. */
PLAINROOT_VALIDATION_API plainroot_validation_result_v1
plainroot_validation_report_create_v1(
    const plainroot_validation_report_options_v1 *options,
    plainroot_validation_report **output);
PLAINROOT_VALIDATION_API plainroot_validation_result_v1
plainroot_validation_report_retain_v1(plainroot_validation_report *report);
PLAINROOT_VALIDATION_API void
plainroot_validation_report_release_v1(plainroot_validation_report *report);

/* The report copies all issue data. Failed additions leave it unchanged.
 * Mutation is single-owner and must not race with reads. */
PLAINROOT_VALIDATION_API plainroot_validation_result_v1
plainroot_validation_report_add_issue_v1(
    plainroot_validation_report *report,
    const plainroot_validation_issue_v1 *issue);
/* Sealing is idempotent. A sealed report can be shared for concurrent reads. */
PLAINROOT_VALIDATION_API plainroot_validation_result_v1
plainroot_validation_report_seal_v1(plainroot_validation_report *report);
PLAINROOT_VALIDATION_API plainroot_validation_result_v1
plainroot_validation_report_is_valid_v1(
    const plainroot_validation_report *report, uint8_t *is_valid);
PLAINROOT_VALIDATION_API plainroot_validation_result_v1
plainroot_validation_report_issue_count_v1(
    const plainroot_validation_report *report, size_t *issue_count);
/* Returned text pointers are borrowed from the sealed report and remain valid
 * until the report is released. Output struct_size must be initialized. */
PLAINROOT_VALIDATION_API plainroot_validation_result_v1
plainroot_validation_report_get_issue_v1(
    const plainroot_validation_report *report, size_t issue_index,
    plainroot_validation_issue_view_v1 *output);
PLAINROOT_VALIDATION_API plainroot_validation_result_v1
plainroot_validation_report_get_parameter_v1(
    const plainroot_validation_report *report, size_t issue_index,
    size_t parameter_index,
    plainroot_validation_parameter_view_v1 *output);

#ifdef __cplusplus
}
#endif

#endif
