#!/usr/bin/env bash
# Run haste benchmarks against the freshly built yksompp+yk binary.
# Assumes yk-build/SOM++ exists (run scripts/yk/build-and-test.sh first).
#
# Usage: scripts/yk/bench.sh [comment]

set -eux

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
COMMENT="${1:-ci}"
WORK="$REPO_ROOT/.bench-work"
SOMPP="$REPO_ROOT/yk-build/SOM++"
SOM_LIB="$REPO_ROOT/Smalltalk"

[ -x "$SOMPP" ] || { echo "missing $SOMPP — run build-and-test.sh first" >&2; exit 1; }
mkdir -p "$WORK"

# Install cargo if missing (only reachable in the CI container).
if ! command -v cargo >/dev/null; then
  [ "$(id -u)" = 0 ] && apt-get update && apt-get install -y --no-install-recommends ca-certificates curl
  curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs \
    | sh -s -- -y --default-toolchain stable --profile minimal
  . "$HOME/.cargo/env"
fi

command -v haste >/dev/null || cargo install --git https://github.com/ykjit/haste
[ -d "$WORK/yk-benchmarks" ] || git clone --depth 1 --recurse-submodules --shallow-submodules \
  https://github.com/ykjit/yk-benchmarks "$WORK/yk-benchmarks"

# Derive haste_yksompp_fork.toml from upstream haste_csom.toml. Longer suffix
# first so the shorter placeholder doesn't eat it.
sed -e "s|/path/to/CSOM/Smalltalk|$SOM_LIB|g" \
    -e "s|/path/to/CSOM|$SOMPP|g" \
    -e "s|^csom = |yksompp = |" \
    "$WORK/yk-benchmarks/haste_csom.toml" > "$WORK/yk-benchmarks/haste_yksompp_fork.toml"

cd "$WORK/yk-benchmarks"
haste bench -f ./haste_yksompp_fork.toml -c "$COMMENT" --order declaration
