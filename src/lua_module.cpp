#include <plainroot/validation/validation.hpp>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {

using namespace plainroot::validation;

constexpr const char *kBuilderMetatable = "plainroot.validation.builder";
constexpr std::size_t kMaximumRules = 32;

enum class ValueKind { SignedInteger, Real, Text };
enum class RuleKind {
  SignedRange,
  RealRange,
  Finite,
  Required,
  Nonblank,
  Length,
  MaximumBytes,
  Email,
  Pattern,
};

struct Rule {
  RuleKind kind = RuleKind::Required;
  int64_t signed_minimum = 0;
  int64_t signed_maximum = 0;
  double real_minimum = 0;
  double real_maximum = 0;
  uint32_t minimum = 0;
  uint32_t maximum = 0;
  PatternMatchMode pattern_mode = PatternMatchMode::Full;
  CompiledPattern pattern;
};

struct Builder {
  ValueKind kind = ValueKind::Text;
  int64_t signed_value = 0;
  double real_value = 0;
  std::shared_ptr<const std::string> text_value;
  TextOptions text_options;
  Result configuration_status = PLAINROOT_VALIDATION_OK;
  std::vector<Rule> rules;
};

struct BuilderBox {
  bool initialized = false;
  alignas(Builder) std::byte storage[sizeof(Builder)];

  Builder *get() noexcept {
    return std::launder(reinterpret_cast<Builder *>(storage));
  }
};

void set_string_field(lua_State *state, const char *key,
                      std::string_view value) {
  lua_pushlstring(state, value.data(), value.size());
  lua_setfield(state, -2, key);
}

void set_boolean_field(lua_State *state, const char *key, bool value) {
  lua_pushboolean(state, value ? 1 : 0);
  lua_setfield(state, -2, key);
}

std::string_view status_name(Result status) noexcept {
  return result_name(status);
}

int push_error(lua_State *state, Result status) {
  lua_pushnil(state);
  lua_createtable(state, 0, 1);
  set_string_field(state, "status", status_name(status));
  return 2;
}

int push_reset_error(lua_State *state, int stack_base, Result status) {
  lua_settop(state, stack_base);
  return push_error(state, status);
}

bool read_string(lua_State *state, int index, std::string_view& output) {
  if (lua_type(state, index) != LUA_TSTRING)
    return false;
  std::size_t size = 0;
  const char *data = lua_tolstring(state, index, &size);
  output = std::string_view(data, size);
  return true;
}

bool read_integer(lua_State *state, int index, lua_Integer& output) {
  if (lua_type(state, index) != LUA_TNUMBER || !lua_isinteger(state, index))
    return false;
  int is_number = 0;
  output = lua_tointegerx(state, index, &is_number);
  return is_number != 0;
}

bool read_unsigned_32(lua_State *state, int index, uint32_t& output) {
  lua_Integer value = 0;
  if (!read_integer(state, index, value) || value < 0 ||
      static_cast<uint64_t>(value) > std::numeric_limits<uint32_t>::max())
    return false;
  output = static_cast<uint32_t>(value);
  return true;
}

bool read_number(lua_State *state, int index, double& output) {
  if (lua_type(state, index) != LUA_TNUMBER)
    return false;
  int is_number = 0;
  output = lua_tonumberx(state, index, &is_number);
  return is_number != 0;
}

bool parse_options(lua_State *state, int index, ParseOptions& output,
                   bool text_only = false) {
  if (lua_isnoneornil(state, index))
    return true;
  if (lua_type(state, index) != LUA_TTABLE)
    return false;

  const int stack_base = lua_gettop(state);
  const int table_index = lua_absindex(state, index);
  lua_pushnil(state);
  while (lua_next(state, table_index) != 0) {
    std::string_view key;
    bool valid = read_string(state, -2, key);
    if (valid && text_only && key != "maximum_input_bytes") {
      valid = false;
    } else if (valid && key == "maximum_input_bytes") {
      uint32_t maximum = 0;
      valid = read_unsigned_32(state, -1, maximum) &&
              maximum <= PLAINROOT_VALIDATION_MAXIMUM_INPUT_BYTES;
      if (valid)
        output.maximum_input_bytes = maximum;
    } else if (valid && key == "trim_ascii_whitespace") {
      valid = lua_type(state, -1) == LUA_TBOOLEAN;
      if (valid)
        output.trim_ascii_whitespace = lua_toboolean(state, -1) != 0;
    } else if (valid && key == "boolean_case_insensitive_ascii") {
      valid = lua_type(state, -1) == LUA_TBOOLEAN;
      if (valid)
        output.boolean_case_insensitive_ascii = lua_toboolean(state, -1) != 0;
    } else if (valid && key == "boolean_accept_yes_no") {
      valid = lua_type(state, -1) == LUA_TBOOLEAN;
      if (valid)
        output.boolean_accept_yes_no = lua_toboolean(state, -1) != 0;
    } else if (valid && key == "boolean_accept_0_1") {
      valid = lua_type(state, -1) == LUA_TBOOLEAN;
      if (valid)
        output.boolean_accept_0_1 = lua_toboolean(state, -1) != 0;
    } else {
      valid = false;
    }
    lua_pop(state, 1);
    if (!valid) {
      lua_settop(state, stack_base);
      return false;
    }
  }
  lua_settop(state, stack_base);
  return true;
}

