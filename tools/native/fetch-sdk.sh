#!/bin/sh
set -eu
dest="${1:?Provide a workspace dependency directory}"
mkdir -p "$dest"
if [ ! -d "$dest/endstone/.git" ]; then
 git -c core.autocrlf=false clone --no-checkout https://github.com/EndstoneMC/endstone.git "$dest/endstone"
fi
git -C "$dest/endstone" -c core.autocrlf=false checkout --detach 8f84d6f5b556916597ed5b6b71329b2ed3ca8fc8
mkdir -p "$dest/expected-lite/include/nonstd"
curl -fsSL https://raw.githubusercontent.com/martinmoene/expected-lite/v0.9.0/include/nonstd/expected.hpp -o "$dest/expected-lite/include/nonstd/expected.hpp"
printf '%s  %s\n' bbab48a56231800c21373be71e47f1b9d8d4e8e10e2c9d3a51c4f5f850104086 "$dest/expected-lite/include/nonstd/expected.hpp" | sha256sum -c -
