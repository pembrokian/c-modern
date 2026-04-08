#!/bin/sh

set -eu

usage() {
  cat <<'EOF'
usage: prepare_v0_2_snapshot.sh [--source-root <path>] [--build-dir <path>] [--allow-dirty]

Reruns the repo-owned mc_v0_2_gate, verifies committed-state release
preparation policy, and writes a v0.2 snapshot report without creating the tag.

Options:
  --source-root <path>  Repository root. Defaults to the current directory.
  --build-dir <path>    CMake build directory. Defaults to <source-root>/build/debug.
  --allow-dirty         Skip the clean-worktree check. Intended only for repo-owned audits.
  --help                Show this help text.
EOF
}

die() {
  printf '%s\n' "$1" >&2
  exit 1
}

SOURCE_ROOT="."
BUILD_DIR=""
ALLOW_DIRTY=0

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
    --allow-dirty)
      ALLOW_DIRTY=1
      shift
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

command -v git >/dev/null 2>&1 || die "git is required"
command -v ctest >/dev/null 2>&1 || die "ctest is required"

git -C "$SOURCE_ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1 || \
  die "source root is not inside a git worktree: $SOURCE_ROOT"

printf 'Running mc_v0_2_gate from %s\n' "$BUILD_DIR"
ctest --test-dir "$BUILD_DIR" -R '^mc_v0_2_gate$' --output-on-failure

if [ "$ALLOW_DIRTY" -eq 0 ]; then
  if [ -n "$(git -C "$SOURCE_ROOT" status --porcelain --untracked-files=normal)" ]; then
    die "release snapshot preparation requires a clean committed worktree"
  fi
fi

COMMIT_HASH=$(git -C "$SOURCE_ROOT" rev-parse HEAD)
SHORT_HASH=$(git -C "$SOURCE_ROOT" rev-parse --short HEAD)
REPORT_DIR="$BUILD_DIR/release"
REPORT_PATH="$REPORT_DIR/v0.2-snapshot.txt"

mkdir -p "$REPORT_DIR"

cat > "$REPORT_PATH" <<EOF
Modern C v0.2 Release Snapshot
==============================

source_root = $SOURCE_ROOT
build_dir = $BUILD_DIR
commit = $COMMIT_HASH
short_commit = $SHORT_HASH
gate = mc_v0_2_gate
clean_worktree_required = yes
audit_allow_dirty = $( [ "$ALLOW_DIRTY" -eq 1 ] && printf yes || printf no )

Suggested tag command:
git -C "$SOURCE_ROOT" tag -a v0.2 $COMMIT_HASH -m "v0.2"

Suggested push command:
git -C "$SOURCE_ROOT" push origin v0.2
EOF

printf 'Release snapshot candidate commit: %s\n' "$COMMIT_HASH"
printf 'Snapshot report: %s\n' "$REPORT_PATH"
printf 'Suggested tag command:\n'
printf 'git -C "%s" tag -a v0.2 %s -m "v0.2"\n' "$SOURCE_ROOT" "$COMMIT_HASH"
printf 'Suggested push command:\n'
printf 'git -C "%s" push origin v0.2\n' "$SOURCE_ROOT"

if [ "$ALLOW_DIRTY" -eq 1 ]; then
  printf 'Audit mode: dirty-worktree check was skipped.\n'
fi