bool parse_text_options(lua_State *state, int index, TextOptions& output) {
  ParseOptions parse;
  if (!parse_options(state, index, parse, true))
    return false;
  output.maximum_input_bytes = parse.maximum_input_bytes;
  return true;
}

int push_parse_issue(lua_State *state, IssueCode code) {
  lua_createtable(state, 0, 3);
  set_string_field(state, "code", issue_code_name(code));
  set_string_field(state, "field", {});
  lua_createtable(state, 0, 0);
  lua_setfield(state, -2, "parameters");
  return 1;
}

template <typename T>
int push_parse_result(lua_State *state, int stack_base,
                      const ParseResult<T>& parsed) {
  if (parsed.result != PLAINROOT_VALIDATION_OK)
    return push_reset_error(state, stack_base, parsed.result);

  lua_createtable(state, 0, 5);
  set_string_field(state, "status", status_name(parsed.result));
  set_boolean_field(state, "parsed", parsed.parsed);
  set_boolean_field(state, "valid", parsed.parsed);
  if constexpr (std::is_same_v<T, bool>) {
    set_string_field(state, "type", "bool");
  } else if constexpr (std::is_same_v<T, double>) {
    set_string_field(state, "type", "f64");
  } else if constexpr (std::is_same_v<T, uint64_t>) {
    set_string_field(state, "type", "u64");
  } else {
    set_string_field(state, "type", "i64");
  }
  if (parsed.parsed) {
    if constexpr (std::is_same_v<T, bool>) {
      lua_pushboolean(state, parsed.value ? 1 : 0);
    } else if constexpr (std::is_floating_point_v<T>) {
      lua_pushnumber(state, static_cast<lua_Number>(parsed.value));
    } else if constexpr (std::is_same_v<T, uint64_t>) {
      const std::string decimal = std::to_string(parsed.value);
      lua_pushlstring(state, decimal.data(), decimal.size());
    } else {
      lua_pushinteger(state, static_cast<lua_Integer>(parsed.value));
    }
    lua_setfield(state, -2, "value");
    lua_createtable(state, 0, 0);
  } else {
    lua_createtable(state, 1, 0);
    if (parsed.failure_code != PLAINROOT_VALIDATION_ISSUE_NONE) {
      push_parse_issue(state, parsed.failure_code);
      lua_rawseti(state, -2, 1);
    }
  }
  lua_setfield(state, -2, "issues");
  return 1;
}

int parse_integer(lua_State *state) {
  const int stack_base = lua_gettop(state);
  std::string_view text;
  ParseOptions options;
  if (!read_string(state, 1, text) || !parse_options(state, 2, options))
    return push_reset_error(state, stack_base,
                            PLAINROOT_VALIDATION_INVALID_ARGUMENT);
  return push_parse_result(state, stack_base,
                           plainroot::validation::parse_integer<int64_t>(text,
                                                                         options));
}

int parse_real(lua_State *state) {
  const int stack_base = lua_gettop(state);
  std::string_view text;
  ParseOptions options;
  if (!read_string(state, 1, text) || !parse_options(state, 2, options))
    return push_reset_error(state, stack_base,
                            PLAINROOT_VALIDATION_INVALID_ARGUMENT);
  return push_parse_result(state, stack_base,
                           plainroot::validation::parse_real<double>(text,
                                                                      options));
}

int parse_unsigned_integer(lua_State *state) {
  const int stack_base = lua_gettop(state);
  std::string_view text;
  ParseOptions options;
  if (!read_string(state, 1, text) || !parse_options(state, 2, options))
    return push_reset_error(state, stack_base,
                            PLAINROOT_VALIDATION_INVALID_ARGUMENT);
  return push_parse_result(
      state, stack_base,
      plainroot::validation::parse_integer<uint64_t>(text, options));
}

