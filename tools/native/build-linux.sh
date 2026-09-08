#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
command -v docker >/dev/null || { echo 'BLOCKED: Docker engine/CLI unavailable' >&2; exit 78; }
test "$(docker info --format '{{.OSType}}')" = linux
docker compose version
docker buildx version
mkdir -p dist/linux-x64-dev
docker buildx build --platform linux/amd64 --target artifact-export --output type=local,dest=dist/linux-x64-dev .
