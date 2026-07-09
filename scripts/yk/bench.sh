#!/usr/bin/env bash
# Substitute paths into scripts/yk/haste_yksompp_fork.toml (via envsubst) and
# run haste bench.
#
# Usage:
#   scripts/yk/bench.sh <sompp-binary> <som-lib-dir> <yk-benchmarks-dir> [comment]
#
# Prereqs: haste + envsubst (gettext-base) on PATH; all three paths exist.

set -eux

if [ $# -lt 3 ]; then
  echo "usage: $0 <sompp-binary> <som-lib-dir> <yk-benchmarks-dir> [comment]" >&2
  exit 2
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
export SOMPP SOM_LIB
SOMPP=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
SOM_LIB=$(cd "$2" && pwd)
YK_BENCHMARKS=$(cd "$3" && pwd)
COMMENT="${4:-ci}"

# Restrict substitution to SOMPP/SOM_LIB so stray $vars in future edits don't
# get eaten.
envsubst '${SOMPP} ${SOM_LIB}' \
  < "$SCRIPT_DIR/haste_yksompp_fork.toml" \
  > "$YK_BENCHMARKS/haste_yksompp_fork.toml"

cd "$YK_BENCHMARKS"
haste bench -f ./haste_yksompp_fork.toml -c "$COMMENT" --order declaration

# Emit a haste-diff table for the fresh datum. yk-benchmarks is cloned fresh in
# CI so IDs restart from 1; take the highest just in case.
DATUM_ID=$(ls -1 .haste 2>/dev/null | grep -E '^[0-9]+$' | sort -n | tail -1)
[ -n "$DATUM_ID" ] && haste diff -f ./haste_yksompp_fork.toml "$DATUM_ID" "$DATUM_ID"
