# plainroot-validation

`plainroot-validation` is planned as an independent library for validating
typed values and parsing user-provided text. It is intended for graphical
applications, command-line tools, services, and other consumers.

The library will not depend on `plainroot-ui`, `plainroot-toolkit`,
`plainroot-state`, Lua, or a settings service. Callers may use its results in a
UI or before a state update, while the validation library itself remains
usable in a synchronous CLI.

## Project status

This repository currently contains the design contract and implementation
roadmap. The public API and build target have not been implemented. The
roadmap defines the acceptance decisions and staged delivery gates before the
first ABI is published.

## Design documents

- [Validation contract](docs/validation-contract.md): boundaries, result
  semantics, parsing, composition, and security requirements.
- [Implementation roadmap](docs/roadmap.md): ordered phases and acceptance
  checks.
- [Project-family roadmap](../plainroot-docs/plainroot-validation-roadmap.md):
  how the library fits with state, CLI, Lua, and form handling in PlainrootDE.

## Intended API shape

The following examples illustrate the intended C++ API; they are not available
in this design-stage repository yet:

```cpp
auto typed = RangeValidator<int>{2, 10}.validate(value);
auto parsed = Validators::Numbers::parseInteger<int>(text);

auto report = Validators::Text::with(email)
    .isNotEmpty()
    .isEmail()
    .validate();
```

Typed checks, text parsing, and constraints on parsed values will be separate
operations. Validation reports will use stable issue codes and parameters;
applications will choose how to localize and present them.

## License

Copyright © 2026 Alexei Nasra. This project is licensed under the GNU Lesser
General Public License, version 3 or any later version. See [LICENSE](LICENSE)
and [COPYING](COPYING).
