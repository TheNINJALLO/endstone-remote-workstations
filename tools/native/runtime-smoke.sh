#!/bin/sh
set -eu
if [ "${VCF_LICENSE_CONFIRMED:-0}" != 1 ]; then
 echo 'BLOCKED: confirm the applicable server license separately; this script does not accept it.' >&2
 exit 78
fi
if [ ! -f /inputs/server/bedrock_server ] || [ ! -f /inputs/SHA256SUMS ] || [ ! -f /inputs/launch.sh ]; then
 echo 'BLOCKED: authorized Linux bedrock_server, runtime, SHA256SUMS and launcher are required in /inputs.' >&2
 exit 78
fi
(cd /inputs && sha256sum -c SHA256SUMS)
# launch.sh belongs to the private test inputs and must use /data for the
# disposable world, port 29169, and the exact separately admitted loader.
mkdir -p /data/plugins
cp /artifacts/plugins/endstone_onistone_vcf.so /data/plugins/
exec /bin/sh /inputs/launch.sh /data
