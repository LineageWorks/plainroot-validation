# plainroot-validation

`plainroot-validation` is an independent C++ library with a versioned C API
for reusable input parsing and validation. It does not depend on the UI,
toolkit, state store, Lua, settings service, or an event loop.

## Current status

The implementation provides the shared report foundation, typed numeric
rules and parsers, UTF-8 text validation, ASCII email syntax screening, and
compiled RE2 patterns. It includes stable issue codes, named typed parameters,
explicit C handle lifetime, bounded report/input/pattern sizes, inclusive
numeric ranges, finite-number checks, required and nonblank checks, Unicode
scalar length, byte limits, and full or partial regex matching. Immutable C++
rule composition is available through numeric and text factories. The
installable CLI demonstrates typed parsing, range composition, and local
formatting of structured issues. A separate workspace integration example
demonstrates validation in an application-owned state update command.

The library installs C and C++ headers and pkg-config metadata. The workspace
build includes this independent target. A small CLI example shows how a
consumer can format issue codes without UI or toolkit dependencies.
RE2 is required and detected with pkg-config; it is kept out of the public
headers. Unicode whitespace uses a pinned Unicode 17.0 property table rather
than an ICU runtime dependency.

## Build

From the `plainroot-desktop` workspace:

```sh
./scripts/workspace.sh build
```

Or build the standalone project:

```sh
meson setup build plainroot-validation
meson compile -C build
```

Run the CLI with one age value; it accepts the inclusive range 0 through 130:

```sh
build/plainroot-validation/plainroot-validation-cli ' 42 '
build/plainroot-validation/plainroot-validation-cli 200
```

The first command prints `valid profile.age=42`. The second reports an
`out-of-range` issue and exits with status 2. Malformed input reports its
stable parse issue code. The CLI is installed as
`$prefix/bin/plainroot-validation-cli` with the validation library.

`./scripts/workspace.sh test` also builds and runs the separate
`examples/state-integration` consumer. It links both `plainroot-validation`
and `plainroot-state`; the validation library's own Meson project has no state
dependency and remains buildable on its own.

## C++ report example

```cpp
#include <plainroot/validation/validation.hpp>

plainroot::validation::Report report;
if (plainroot::validation::Report::create(report) == PLAINROOT_VALIDATION_OK) {
  plainroot::validation::Issue issue;
  issue.code = PLAINROOT_VALIDATION_ISSUE_OUT_OF_RANGE;
  issue.field = "profile.age";
  issue.parameters = {{"minimum", int64_t{18}}, {"maximum", int64_t{120}}};
  report.add_issue(issue);
  report.seal();
}
```

Callers own localization and presentation. The library returns machine-readable
codes and typed parameters, not translated messages. Parsing, validation, and
normalization are separate operations; validators never silently transform
input.

## Numeric parsing example

```cpp
using namespace plainroot::validation;

ParseOptions options;
options.trim_ascii_whitespace = true;
const auto age = parse_integer<int>(" 42 ", options);
if (age) {
  Report report;
  if (Report::create(report) == PLAINROOT_VALIDATION_OK) {
    RangeValidator<int>{0, 130}.validate(age.value, report, "profile.age");
    report.seal();
  }
}
```

Parsers use ASCII syntax independent of process locale. They do not trim
whitespace unless explicitly enabled. Boolean aliases and case-insensitive
matching are opt-in through `ParseOptions`.

## Composed validation example

```cpp
using namespace plainroot::validation;

const auto age = Validators::Numbers::parseInteger<int>("42");
if (age) {
  const auto result = Validators::Numbers::with(age.value)
                          .isInRange(0, 130)
                          .validate("profile.age");
  if (!result.valid()) {
    // Inspect result.issue_count() and result.issue() for stable codes.
  }
}

CompiledPattern account_pattern;
CompiledPattern::compile(R"(^[a-z0-9_]{3,24}$)", account_pattern);
const auto account = Validators::Text::with("alex_42")
                         .isNotEmpty()
                         .hasLength(3, 24)
                         .matches(account_pattern)
                         .validate("profile.account");
```

Factory chains are immutable snapshots. Text factories copy their input,
numeric factories store values by value, and copied compiled-pattern handles
share the already-compiled expression. Validation failures accumulate in
declaration order. An operation error such as an invalid rule configuration
stops later rules and is returned in `ValidationResult::status()`. Parsing
remains separate: run value constraints only when the parse result succeeds.

## Validate before a state update

Application commands own the path from raw input to state mutation. Parse text
first, then use `Store::update()` to stage the typed candidate and validate the
staged value in the update check callback. Returning
`PLAINROOT_STATE_REJECTED` discards that draft without changing the store's
value or revision. The integration example tests both accepted input and a
range rejection against a pre-existing value.

This is an application command boundary, not a store-wide invariant: code that
can access the store may still commit a direct transaction. Consumers that
require validation must route all writes to the relevant values through their
owned command or service interface. Validation also does not make a value safe
to embed in shell commands, SQL, HTML, or another output context.

## Design documents

- [Validation contract](docs/validation-contract.md): boundaries, result
  semantics, parsing, Unicode, composition, and security requirements.
- [Implementation roadmap](docs/roadmap.md): ordered phases and remaining
  verification.
- [Project-family roadmap](../plainroot-docs/plainroot-validation-roadmap.md):
  how validation fits with state, CLI, Lua, and form handling in PlainrootDE.

## License

Copyright © 2026 Alexei Nasra. This project is licensed under the GNU Lesser
General Public License, version 3 or any later version. See [LICENSE](LICENSE)
and [COPYING](COPYING).
