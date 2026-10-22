# Validation Contract

This document defines the intended behavior of `plainroot-validation` before
its public API is implemented. A decision marked **Open** must be resolved and
recorded before it becomes part of the versioned ABI.

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

Numeric validators should use C++ templates constrained to numeric types.
Boolean and character types must not be accepted as ordinary numeric types.
Version one should cover standard signed and unsigned integers and floating
point types, with a fixed-width C interface underneath the C++ templates.

Range bounds are inclusive by default. The caller must be able to request
exclusive bounds explicitly if a consumer needs them. A range whose minimum is
greater than its maximum is a rule-construction error.

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
locale. The exact accepted grammar, whitespace handling, exponent behavior,
and localized-input policy are **Open** decisions for the foundation phase.

Boolean text parsing must use an explicit set of accepted tokens and a
documented case policy. The default token set should be small and predictable;
aliases such as `yes/no` or `1/0` should require an explicit option. Parsing
returns the parsed boolean so callers need not validate against one expected
value just to convert the text.

## Text validation and Unicode

Text inputs are UTF-8. Invalid UTF-8 must produce a stable issue instead of
being silently repaired. Validation never modifies the input.

Resource limits should be measured in bytes and enforced before expensive
rules. Human-oriented length constraints should document their unit. The
recommended initial length unit is Unicode scalar values, with byte limits as
a separate safety bound. Grapheme-cluster counting requires a Unicode data
dependency and is deferred until a real consumer establishes that need.

Required text and nonblank text should be separate rules. The precise Unicode
whitespace definition for nonblank input is an **Open** implementation choice.
Email validation should be documented as syntax screening only; it cannot
prove mailbox existence or delivery. Internationalized email support and its
normalization rules are also **Open** decisions.

## Regular expressions and untrusted input

Patterns are rule data, never source code. The library must not evaluate text
as Lua, shell, SQL, or another language to validate it.

Invalid patterns produce a rule-construction error. Regex matching must have
explicit pattern and input size limits. Before publishing a regex API, choose
and document an engine and its worst-case behavior. Do not expose an
unbounded-backtracking engine to patterns or inputs that may be untrusted.
This engine choice is an **Open** decision for the foundation phase.

Validation does not replace parameterized SQL, safe process APIs, HTML
escaping, or context-specific output encoding. Passing a validator does not
make input safe for those contexts.

## Composable rules and factories

The C++ API should provide small validators and immutable rule composition.
The convenience factories should delegate to those validators rather than
implementing separate checks.

An intended shape is:

```cpp
auto parsed = Validators::Numbers::parseInteger<int>(input);
if (parsed) {
    auto report = RangeValidator<int>{2, 10}.validate(parsed.value());
}

auto report = Validators::Text::with(input)
    .isNotEmpty()
    .hasLength(3, 80)
    .matches(compiled_pattern)
    .validate();
```

Names and exact syntax remain subject to the roadmap's API review. In
particular, text length constraints must not be named or overloaded as
numeric value ranges.

Composition should declare whether it accumulates all independent failures or
short-circuits. Rule objects should be immutable after construction and safe
to reuse. A custom C++ callback extension point may be considered after the
built-in rule model is stable; arbitrary Lua callbacks are not part of the
initial data-only integration.

## C and C++ API boundary

The implementation should be C++ behind a versioned C-compatible API, with a
thin C++ wrapper for templates, RAII, and fluent composition. Both interfaces
must use the same validation behavior.

The C API should use fixed-width types, explicit ownership, opaque handles only
where a compiled rule or report needs retained lifetime, and caller-readable
results. Every entry point catches implementation exceptions. No C++ exception
may cross the C ABI. Public headers must not expose backend or UI types.

The standalone Meson package should install its headers and pkg-config
metadata. Keep runtime dependencies small. A third-party regex or Unicode
dependency must have a documented reason and build-time detection policy.

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
