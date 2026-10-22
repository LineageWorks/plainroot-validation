# Validation Contract

This document defines the behavior of `plainroot-validation`. Decisions that
are deferred are kept out of the initial versioned ABI until their semantics
are settled.

## Purpose and boundaries

The library provides reusable, deterministic validation and parsing for
application input. It must work without a UI, toolkit, state store, event loop,
Lua runtime, settings daemon, or persistence layer.

The library owns validation rules and structured results. Callers own input
collection, field labels, localization, display timing, and actions taken
after a result. Validation is a domain check; it is not a general-purpose
sanitizer, output encoder, or code-execution security boundary.

The dependency direction is:

```text
plainroot-validation     plainroot-state
          \                 /
           application commands
                   |
        UI, CLI, or service adapter
```

Neither library depends on the other. An application command may validate a
candidate before committing it to state. A form/controller layer may later
compose both libraries without making either one depend on the controller.

## Separate parsing, validation, and normalization

These operations have different outcomes and must remain distinguishable:

1. **Parsing** converts a representation such as text into a typed value. It
   can fail because the text is malformed or outside the target type's range.
2. **Validation** checks a value or parsed value against rules such as a range,
   required status, length, or pattern.
3. **Normalization** deliberately transforms a value, for example by trimming
   whitespace. It is an explicit caller-selected operation and must never be
   performed implicitly by validation.

The result for failed parsing must not contain a usable parsed value. A text
builder may compose parse and constraint rules, but it must report whether
parsing or a later constraint failed.

## Result model

Results should support both a simple `valid()` query and a list of structured
issues. Each issue should contain:

- A stable machine-readable code.
- Typed parameters needed to describe the failure, such as minimum, maximum,
  or an invalid byte offset.
- Optional caller-provided context such as a field path, kept separate from
  localized wording.

The library must not return translated prose. User-facing applications map
issue codes and parameters to their own message catalogs. Diagnostics must not
include the rejected input by default, since it may contain private data.

Independent rules should accumulate issues in declaration order. A failed
parse should stop rules that require the parsed value. Reports must have a
documented maximum issue count so a large rule set cannot cause unbounded
allocation.

Rule construction errors are distinct from invalid user input. Examples include
an inverted range, an invalid regex pattern, or conflicting builder options.

## Typed numeric validation

Numeric validators use C++ templates constrained to standard signed and
unsigned integer types up to 64 value bits, plus `float` and `double`. Boolean
and character code-unit types are not accepted as ordinary numeric types. The
versioned C interface uses `int64_t`, `uint64_t`, and `double` underneath the
C++ templates.

Version one provides inclusive range bounds. Exclusive bounds can be added
after a consumer establishes the need. A range whose minimum is greater than
its maximum is a rule-construction error.

Typed floating-point validation rejects NaN and infinities. Numeric limits and
comparisons must avoid overflow and undefined conversions. A finite-number
validator applies to typed floating-point values; it does not parse text.

## Parsing numeric and boolean text

Numeric text parsing must identify the requested target type, such as signed
integer, unsigned integer, or real number. Parsing must reject overflow,
underflow when the target policy requires it, trailing unconsumed characters,
and syntax that is not accepted by the selected parser mode.

The default parser should be locale-independent. Localized digits and decimal
separators require an explicit parser policy and must not depend on process
locale. Version one uses an ASCII grammar. Signed and unsigned integers accept
an optional leading `+`; signed integers also accept `-`, while unsigned
integers reject it. A real number accepts an optional sign, decimal digits with
an optional decimal point, and an optional `e` or `E` exponent with an optional
sign. At least one mantissa digit and, when present, at least one exponent
digit are required. Hexadecimal notation, digit separators, `NaN`, and
infinities are rejected. The parser consumes the entire input. It does not
trim whitespace by default; the caller may explicitly enable ASCII whitespace
trimming. Numeric text input is limited to 64 KiB by default and 1 MiB at most,
with a lower per-call limit available. Decimal separators and localized digits
are never inferred from the process locale. Representable subnormal
floating-point values are accepted. A nonzero value that underflows outside
the target type's representable range is a parse error, as is overflow.

