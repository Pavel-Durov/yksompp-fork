#!/bin/sh
# Build and push the prebuilt yk image to GitHub Packages (GHCR).
#
# Usage:
#   build-yk-image.sh <path-to-yk>
#
# Example:
#   export GHCR_PAT=ghp_xxx           # PAT with write:packages
#   yksompp/scripts/yk/build-yk-image.sh /home/pd/yk
#
# Overrides:
#   YK_IMAGE=ghcr.io/<owner>/<name>   # default: ghcr.io/pavel-durov/yk-build
#   GHCR_USER=<github-user>           # default: pavel-durov
#   PUSH=0                            # build only, skip push

set -eu

if [ $# -ne 1 ]; then
  echo "usage: $0 <path-to-yk>" >&2
  exit 2
fi

YK_DIR=$1
GHCR_USER="${GHCR_USER:-pavel-durov}"
YK_IMAGE="${YK_IMAGE:-ghcr.io/${GHCR_USER}/yk-build}"

if [ ! -e "$YK_DIR/.git" ]; then
  echo "not a git checkout: $YK_DIR" >&2
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
YK_DIR="$(cd "$YK_DIR" && pwd)"
CONTEXT="$(dirname "$YK_DIR")"
YK_SHA=$(git -C "$YK_DIR" rev-parse --short HEAD)

logged_in=0
if [ -n "${GHCR_PAT:-}" ]; then
  printf '%s' "$GHCR_PAT" | docker login ghcr.io -u "$GHCR_USER" --password-stdin
  logged_in=1
fi

export DOCKER_BUILDKIT=1
docker build -f "$SCRIPT_DIR/Dockerfile.yk" \
  -t "$YK_IMAGE:$YK_SHA" \
  -t "$YK_IMAGE:latest" \
  "$CONTEXT"

if [ "${PUSH:-1}" = "1" ]; then
  docker push "$YK_IMAGE:$YK_SHA"
  docker push "$YK_IMAGE:latest"
fi

if [ "$logged_in" = "1" ]; then
  docker logout ghcr.io >/dev/null
  echo "Reminder: docker login writes credentials to \$HOME/.docker/config.json."
  echo "  Verify it's clean: grep -v ghcr.io \$HOME/.docker/config.json"
  echo "  Or remove entirely: rm \$HOME/.docker/config.json"
fi

echo "Built $YK_IMAGE:$YK_SHA"