int parse_boolean(lua_State *state) {
  const int stack_base = lua_gettop(state);
  std::string_view text;
  ParseOptions options;
  if (!read_string(state, 1, text) || !parse_options(state, 2, options))
    return push_reset_error(state, stack_base,
                            PLAINROOT_VALIDATION_INVALID_ARGUMENT);
  return push_parse_result(state, stack_base,
                           plainroot::validation::parse_boolean(text, options));
}

BuilderBox *test_builder(lua_State *state, int index) {
  return static_cast<BuilderBox *>(luaL_testudata(state, index,
                                                  kBuilderMetatable));
}

int collect_builder(lua_State *state) {
  auto *box = test_builder(state, 1);
  if (box && box->initialized) {
    box->get()->~Builder();
    box->initialized = false;
  }
  return 0;
}

int push_builder_copy(lua_State *state, int stack_base, const Builder& source,
                      const Rule *rule,
                      Result configuration_status = PLAINROOT_VALIDATION_OK) {
  auto *box = static_cast<BuilderBox *>(
      lua_newuserdatauv(state, sizeof(BuilderBox), 0));
  box->initialized = false;
  luaL_getmetatable(state, kBuilderMetatable);
  lua_setmetatable(state, -2);

  try {
    new (box->storage) Builder(source);
    box->initialized = true;
    Builder *copy = box->get();
    if (rule) {
      if (copy->rules.size() >= kMaximumRules) {
        copy->~Builder();
        box->initialized = false;
        return push_reset_error(state, stack_base,
                                PLAINROOT_VALIDATION_LIMIT_EXCEEDED);
      }
      copy->rules.push_back(*rule);
    }
    if (copy->configuration_status == PLAINROOT_VALIDATION_OK &&
        configuration_status != PLAINROOT_VALIDATION_OK)
      copy->configuration_status = configuration_status;
    return 1;
  } catch (const std::bad_alloc&) {
    if (box->initialized) {
      box->get()->~Builder();
      box->initialized = false;
    }
    return push_reset_error(state, stack_base,
                            PLAINROOT_VALIDATION_OUT_OF_MEMORY);
  } catch (...) {
    if (box->initialized) {
      box->get()->~Builder();
      box->initialized = false;
    }
    return push_reset_error(state, stack_base,
                            PLAINROOT_VALIDATION_INTERNAL_ERROR);
  }
}

int numbers_with(lua_State *state) {
  const int stack_base = lua_gettop(state);
  if (lua_type(state, 1) != LUA_TNUMBER)
    return push_reset_error(state, stack_base,
                            PLAINROOT_VALIDATION_INVALID_ARGUMENT);

  Builder builder;
  if (lua_isinteger(state, 1)) {
    builder.kind = ValueKind::SignedInteger;
    builder.signed_value = lua_tointeger(state, 1);
  } else {
    int is_number = 0;
    builder.real_value = lua_tonumberx(state, 1, &is_number);
    if (!is_number)
      return push_reset_error(state, stack_base,
                              PLAINROOT_VALIDATION_INVALID_ARGUMENT);
    builder.kind = ValueKind::Real;
  }
  return push_builder_copy(state, stack_base, builder, nullptr);
}

int text_with(lua_State *state) {
  const int stack_base = lua_gettop(state);
  std::string_view text;
  TextOptions options;
  if (!read_string(state, 1, text) || !parse_text_options(state, 2, options))
    return push_reset_error(state, stack_base,
                            PLAINROOT_VALIDATION_INVALID_ARGUMENT);

  const uint32_t maximum = options.maximum_input_bytes == 0
                               ? PLAINROOT_VALIDATION_DEFAULT_MAXIMUM_INPUT_BYTES
                               : options.maximum_input_bytes;
  if (text.size() > maximum)
    return push_reset_error(state, stack_base,
                            PLAINROOT_VALIDATION_LIMIT_EXCEEDED);

  Builder builder;
  try {
    builder.kind = ValueKind::Text;
    builder.text_value = std::make_shared<const std::string>(text);
    builder.text_options = options;
  } catch (const std::bad_alloc&) {
    return push_reset_error(state, stack_base,
                            PLAINROOT_VALIDATION_OUT_OF_MEMORY);
  } catch (...) {
    return push_reset_error(state, stack_base,
                            PLAINROOT_VALIDATION_INTERNAL_ERROR);
  }
  return push_builder_copy(state, stack_base, builder, nullptr);
}

