local loader, load_error = package.loadlib(
  PLAINROOT_VALIDATION_MODULE,
  "luaopen_plainroot_validation"
)
assert(loader, load_error)
local validation = loader()

local parsed_age = validation.numbers.parse_integer("42")
assert(parsed_age.status == "ok")
assert(parsed_age.parsed and parsed_age.valid and parsed_age.value == 42)
assert(#parsed_age.issues == 0)

local injection_attempt =
  "rawset(_G, '__plainroot_validation_input_was_executed', true)"
local malformed = validation.numbers.parse_integer(injection_attempt)
assert(malformed.status == "ok")
assert(not malformed.parsed and not malformed.valid)
assert(malformed.issues[1].code == "invalid-integer")
assert(malformed.issues[1].field == "")
assert(#malformed.issues[1].parameters == 0)
assert(_G.__plainroot_validation_input_was_executed == nil)

local trimmed = validation.numbers.parse_integer(" 12 ", {
  trim_ascii_whitespace = true,
})
assert(trimmed.parsed and trimmed.value == 12)

local parsed_real = validation.numbers.parse_real("1.25e2")
assert(parsed_real.parsed and parsed_real.value == 125)
assert(parsed_real.type == "f64")

local parsed_unsigned = validation.numbers.parse_unsigned_integer("18446744073709551615")
assert(parsed_unsigned.parsed and parsed_unsigned.type == "u64")
assert(parsed_unsigned.value == "18446744073709551615")

local parsed_boolean = validation.booleans.parse("YES", {
  boolean_case_insensitive_ascii = true,
  boolean_accept_yes_no = true,
})
assert(parsed_boolean.parsed and parsed_boolean.value == true)

local base = validation.numbers.with(4)
local accepted = base:is_in_range(2, 10):validate("profile.age")
assert(accepted.status == "ok" and accepted.valid)
assert(#accepted.issues == 0)

local rejected = base:is_in_range(5, 10):validate("profile.age")
assert(rejected.status == "ok" and not rejected.valid)
assert(rejected.issues[1].code == "out-of-range")
assert(rejected.issues[1].field == "profile.age")
assert(rejected.issues[1].parameters[1].name == "minimum")
assert(rejected.issues[1].parameters[1].type == "i64")
assert(rejected.issues[1].parameters[1].value == 5)
assert(rejected.issues[1].parameters[2].name == "maximum")

local invalid_rule = base:is_in_range(9, 3):validate()
assert(invalid_rule.status == "invalid-rule" and not invalid_rule.valid)

local real_range = validation.numbers.with(1.25)
  :is_in_range(1.0, 1.5)
  :is_finite()
  :validate("profile.scale")
assert(real_range.status == "ok" and real_range.valid)

local non_finite = validation.numbers.with(math.huge):is_finite():validate()
assert(non_finite.status == "ok" and not non_finite.valid)
assert(non_finite.issues[1].code == "non-finite-number")

local text_result = validation.text.with("alex@example.org")
  :is_not_empty()
  :is_not_blank()
  :has_length(3, 64)
  :has_maximum_bytes(80)
  :is_email()
  :matches("^[a-z]+@[a-z.]+$", "full")
  :validate("account.email")
assert(text_result.status == "ok" and text_result.valid)

local partial_pattern = validation.text.with("prefix-value-suffix")
  :matches("value", "partial")
  :validate()
assert(partial_pattern.status == "ok" and partial_pattern.valid)

local bad_email = validation.text.with(" ")
  :is_not_blank()
  :is_email()
  :validate("account.email")
assert(not bad_email.valid)
assert(bad_email.issues[1].code == "blank")
assert(bad_email.issues[2].code == "invalid-email")

local bad_pattern = validation.text.with("value")
  :matches("(")
  :validate()
assert(bad_pattern.status == "invalid-rule")
assert(#bad_pattern.issues == 0)

local oversized_pattern = validation.text.with("value")
  :matches(string.rep("a", 4097))
  :validate()
assert(oversized_pattern.status == "limit-exceeded")

local lower_limit, lower_limit_error = validation.text.with("12345", {
  maximum_input_bytes = 4,
})
assert(lower_limit == nil)
assert(lower_limit_error.status == "limit-exceeded")

local default_limit, default_limit_error = validation.text.with(string.rep("x", 65537))
assert(default_limit == nil)
assert(default_limit_error.status == "limit-exceeded")

local oversized_option, option_error = validation.text.with("x", {
  maximum_input_bytes = 1024 * 1024 + 1,
})
assert(oversized_option == nil)
assert(option_error.status == "invalid-argument")

local irrelevant_text_option, irrelevant_error = validation.text.with("x", {
  trim_ascii_whitespace = true,
})
assert(irrelevant_text_option == nil)
assert(irrelevant_error.status == "invalid-argument")

local unknown_option, unknown_error = validation.numbers.parse_integer("1", {
  evaluate = "return true",
})
assert(unknown_option == nil)
assert(unknown_error.status == "invalid-argument")

local raw_options = setmetatable({ trim_ascii_whitespace = true }, {
  __pairs = function() error("metamethod must not run") end,
  __index = function() error("index metamethod must not run") end,
})
assert(validation.numbers.parse_integer(" 7 ", raw_options).value == 7)

local too_many = validation.text.with("x")
for _ = 1, 32 do
  too_many = too_many:is_not_empty()
end
local overflow_builder, overflow_error = too_many:is_not_blank()
assert(overflow_builder == nil)
assert(overflow_error.status == "limit-exceeded")

print("Lua validation adapter tests passed")
