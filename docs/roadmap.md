# plainroot-validation Roadmap

This roadmap sequences the independent validation library from settled
contracts to reusable CLI, C++, C, and Lua-facing APIs. Each phase adds tests
for its public behavior before the next layer is built.

## Project boundary

`plainroot-validation` validates typed values and parses text. It is independent
of UI, toolkit, state, Lua, settings, configuration, and persistence. C++
templates and fluent factories compose a shared implementation behind a
versioned C ABI.

The library returns stable issue codes and typed parameters. Consumers own
localization and presentation. Validation does not encode or escape text for
shell, SQL, HTML, Lua, or other output contexts.

## Delivery phases

### 0. Settle the contract — complete

- Keep parsing, validation, and normalization as separate operations.
- Define structured reports, issue ordering, maximum result size, and rule
  configuration errors.
- Define typed numeric range semantics and floating-point handling.
- Define UTF-8 validity, human-oriented length units, number grammar, boolean
  tokens, Unicode whitespace version, email syntax screen, and regex engine.
- Confirm LGPL-3.0-or-later, symbol versioning, ownership, and C/C++ API shape.

**Exit gate:** complete. Numeric, boolean, Unicode, email, and regex semantics
are documented before their APIs are exposed.

### 1. Standalone project and result foundation — implemented

- Add the Meson project, shared-library target, symbol version map, installable
  public headers, and pkg-config metadata.
- Define stable issue/status codes, structured reports, and rule-construction
  errors.
- Add a versioned C API and a thin C++ RAII result wrapper.
- Add a small CLI example that formats issue codes locally without linking UI
  or toolkit libraries.

**Verification:** C and C++ public-boundary tests exercise report ownership,
issue ordering, stable codes, configured limits, and numeric behavior.
Dedicated ABI and exported-symbol checks remain part of phase 7.

### 2. Typed numeric rules and text parsers — implemented

- Implement templated inclusive ranges for signed/unsigned integer and
  floating-point values.
- Implement finite-number checks and safe bound validation.
- Implement locale-independent integer, unsigned integer, real-number, and
  boolean-text parsing with explicit parser options.
- Keep parse failures distinct from range failures and return the parsed value
  only on successful conversion.

Inclusive signed, unsigned, and floating-point range validators, finite-number
checks, and fixed-width C parsers are implemented. The C++ wrapper supports
standard integer types through 64 value bits and `float`/`double`; it checks
narrow-target conversion ranges after the fixed-width parser succeeds.

**Tests:** minimum/maximum boundaries, inverted bounds, negative and unsigned
inputs, narrow and fixed-width overflow, malformed syntax, trailing
characters, decimal exponent forms, NaN/infinity rejection, subnormal and
underflow behavior, ASCII whitespace options, input-size limits, invalid UTF-8,
and all configured boolean tokens are covered through C and C++ callers.

**Exit gate:** complete. The public C and C++ parser and numeric-rule APIs
build and pass their dedicated test executables.

### 3. Text rules and regex policy — implemented

- Implement required and nonblank checks, with Unicode 17.0 `White_Space`
  semantics.
- Count Unicode scalar values for human-oriented length and provide a separate
  UTF-8 byte limit.
- Add the documented ASCII email syntax screen.
- Add reusable RE2-backed compiled patterns with UTF-8 matching, full and
  partial modes, explicit size limits, and bounded compiled-program memory.
- Keep normalization outside validators.

**Verification:** public C and C++ tests pass for empty and whitespace-only
input, Unicode whitespace and scalar lengths, malformed UTF-8 and byte offsets,
length and byte boundaries, email syntax, invalid patterns, unsupported RE2
syntax, full and partial matching, pattern/input limits, and a nested-repeat
near-match input. RE2 avoids catastrophic backtracking.

**Exit gate:** complete. C and C++ callers use the same text, email, and regex
behavior through the versioned C API.

### 4. Immutable composition and convenience factories — implemented

