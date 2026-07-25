#!/bin/sh
# Reproduce the CI test_yk job locally: pull the prebuilt yk image and run
# build-and-test.sh inside it against the current checkout.
#
# Usage:
#   scripts/yk/run-local.sh          # uses ghcr.io/pavel-durov/yk-build:latest
#   YK_IMAGE=... scripts/yk/run-local.sh

set -eu

YK_IMAGE="${YK_IMAGE:-ghcr.io/pavel-durov/yk-build:latest}"
REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

docker run --rm \
  -v "$REPO_ROOT":/repo \
  -w /repo \
  "$YK_IMAGE" \
  scripts/yk/build-and-test.sh
