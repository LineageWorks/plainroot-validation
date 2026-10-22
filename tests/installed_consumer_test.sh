#!/bin/sh
set -eu

build_directory=$1
source_directory=$2
prefix=$3
libdir=$4
temporary_directory=$(mktemp -d)
trap 'rm -rf "$temporary_directory"' EXIT HUP INT TERM

stage_directory="$temporary_directory/stage"
consumer_build_directory="$temporary_directory/consumer-build"
relative_prefix=${prefix#/}
relative_libdir=${libdir#/}
installed_libdir="$stage_directory/$relative_prefix/$relative_libdir"
pkgconfig_directory="$installed_libdir/pkgconfig"

meson install -C "$build_directory" --destdir "$stage_directory"

if [ ! -f "$pkgconfig_directory/plainroot-validation.pc" ]; then
  echo "installed pkg-config metadata was not found at $pkgconfig_directory" >&2
  exit 1
fi

PKG_CONFIG_PATH="$pkgconfig_directory${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}" \
  PKG_CONFIG_SYSROOT_DIR="$stage_directory" \
  pkg-config --print-requires-private plainroot-validation \
  | grep -Fxq re2 || {
    echo "pkg-config metadata does not declare RE2 as a private dependency" >&2
    exit 1
  }

PKG_CONFIG_PATH="$pkgconfig_directory${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}" \
PKG_CONFIG_SYSROOT_DIR="$stage_directory" \
  meson setup "$consumer_build_directory" \
    "$source_directory/tests/installed-consumer"

PKG_CONFIG_PATH="$pkgconfig_directory${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}" \
PKG_CONFIG_SYSROOT_DIR="$stage_directory" \
  meson compile -C "$consumer_build_directory"

PKG_CONFIG_PATH="$pkgconfig_directory${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}" \
PKG_CONFIG_SYSROOT_DIR="$stage_directory" \
LD_LIBRARY_PATH="$installed_libdir" \
  meson test -C "$consumer_build_directory" --print-errorlogs