- Compose independent validators into immutable rule sets.
- Provide `Validators::Numbers` and `Validators::Text` convenience factories
  that delegate to the primitive validators.
- Specify accumulation and short-circuit behavior.
- Support reusable compiled rules without retaining caller-owned input.

**Verification:** public C++ tests cover declaration-order issue accumulation,
operation-error short-circuiting, parse-dependent checks, rule reuse, immutable
builder branches, owned text snapshots, shared compiled patterns, and
equivalent results through primitive and factory entry points.

**Exit gate:** complete. Numeric and text factories delegate to primitive
validators and expose both caller-owned reports and a sealed result wrapper.

### 5. CLI and application command integration — implemented

- Demonstrate typed parsing and composition from a standalone CLI consumer.
- Document how a command validates a candidate before committing a state
  transaction.
- Keep validated writes behind application-owned commands while state has no
  store-wide validation invariant mechanism.

**Tests:** installed CLI consumer and a state-check integration example that
proves rejection leaves the candidate uncommitted. The validation library
itself must still build and run without `plainroot-state`.

The CLI accepts one age value, performs locale-independent integer parsing,
composes an inclusive range check, and formats structured issue codes and
parameters locally. The separate workspace integration consumer depends on
both libraries, stages a candidate through `plainroot-state`, and rejects an
invalid value from the update check callback. Its tests verify that an accepted
candidate advances the revision, while invalid text and an out-of-range value
leave the prior value and revision unchanged. The core validation Meson project
does not discover or require `plainroot-state`.

### 6. Lua adapter — implemented

- Expose allowlisted built-in validators and immutable rule composition to Lua
  after C++ and CLI contracts stabilize.
- Return structured codes and parameters for host localization.
- Keep Lua runtime and execution policy outside the core library. Meson builds
  the adapter as a separate optional Lua 5.4 module.

**Tests:** numeric and text validation, typed report conversion, invalid rule
configuration, input and chain limits, strict option parsing, and proof that
input strings are treated as data rather than executed code. The full project
suite passes with the adapter enabled; a separate core-only build passes with
`-Dlua_adapter=disabled`.

### 7. Release hardening — implemented

- Add opt-in Clang libFuzzer targets for numeric parsing, UTF-8 text checks,
  and bounded regex compilation and matching. Meson runs short sanitizer-backed
  fuzz smoke passes when fuzzing is enabled.
- Document AddressSanitizer and UBSan build commands and ABI compatibility
  checks.
- Document supported grammars, Unicode behavior, regex resource limits, and
  application-owned localization of stable issue codes and typed parameters.
- Verify public C symbols and ELF versioning, forbidden runtime dependencies,
  installed headers, pkg-config metadata, and independent installed C and C++
  consumers. The test compares symbols with a frozen ABI 0.1 manifest as well
  as the declarations in the public header, and compiles frozen C struct-size
  and field-offset assertions for the 64-bit ABI.

**Verification:** the default suite, core-only suite, opt-in fuzz smoke suite,
ABI export audit, and staged installed-consumer checks pass. Sanitizer builds
use Clang's `-Db_sanitize=address,undefined -Db_lundef=false` and omit the
separate staged-consumer suite, whose consumer must be linked with that same
sanitizer runtime. In ptrace-based runners, set `ASAN_OPTIONS=detect_leaks=0`
because LeakSanitizer cannot inspect those processes; address and undefined
behavior checks remain active. Fuzz targets additionally require Clang's
libFuzzer.

**Exit gate:** complete. C, C++, CLI, and optional Lua consumers observe the
same stable results, and no consumer requires UI or state dependencies merely
to validate input.

## Dependency rules

- Core numeric and text validation has no UI, toolkit, state, Lua, D-Bus, or
  persistence dependency.
- Any regex or Unicode dependency must be justified by the contract and
  documented in Meson options and package metadata.
- Lua integration is optional and follows the stable library API.
- A future form/controller may depend on both validation and state; neither
  library depends on the form/controller.
