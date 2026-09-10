#!/bin/sh
set -eu
dest="${1:?Provide a workspace dependency directory}"
mkdir -p "$dest"
if [ ! -d "$dest/endstone/.git" ]; then
 git init "$dest/endstone"
 git -C "$dest/endstone" remote add origin https://github.com/EndstoneMC/endstone.git
fi
revision=8f84d6f5b556916597ed5b6b71329b2ed3ca8fc8
if ! git -C "$dest/endstone" cat-file -e "$revision^{commit}" 2>/dev/null; then
 git -C "$dest/endstone" -c http.lowSpeedLimit=1024 -c http.lowSpeedTime=60 fetch --depth 1 https://github.com/EndstoneMC/endstone.git "$revision"
fi
git -C "$dest/endstone" -c core.autocrlf=false checkout --detach "$revision"
mkdir -p "$dest/expected-lite/include/nonstd"
curl --connect-timeout 15 --max-time 180 --retry 2 -fsSL https://raw.githubusercontent.com/martinmoene/expected-lite/v0.9.0/include/nonstd/expected.hpp -o "$dest/expected-lite/include/nonstd/expected.hpp"
printf '%s  %s\n' bbab48a56231800c21373be71e47f1b9d8d4e8e10e2c9d3a51c4f5f850104086 "$dest/expected-lite/include/nonstd/expected.hpp" | sha256sum -c -
# Only the small header closure used by Bedrock's actual Result<T>/NBT ABI.
manifest="$(dirname "$0")/../../cmake/entt-nbt-headers.sha256"
while read -r expected relative; do
 mkdir -p "$dest/entt/$(dirname "$relative")"
 curl --connect-timeout 15 --max-time 180 --retry 2 -fsSL "https://raw.githubusercontent.com/skypjack/entt/v3.16.0/$relative" -o "$dest/entt/$relative"
 printf '%s  %s\n' "$expected" "$dest/entt/$relative" | sha256sum -c -
done < "$manifest"
