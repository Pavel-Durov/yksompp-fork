#!/usr/bin/env bash
# Derive haste_yksompp_fork.toml and run haste bench.
#
# Usage:
#   scripts/yk/bench.sh <sompp-binary> <som-lib-dir> <yk-benchmarks-dir> [comment]
#
# Prereqs: haste on PATH; all three paths exist; sompp binary is executable.

set -eux

if [ $# -lt 3 ]; then
  echo "usage: $0 <sompp-binary> <som-lib-dir> <yk-benchmarks-dir> [comment]" >&2
  exit 2
fi

SOMPP=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
SOM_LIB=$(cd "$2" && pwd)
YK_BENCHMARKS=$(cd "$3" && pwd)
COMMENT="${4:-ci}"

# Derive haste_yksompp_fork.toml from upstream haste_csom.toml. Longer suffix
# first so the shorter placeholder doesn't eat it.
sed -e "s|/path/to/CSOM/Smalltalk|$SOM_LIB|g" \
    -e "s|/path/to/CSOM|$SOMPP|g" \
    -e "s|^csom = |yksompp = |" \
    "$YK_BENCHMARKS/haste_csom.toml" > "$YK_BENCHMARKS/haste_yksompp_fork.toml"

cd "$YK_BENCHMARKS"
haste bench -f ./haste_yksompp_fork.toml -c "$COMMENT" --order declaration