int append_range(lua_State *state) {
  const int stack_base = lua_gettop(state);
  BuilderBox *box = test_builder(state, 1);
  if (!box || !box->initialized || box->get()->kind == ValueKind::Text)
    return push_reset_error(state, stack_base,
                            PLAINROOT_VALIDATION_INVALID_ARGUMENT);
  Rule rule;
  if (box->get()->kind == ValueKind::SignedInteger) {
    lua_Integer minimum = 0;
    lua_Integer maximum = 0;
    if (!read_integer(state, 2, minimum) || !read_integer(state, 3, maximum))
      return push_reset_error(state, stack_base,
                              PLAINROOT_VALIDATION_INVALID_ARGUMENT);
    rule.kind = RuleKind::SignedRange;
    rule.signed_minimum = static_cast<int64_t>(minimum);
    rule.signed_maximum = static_cast<int64_t>(maximum);
  } else {
    if (!read_number(state, 2, rule.real_minimum) ||
        !read_number(state, 3, rule.real_maximum))
      return push_reset_error(state, stack_base,
                              PLAINROOT_VALIDATION_INVALID_ARGUMENT);
    rule.kind = RuleKind::RealRange;
  }
  return push_builder_copy(state, stack_base, *box->get(), &rule);
}

int append_finite(lua_State *state) {
  const int stack_base = lua_gettop(state);
  BuilderBox *box = test_builder(state, 1);
  if (!box || !box->initialized || box->get()->kind != ValueKind::Real)
    return push_reset_error(state, stack_base,
                            PLAINROOT_VALIDATION_INVALID_ARGUMENT);
  Rule rule;
  rule.kind = RuleKind::Finite;
  return push_builder_copy(state, stack_base, *box->get(), &rule);
}

int append_text_rule(lua_State *state, RuleKind kind) {
  const int stack_base = lua_gettop(state);
  BuilderBox *box = test_builder(state, 1);
  if (!box || !box->initialized || box->get()->kind != ValueKind::Text)
    return push_reset_error(state, stack_base,
                            PLAINROOT_VALIDATION_INVALID_ARGUMENT);
  Rule rule;
  rule.kind = kind;
  return push_builder_copy(state, stack_base, *box->get(), &rule);
}

int append_required(lua_State *state) {
  return append_text_rule(state, RuleKind::Required);
}

int append_nonblank(lua_State *state) {
  return append_text_rule(state, RuleKind::Nonblank);
}

int append_email(lua_State *state) {
  return append_text_rule(state, RuleKind::Email);
}

int append_length(lua_State *state) {
  const int stack_base = lua_gettop(state);
  BuilderBox *box = test_builder(state, 1);
  uint32_t minimum = 0;
  uint32_t maximum = 0;
  if (!box || !box->initialized || box->get()->kind != ValueKind::Text ||
      !read_unsigned_32(state, 2, minimum) ||
      !read_unsigned_32(state, 3, maximum))
    return push_reset_error(state, stack_base,
                            PLAINROOT_VALIDATION_INVALID_ARGUMENT);
  Rule rule;
  rule.kind = RuleKind::Length;
  rule.minimum = minimum;
  rule.maximum = maximum;
  return push_builder_copy(state, stack_base, *box->get(), &rule);
}

int append_maximum_bytes(lua_State *state) {
  const int stack_base = lua_gettop(state);
  BuilderBox *box = test_builder(state, 1);
  uint32_t maximum = 0;
  if (!box || !box->initialized || box->get()->kind != ValueKind::Text ||
      !read_unsigned_32(state, 2, maximum))
    return push_reset_error(state, stack_base,
                            PLAINROOT_VALIDATION_INVALID_ARGUMENT);
  Rule rule;
  rule.kind = RuleKind::MaximumBytes;
  rule.maximum = maximum;
  return push_builder_copy(state, stack_base, *box->get(), &rule);
}

