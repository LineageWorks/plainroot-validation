#!/bin/sh
set -eu

command=$1

accepted=$("$command" 42 2>&1)
if [ "$accepted" != 'accepted profile.age=42 revision=2' ]; then
  printf 'unexpected accepted command result: %s\n' "$accepted" >&2
  exit 1
fi

set +e
range_rejected=$("$command" 200 2>&1)
range_status=$?
parse_rejected=$("$command" not-a-number 2>&1)
parse_status=$?
set -e

if [ "$range_status" -ne 2 ] ||
   [ "$range_rejected" != 'rejected out-of-range field=profile.age minimum=0 maximum=130 stored=30 revision=1' ]; then
  printf 'unexpected range rejection (%s): %s\n' \
    "$range_status" "$range_rejected" >&2
  exit 1
fi

if [ "$parse_status" -ne 2 ] ||
   [ "$parse_rejected" != 'rejected invalid-integer field=profile.age stored=30 revision=1' ]; then
  printf 'unexpected parse rejection (%s): %s\n' \
    "$parse_status" "$parse_rejected" >&2
  exit 1
fi
