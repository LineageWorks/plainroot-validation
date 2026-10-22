# Validation and state command example

This small consumer shows an application-owned command that parses text,
stages the typed value with `plainroot-state`, and checks the staged value with
`plainroot-validation` before the update can commit.

The integration is intentionally a separate Meson project. It links both
libraries, while `plainroot-validation/meson.build` depends only on validation
and its parser/regex dependencies.

From the `plainroot-desktop` workspace, run:

```sh
./scripts/workspace.sh test
```

The workspace configures this example against the uninstalled package metadata
for both local libraries and runs cases for valid input, malformed input, and
an out-of-range candidate. The rejection cases verify that the stored value
and revision remain unchanged. The command boundary is application-owned;
direct state transactions can still bypass it.