Boolean text parsing accepts exactly lowercase ASCII `true` and `false` by
default. Matching is case-sensitive. Options can independently enable
ASCII-case-insensitive matching, `yes/no`, and `1/0` aliases. Parsing returns
the parsed boolean so callers need not validate against one expected value
just to convert the text.

The C parser APIs return an operation status separately from the parse result.
Malformed text and values outside the target type return `OK`, set `parsed` to
false, and provide a stable `failure_code`; the output value is zeroed. Invalid
options, null pointers paired with nonzero lengths, and input-size violations
return an operation error. The C++ `ParseResult<T>` preserves these two layers
as `result` and `parsed`.

## Text validation and Unicode

Text input defaults to a 64 KiB byte limit; callers can lower that limit, and
the hard limit is 1 MiB. The library enforces it before scanning input.
Malformed UTF-8 is appended as an `INVALID_UTF8` issue with the first invalid
byte offset. Validation does not repair or normalize text.

`RequiredTextValidator` rejects only an empty string. Whitespace is accepted.
`NonblankTextValidator` rejects the empty string and strings made entirely of
Unicode 17.0 `White_Space` characters. The set is pinned from the official
[Unicode 17.0 PropList.txt](https://www.unicode.org/Public/17.0.0/ucd/PropList.txt)
and includes tabs, line separators, and spaces such as U+00A0 and U+3000. It
does not treat U+200B ZERO WIDTH SPACE as whitespace.

`TextLengthValidator` counts Unicode scalar values, not bytes or grapheme
clusters. A base letter followed by a combining accent counts as two. It
performs no normalization. `TextByteLimitValidator` checks UTF-8 byte length
separately and reports `TOO_LONG` with a `maximum_bytes` parameter.

`EmailValidator` is an ASCII syntax screen, not mailbox verification. It
accepts a dot-atom local part up to 64 bytes and a dotted domain with at least
two labels, each at most 63 bytes; the whole address is at most 254 bytes.
Domain labels must start and end with an ASCII letter or digit, with hyphens
allowed internally. It rejects quoted local parts, address literals,
internationalized addresses, and single-label domains. It cannot establish
that a mailbox exists or can receive mail.

## Regular expressions and untrusted input

Patterns are rule data, never source code. The library must not evaluate text
as Lua, shell, SQL, or another language to validate it.

`CompiledPattern` uses [RE2](https://github.com/google/re2) in UTF-8 mode. RE2
avoids exponential backtracking and has matching time linear in input size for
a fixed compiled pattern. Patterns are limited to 4 KiB by default and 64 KiB
at most. Input uses the same default 64 KiB and hard 1 MiB text byte limits as
other text rules. RE2 is configured with an 8 MiB budget for its compiled
program and DFA caches; this is not a cap on every allocation made by the
process.

Regex compilation reports `INVALID_RULE` for unsupported syntax or malformed
expressions; a compiled program that exceeds RE2's internal limit returns
`LIMIT_EXCEEDED`. The API returns only a boolean match result, without
captures. Full mode requires a match of the whole input; partial mode looks
for a matching substring, subject to anchors in the pattern. RE2 does not
support look-around or backreferences; see its
[syntax reference](https://github.com/google/re2/wiki/Syntax). Compiled
patterns can be reused for concurrent validation. Callers must keep a pattern
alive until every validation using it has finished.

Patterns are rule data, never executable code. The library does not evaluate
text as Lua, shell, SQL, or another language. Validation does not replace
parameterized SQL, safe process APIs, HTML escaping, or context-specific
output encoding. Passing a validator does not make input safe for those
contexts.

## Composable rules and factories

The C++ API provides small primitive validators and immutable rule composition.
The convenience factories delegate to those validators rather than implement
separate checks. `Validators::Numbers::with(value)` stores a numeric snapshot;
`Validators::Text::with(input)` copies the UTF-8 input into immutable owned
storage. Compiled pattern handles are shareable, so a rule chain retains its
compiled expression even if the caller releases its own handle.

The fluent API supports this shape:

```cpp
auto parsed = Validators::Numbers::parseInteger<int>(input);
if (parsed) {
    auto result = Validators::Numbers::with(parsed.value)
        .isInRange(2, 10)
        .validate("profile.count");
}

auto result = Validators::Text::with(input)
    .isNotEmpty()
    .hasLength(3, 80)
    .matches(compiled_pattern)
    .validate("profile.name");
```

Text length constraints are named separately from numeric value ranges.
Factories can validate into a caller-owned report with
`validate(report, field)`, or create a sealed `ValidationResult` with
`validate(field)`. The result exposes the operation status, validity, and
structured issues.

Validation failures accumulate in declaration order. A rule's operation error
such as `INVALID_RULE`, `LIMIT_EXCEEDED`, or a sealed report stops later rules;
ordinary user-input failures remain in the report and do not stop independent
checks. Parsing remains a separate operation, so callers run value rules only
after parsing succeeds. Rule chains are immutable and reusable. A custom C++
callback extension point may be considered after the built-in rule model is
stable; arbitrary Lua callbacks are not part of the initial data-only
integration.

## C and C++ API boundary

The implementation should be C++ behind a versioned C-compatible API, with a
thin C++ wrapper for templates, RAII, and fluent composition. Both interfaces
must use the same validation behavior.

The C API should use fixed-width types, explicit ownership, opaque handles only
where a compiled rule or report needs retained lifetime, and caller-readable
results. Every entry point catches implementation exceptions. No C++ exception
may cross the C ABI. Public headers must not expose backend or UI types.

The standalone Meson package installs its headers and pkg-config metadata.
RE2 is a required Meson dependency discovered through pkg-config and is listed
as a private pkg-config dependency because its types do not appear in public
headers. The pinned Unicode whitespace table avoids a runtime ICU dependency.

The first ABI version provides a bounded, explicitly sealed report builder.
Reports contain stable issue codes, optional UTF-8 field paths, and named
typed parameters. Field paths and parameter names are UTF-8 without U+0000;
text parameter values are UTF-8 and may contain U+0000. Report issues are
appended in declaration order. Report mutation is single-owner and must not
race with reads; once sealed, a report can be shared for concurrent reads. C
handles are explicitly retained and released. An add operation either appends
the complete issue or leaves the report unchanged. Defaults are 64 issues, 8
parameters per issue, and 1 MiB of aggregate field/name/text bytes. Hard limits
are 256 issues, 16 parameters per issue, and 16 MiB of aggregate text bytes;
callers may lower limits when creating a report.

## State, forms, and Lua

`plainroot-state` remains independent. Its per-update check callback can call
validation, but callers that commit directly can bypass that check. Therefore
validation must not be described as a store-wide invariant until state offers
an explicit invariant policy. Application command functions should own
validated writes in the initial integration.

Raw form drafts may be incomplete or invalid while the user edits. A later
form/controller layer should track raw text, parsed values, baseline, dirty and
touched status, issues, and asynchronous validation generations. These are
form concerns, not validator state.

Lua exposure follows a stable C++/C API and CLI demonstration. It should expose
allowlisted rule names and structured results, not evaluate strings as
programs. The validator project itself remains Lua-independent; an adapter
belongs in a consumer or a separate optional integration target.

## Testing contract

Each implementation phase adds tests at the public boundary. Tests should
cover success and failure, boundary values, malformed rule configuration,
stable issue order, resource limits, and the distinction between parsing and
constraints. The roadmap lists phase-specific acceptance checks.
