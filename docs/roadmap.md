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
- Define UTF-8 validity, human-oriented length units, number grammar, and
  boolean tokens. Defer Unicode whitespace implementation to phase 3 with a
  pinned Unicode data version.
- Defer regex support from the version 1 ABI until a bounded-time engine and
  dependency policy are selected.
- Confirm LGPL-3.0-or-later, symbol versioning, ownership, and C/C++ API shape.

**Exit gate:** complete. Numeric and boolean parsing semantics are documented;
regex and Unicode whitespace behavior are explicitly deferred out of the
initial ABI.

### 1. Standalone project and result foundation — implemented

- Add the Meson project, shared-library target, symbol version map, installable
  public headers, and pkg-config metadata.
- Define stable issue/status codes, structured reports, and rule-construction
  errors.
- Add a versioned C API and a thin C++ RAII result wrapper.
- Add a small CLI example that formats issue codes locally without linking UI
  or toolkit libraries.

**Verification still needed:** C and C++ ABI consumers, report
ownership/lifetime, stable issue codes, deterministic issue ordering, size
limits, and exported-symbol checks. This phase has not run a test suite yet.

### 2. Typed numeric rules and text parsers

- Implement templated inclusive ranges for signed/unsigned integer and
  floating-point values.
- Implement finite-number checks and safe bound validation.
- Implement locale-independent integer, unsigned integer, real-number, and
  boolean-text parsing with explicit parser options.
- Keep parse failures distinct from range failures and return the parsed value
  only on successful conversion.

**Tests:** minimum/maximum boundaries, inverted bounds, zero, negative and
unsigned inputs, overflow, malformed syntax, trailing characters, exponent
policy, NaN/infinity, and every configured boolean token.

### 3. Text rules and regex policy

- Implement UTF-8 validity, required/nonblank text, byte limits, and the
  agreed human-oriented length rule.
- Implement the agreed email syntax check and stable pattern mismatch results.
- Add a compiled regex/rule representation with bounded pattern and input
  sizes.
- Keep normalization outside validators.

**Tests:** empty and whitespace-only input, multibyte strings, invalid UTF-8,
length boundaries, malformed patterns, adversarial patterns/inputs, and input
size limits. Security tests must show the selected regex policy avoids
unbounded work.

### 4. Immutable composition and convenience factories

- Compose independent validators into immutable rule sets.
- Provide `Validators::Numbers` and `Validators::Text` convenience factories
  that delegate to the primitive validators.
- Specify accumulation and short-circuit behavior.
- Support reusable compiled rules without retaining caller-owned input.

**Tests:** composition order, all-independent-errors behavior, dependent-rule
short-circuiting, rule reuse, immutable configuration, and equivalent results
through primitive and factory entry points.

### 5. CLI and application command integration

- Demonstrate typed parsing and composition from a standalone CLI consumer.
- Document how a command validates a candidate before committing a state
  transaction.
- Keep validated writes behind application-owned commands while state has no
  store-wide validation invariant mechanism.

**Tests:** installed CLI consumer and a state-check integration example that
proves rejection leaves the candidate uncommitted. The validation library
itself must still build and run without `plainroot-state`.

### 6. Lua adapter

- Expose allowlisted built-in validators and immutable rule composition to Lua
  after C++ and CLI contracts stabilize.
- Return structured codes and parameters for host localization.
- Keep Lua runtime and execution policy outside the core library.

**Tests:** representative numeric/text validation, report conversion, invalid
rule configuration, resource limits, and proof that input strings are treated
as data rather than executed code.

### 7. Release hardening

- Add fuzzing for text parsers and UTF-8 handling.
- Add sanitizer and ABI compatibility builds.
- Document supported grammars, Unicode behavior, regex resource limits, and
  localization guidance.
- Verify installed headers, versioned symbols, pkg-config usage, and dependency
  boundaries with independent consumers.

**Release gate:** C, C++, CLI, and optional Lua consumers observe the same
stable results, and no consumer requires UI or state dependencies merely to
validate input.

## Dependency rules

- Core numeric and text validation has no UI, toolkit, state, Lua, D-Bus, or
  persistence dependency.
- Any regex or Unicode dependency must be justified by the contract and
  documented in Meson options and package metadata.
- Lua integration is optional and follows the stable library API.
- A future form/controller may depend on both validation and state; neither
  library depends on the form/controller.
