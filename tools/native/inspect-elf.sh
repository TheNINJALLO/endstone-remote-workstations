#!/bin/sh
set -eu
file="${1:?ELF path}"
report="${2:?Report path}"
{
 file "$file"
 readelf -h "$file"
 readelf -d "$file"
 readelf --version-info "$file"
 nm -D --defined-only "$file"
} > "$report"
if readelf -d "$file" | grep -q 'NEEDED.*libstdc++'; then
 echo 'Rejected: plugin depends on libstdc++ instead of libc++' >&2
 exit 1
fi
nm -D --defined-only "$file" | grep -q 'init_endstone_plugin'
nm -D --defined-only "$file" | grep -q 'oni_vcf_get_api'