int append_pattern(lua_State *state) {
  const int stack_base = lua_gettop(state);
  BuilderBox *box = test_builder(state, 1);
  std::string_view pattern_text;
  if (!box || !box->initialized || box->get()->kind != ValueKind::Text ||
      !read_string(state, 2, pattern_text))
    return push_reset_error(state, stack_base,
                            PLAINROOT_VALIDATION_INVALID_ARGUMENT);

  Rule rule;
  rule.kind = RuleKind::Pattern;
  if (lua_isnoneornil(state, 3)) {
    rule.pattern_mode = PatternMatchMode::Full;
  } else {
    std::string_view mode;
    if (!read_string(state, 3, mode))
      return push_reset_error(state, stack_base,
                              PLAINROOT_VALIDATION_INVALID_ARGUMENT);
    if (mode == "full")
      rule.pattern_mode = PatternMatchMode::Full;
    else if (mode == "partial")
      rule.pattern_mode = PatternMatchMode::Partial;
    else
      return push_reset_error(state, stack_base,
                              PLAINROOT_VALIDATION_INVALID_ARGUMENT);
  }

  const Result compiled = CompiledPattern::compile(pattern_text, rule.pattern);
  if (compiled == PLAINROOT_VALIDATION_OK)
    return push_builder_copy(state, stack_base, *box->get(), &rule);
  return push_builder_copy(state, stack_base, *box->get(), nullptr, compiled);
}

Result validate_builder(const Builder& builder, std::string_view field,
                        Report& report) {
  if (builder.configuration_status != PLAINROOT_VALIDATION_OK)
    return builder.configuration_status;

  for (const Rule& rule : builder.rules) {
    Result result = PLAINROOT_VALIDATION_OK;
    switch (rule.kind) {
      case RuleKind::SignedRange:
        result = RangeValidator<int64_t>{rule.signed_minimum,
                                         rule.signed_maximum}
                     .validate(builder.signed_value, report, field);
        break;
      case RuleKind::RealRange:
        result = RangeValidator<double>{rule.real_minimum, rule.real_maximum}
                     .validate(builder.real_value, report, field);
        break;
      case RuleKind::Finite:
        result = FiniteNumberValidator<double>{}.validate(builder.real_value,
                                                            report, field);
        break;
      case RuleKind::Required:
        result = RequiredTextValidator{}.validate(
            *builder.text_value, report, field, builder.text_options);
        break;
      case RuleKind::Nonblank:
        result = NonblankTextValidator{}.validate(
            *builder.text_value, report, field, builder.text_options);
        break;
      case RuleKind::Length:
        result = TextLengthValidator{rule.minimum, rule.maximum}.validate(
            *builder.text_value, report, field, builder.text_options);
        break;
      case RuleKind::MaximumBytes:
        result = TextByteLimitValidator{rule.maximum}.validate(
            *builder.text_value, report, field, builder.text_options);
        break;
      case RuleKind::Email:
        result = EmailValidator{}.validate(*builder.text_value, report, field,
                                           builder.text_options);
        break;
      case RuleKind::Pattern:
        result = rule.pattern.validate(*builder.text_value, report, field,
                                       rule.pattern_mode,
                                       builder.text_options);
        break;
    }
    if (result != PLAINROOT_VALIDATION_OK)
      return result;
  }
  return PLAINROOT_VALIDATION_OK;
}

void push_parameter(lua_State *state, const Parameter& parameter) {
  lua_createtable(state, 0, 3);
  set_string_field(state, "name", parameter.name);
  std::visit(
      [state](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, bool>) {
          set_string_field(state, "type", "bool");
          set_boolean_field(state, "value", value);
        } else if constexpr (std::is_same_v<T, int64_t>) {
          set_string_field(state, "type", "i64");
          lua_pushinteger(state, static_cast<lua_Integer>(value));
          lua_setfield(state, -2, "value");
        } else if constexpr (std::is_same_v<T, uint64_t>) {
          set_string_field(state, "type", "u64");
          const std::string decimal = std::to_string(value);
          set_string_field(state, "value", decimal);
        } else if constexpr (std::is_same_v<T, double>) {
          set_string_field(state, "type", "f64");
          lua_pushnumber(state, static_cast<lua_Number>(value));
          lua_setfield(state, -2, "value");
        } else {
          set_string_field(state, "type", "text");
          set_string_field(state, "value", value);
        }
      },
      parameter.value);
}

void push_issue(lua_State *state, const Issue& issue) {
  lua_createtable(state, 0, 3);
  set_string_field(state, "code", issue_code_name(issue.code));
  set_string_field(state, "field", issue.field);
  lua_createtable(state, static_cast<int>(issue.parameters.size()), 0);
  for (std::size_t index = 0; index < issue.parameters.size(); ++index) {
    push_parameter(state, issue.parameters[index]);
    lua_rawseti(state, -2, static_cast<lua_Integer>(index + 1));
  }
  lua_setfield(state, -2, "parameters");
}

