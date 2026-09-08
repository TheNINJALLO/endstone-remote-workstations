#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
: "${VCF_LINUX_INPUTS:?Set VCF_LINUX_INPUTS to authorized private Linux inputs}"
if [ "${VCF_LICENSE_CONFIRMED:-0}" != 1 ]; then
    echo 'BLOCKED: confirm the applicable server license separately.' >&2
    exit 78
fi
test -f "$VCF_LINUX_INPUTS/launch.sh"
test "$(docker info --format '{{.OSType}}')" = linux
mkdir -p out/linux-smoke
docker compose --profile runtime build smoke
docker compose --profile runtime run --rm --service-ports smoke
