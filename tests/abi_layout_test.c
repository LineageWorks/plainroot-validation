#include <plainroot/validation/validation.h>

#include <stddef.h>
#include <stdint.h>

_Static_assert(sizeof(plainroot_validation_result_v1) == 4,
               "result enum ABI must remain 32-bit");
_Static_assert(sizeof(plainroot_validation_issue_code_v1) == 4,
               "issue-code enum ABI must remain 32-bit");
_Static_assert(sizeof(plainroot_validation_parameter_type_v1) == 4,
               "parameter-type enum ABI must remain 32-bit");
_Static_assert(sizeof(plainroot_validation_pattern_match_mode_v1) == 4,
               "pattern-mode enum ABI must remain 32-bit");

#if UINTPTR_MAX == UINT64_MAX
_Static_assert(sizeof(plainroot_validation_parameter_v1) == 48,
               "64-bit parameter ABI changed");
_Static_assert(offsetof(plainroot_validation_parameter_v1, type) == 24,
               "64-bit parameter type offset changed");
_Static_assert(offsetof(plainroot_validation_parameter_v1, as) == 32,
               "64-bit parameter value offset changed");
_Static_assert(sizeof(plainroot_validation_issue_v1) == 40,
               "64-bit issue ABI changed");
_Static_assert(offsetof(plainroot_validation_issue_v1, parameters) == 24,
               "64-bit issue parameters offset changed");
_Static_assert(sizeof(plainroot_validation_report_options_v1) == 24,
               "64-bit report-options ABI changed");
_Static_assert(sizeof(plainroot_validation_parameter_view_v1) == 40,
               "64-bit parameter-view ABI changed");
_Static_assert(sizeof(plainroot_validation_issue_view_v1) == 32,
               "64-bit issue-view ABI changed");
_Static_assert(sizeof(plainroot_validation_parse_options_v1) == 12,
               "parse-options ABI changed");
_Static_assert(sizeof(plainroot_validation_parse_i64_result_v1) == 24,
               "signed parse-result ABI changed");
_Static_assert(sizeof(plainroot_validation_parse_u64_result_v1) == 24,
               "unsigned parse-result ABI changed");
_Static_assert(sizeof(plainroot_validation_parse_f64_result_v1) == 24,
               "real parse-result ABI changed");
_Static_assert(sizeof(plainroot_validation_parse_bool_result_v1) == 12,
               "boolean parse-result ABI changed");
_Static_assert(sizeof(plainroot_validation_text_options_v1) == 8,
               "text-options ABI changed");
_Static_assert(sizeof(plainroot_validation_pattern_options_v1) == 8,
               "pattern-options ABI changed");
#endif

int main(void) { return 0; }