int push_report_result(lua_State *state, int stack_base, Report& report,
                       Result status) {
  if (report && !report.sealed()) {
    const Result seal_status = report.seal();
    if (status == PLAINROOT_VALIDATION_OK)
      status = seal_status;
  }

  bool valid = false;
  if (status == PLAINROOT_VALIDATION_OK && report) {
    const Result valid_status = report.is_valid(valid);
    if (valid_status != PLAINROOT_VALIDATION_OK)
      status = valid_status;
  }

  std::size_t issue_count = 0;
  if (report) {
    const Result count_status = report.issue_count(issue_count);
    if (count_status != PLAINROOT_VALIDATION_OK)
      return push_reset_error(state, stack_base, count_status);
  }

  lua_createtable(state, 0, 3);
  set_string_field(state, "status", status_name(status));
  set_boolean_field(state, "valid", status == PLAINROOT_VALIDATION_OK && valid);
  lua_createtable(state, static_cast<int>(issue_count), 0);
  for (std::size_t index = 0; index < issue_count; ++index) {
    Issue issue;
    const Result issue_status = report.issue(index, issue);
    if (issue_status != PLAINROOT_VALIDATION_OK)
      return push_reset_error(state, stack_base, issue_status);
    push_issue(state, issue);
    lua_rawseti(state, -2, static_cast<lua_Integer>(index + 1));
  }
  lua_setfield(state, -2, "issues");
  return 1;
}

int builder_validate(lua_State *state) {
  const int stack_base = lua_gettop(state);
  BuilderBox *box = test_builder(state, 1);
  if (!box || !box->initialized)
    return push_reset_error(state, stack_base,
                            PLAINROOT_VALIDATION_INVALID_ARGUMENT);
  std::string_view field;
  if (!lua_isnoneornil(state, 2) && !read_string(state, 2, field))
    return push_reset_error(state, stack_base,
                            PLAINROOT_VALIDATION_INVALID_ARGUMENT);

  Report report;
  Result status = Report::create(report);
  if (status == PLAINROOT_VALIDATION_OK) {
    try {
      status = validate_builder(*box->get(), field, report);
    } catch (const std::bad_alloc&) {
      status = PLAINROOT_VALIDATION_OUT_OF_MEMORY;
    } catch (...) {
      status = PLAINROOT_VALIDATION_INTERNAL_ERROR;
    }
  }
  return push_report_result(state, stack_base, report, status);
}

void set_function(lua_State *state, const char *name, lua_CFunction function) {
  lua_pushcfunction(state, function);
  lua_setfield(state, -2, name);
}

void create_builder_metatable(lua_State *state) {
  luaL_newmetatable(state, kBuilderMetatable);
  set_function(state, "__gc", collect_builder);
  lua_createtable(state, 0, 11);
  set_function(state, "is_in_range", append_range);
  set_function(state, "is_finite", append_finite);
  set_function(state, "is_not_empty", append_required);
  set_function(state, "is_not_blank", append_nonblank);
  set_function(state, "has_length", append_length);
  set_function(state, "has_maximum_bytes", append_maximum_bytes);
  set_function(state, "is_email", append_email);
  set_function(state, "matches", append_pattern);
  set_function(state, "validate", builder_validate);
  lua_setfield(state, -2, "__index");
  lua_pop(state, 1);
}

void create_numbers_table(lua_State *state) {
  lua_createtable(state, 0, 5);
  set_function(state, "with", numbers_with);
  set_function(state, "parse_integer", parse_integer);
  set_function(state, "parse_unsigned_integer", parse_unsigned_integer);
  set_function(state, "parse_real", parse_real);
}

void create_booleans_table(lua_State *state) {
  lua_createtable(state, 0, 1);
  set_function(state, "parse", parse_boolean);
}

void create_text_table(lua_State *state) {
  lua_createtable(state, 0, 1);
  set_function(state, "with", text_with);
}

}  // namespace

extern "C" __attribute__((visibility("default"))) int
luaopen_plainroot_validation(lua_State *state) {
  create_builder_metatable(state);
  lua_createtable(state, 0, 3);
  create_numbers_table(state);
  lua_setfield(state, -2, "numbers");
  create_booleans_table(state);
  lua_setfield(state, -2, "booleans");
  create_text_table(state);
  lua_setfield(state, -2, "text");
  return 1;
}
