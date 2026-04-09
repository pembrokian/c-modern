#!/bin/sh

set -eu

usage() {
  cat <<'EOF'
usage: prune_legacy_build_debris.sh [--source-root <path>] [--build-dir <path>]

Prune obsolete build-root residue after the semantic layout migration.

This removes:
- root-level binaries and static libraries that now live under bin/ and lib/
- stale renamed mc_phase7_tool_* binaries
- legacy top-level phase*, stage*, and tmp_* entries

This preserves:
- active CMake metadata and canonical layout roots
- manual, probe, repro, and check paths

Options:
  --source-root <path>  Repository root. Defaults to the current directory.
  --build-dir <path>    Primary binary dir. Defaults to <source-root>/build/debug.
  --help                Show this help text.
EOF
}

die() {
  printf '%s\n' "$1" >&2
  exit 1
}

SOURCE_ROOT="."
BUILD_DIR=""

while [ "$#" -gt 0 ]; do
  case "$1" in
    --source-root)
      [ "$#" -ge 2 ] || die "missing value for --source-root"
      SOURCE_ROOT="$2"
      shift 2
      ;;
    --build-dir)
      [ "$#" -ge 2 ] || die "missing value for --build-dir"
      BUILD_DIR="$2"
      shift 2
      ;;
    --help)
      usage
      exit 0
      ;;
    *)
      die "unknown argument: $1"
      ;;
  esac
done

[ -d "$SOURCE_ROOT" ] || die "source root does not exist: $SOURCE_ROOT"
SOURCE_ROOT=$(cd "$SOURCE_ROOT" && pwd)

if [ -z "$BUILD_DIR" ]; then
  BUILD_DIR="$SOURCE_ROOT/build/debug"
fi

[ -d "$BUILD_DIR" ] || die "build directory does not exist: $BUILD_DIR"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
BUILD_ROOT="$SOURCE_ROOT/build"

remove_path() {
  path="$1"
  if [ -e "$path" ]; then
    printf 'removing %s\n' "$path"
    rm -rf "$path"
  fi
}

is_preserved_entry() {
  name="$1"
  case "$name" in
    .cmake|CMakeCache.txt|CMakeFiles|CTestTestfile.cmake|Makefile|Testing|cmake_install.cmake|compile_commands.json|audit|bin|build|cmake-debug|codegen|debug|dumps|dumps_audit|lib|mci|probes|release|tmp|tool)
      return 0
      ;;
    *manual*|*probe*|*repro*|*check*)
      return 0
      ;;
  esac
  return 1
}

prune_relocated_outputs() {
  dir="$1"
  [ -d "$dir" ] || return 0

  if [ -x "$dir/bin/mc" ] && [ -f "$dir/mc" ]; then
    remove_path "$dir/mc"
  fi

  for path in "$dir"/mc_*_tests; do
    [ -e "$path" ] || continue
    name=$(basename "$path")
    if [ -x "$dir/bin/$name" ]; then
      remove_path "$path"
    fi
  done

  if [ -e "$dir/mc_codegen_executable_tests" ]; then
    remove_path "$dir/mc_codegen_executable_tests"
  fi

  for path in "$dir"/libmc_*.a; do
    [ -e "$path" ] || continue
    name=$(basename "$path")
    if [ -f "$dir/lib/$name" ]; then
      remove_path "$path"
    fi
  done

  for path in "$dir"/mc_phase7_tool_*; do
    [ -e "$path" ] || continue
    remove_path "$path"
  done
}

prune_legacy_named_entries() {
  dir="$1"
  [ -d "$dir" ] || return 0

  for path in "$dir"/* "$dir"/.*; do
    [ -e "$path" ] || continue
    name=$(basename "$path")
    case "$name" in
      .|..)
        continue
        ;;
    esac

    if is_preserved_entry "$name"; then
      continue
    fi

    case "$name" in
      phase*|stage*|tmp_*)
        remove_path "$path"
        ;;
    esac
  done
}

prune_relocated_outputs "$BUILD_ROOT"
prune_legacy_named_entries "$BUILD_ROOT"

if [ "$BUILD_DIR" != "$BUILD_ROOT" ]; then
  prune_relocated_outputs "$BUILD_DIR"
  prune_legacy_named_entries "$BUILD_DIR"
fi