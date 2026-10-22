#!/bin/sh
set -eu

library=$1
public_header=$2
nm=$3
readelf=$4
abi_baseline=$5
temporary_directory=$(mktemp -d)
trap 'rm -rf "$temporary_directory"' EXIT HUP INT TERM

expected_symbols="$temporary_directory/expected-symbols"
actual_symbols="$temporary_directory/actual-symbols"
unexpected_symbols="$temporary_directory/unexpected-symbols"
unexpected_dependencies="$temporary_directory/unexpected-dependencies"

grep -Eho 'plainroot_validation_[[:alnum:]_]+_v1[[:space:]]*\(' "$public_header" \
  | sed -E 's/[[:space:]]*\($//' \
  | sort -u > "$expected_symbols"

"$nm" -D --defined-only "$library" | awk '
  $3 ~ /^plainroot_validation_/ {
    symbol = $3
    sub(/@@.*/, "", symbol)
    print symbol
  }
' | sort -u > "$actual_symbols"

if ! diff -u "$expected_symbols" "$actual_symbols"; then
  echo "exported symbols differ from the public C header" >&2
  exit 1
fi

if ! diff -u "$abi_baseline" "$actual_symbols"; then
  echo "exported symbols differ from the frozen PLAINROOT_VALIDATION_0.1 ABI" >&2
  exit 1
fi

"$nm" -D --defined-only "$library" | awk '
  $3 ~ /^plainroot_validation_/ && $3 !~ /@@PLAINROOT_VALIDATION_0[.]1$/ {
    print "unversioned or unexpectedly versioned public symbol: " $3
    failed = 1
  }
  $3 !~ /^plainroot_validation_/ && $3 != "PLAINROOT_VALIDATION_0.1" {
    print "unexpected dynamic export: " $3
    failed = 1
  }
  END { exit failed }
' > "$unexpected_symbols" || {
  cat "$unexpected_symbols" >&2
  exit 1
}

"$readelf" --version-info "$library" | grep -Fq 'PLAINROOT_VALIDATION_0.1' || {
  echo "ELF symbol version PLAINROOT_VALIDATION_0.1 is missing" >&2
  exit 1
}

"$readelf" -d "$library" | grep -F 'SONAME' \
  | grep -Fq 'libplainroot-validation.so.0' || {
    echo "expected library SONAME libplainroot-validation.so.0" >&2
    exit 1
  }

"$readelf" -d "$library" | awk '
  /NEEDED/ && /lib(lua|wayland|vulkan|gtk|plainroot-(ui|toolkit|state))/ {
    print "forbidden dependency in validation core: " $0
    failed = 1
  }
  END { exit failed }
' > "$unexpected_dependencies" || {
  cat "$unexpected_dependencies" >&2
  exit 1
}

echo "ABI exports match the public header and frozen manifest; version and dependency checks pass."
