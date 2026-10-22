#!/bin/sh
set -eu

cli=$1

valid_output=$("$cli" ' 42 ' 2>&1)
if [ "$valid_output" != 'valid profile.age=42' ]; then
  printf 'unexpected valid CLI output: %s\n' "$valid_output" >&2
  exit 1
fi

set +e
range_output=$("$cli" 200 2>&1)
range_status=$?
parse_output=$("$cli" not-a-number 2>&1)
parse_status=$?
usage_output=$("$cli" 2>&1)
usage_status=$?
set -e

if [ "$range_status" -ne 2 ] ||
   ! printf '%s\n' "$range_output" | grep -Fq 'out-of-range field=profile.age minimum=0 maximum=130'; then
  printf 'unexpected out-of-range CLI result (%s): %s\n' \
    "$range_status" "$range_output" >&2
  exit 1
fi

if [ "$parse_status" -ne 2 ] ||
   [ "$parse_output" != 'invalid-integer field=profile.age' ]; then
  printf 'unexpected parse CLI result (%s): %s\n' \
    "$parse_status" "$parse_output" >&2
  exit 1
fi

if [ "$usage_status" -ne 64 ] ||
   [ "$usage_output" != 'usage: plainroot-validation-cli AGE' ]; then
  printf 'unexpected CLI usage result (%s): %s\n' \
    "$usage_status" "$usage_output" >&2
  exit 1
fi
