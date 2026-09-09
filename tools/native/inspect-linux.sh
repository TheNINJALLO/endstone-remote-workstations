#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
: "${VCF_LINUX_INPUTS:?Set VCF_LINUX_INPUTS to authorized private Linux inputs}"
test -f "$VCF_LINUX_INPUTS/input-manifest.json"
test -f "$VCF_LINUX_INPUTS/SHA256SUMS"
test "$(docker info --format '{{.OSType}}')" = linux
mkdir -p out/linux-abi
docker compose --profile research build abi-lab
docker compose --profile research run -T --rm --no-deps --entrypoint /bin/sh abi-lab /workspace/tools/native/inspect-runtime.sh
