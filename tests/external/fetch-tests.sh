#!/bin/sh
#
# Fetch published CPC conformance media used for manual compatibility runs.
#
# GPL 3.0 License (see: LICENSE)
# Copyright (C) 2026 tomaz stih
#

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$script_dir/../.." && pwd)
destination=${1:-"$root/build/external-tests"}
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

mkdir -p "$destination"

fetch()
{
    url=$1
    name=$2
    hash=$3
    curl -fsSL "$url" -o "$temporary/$name"
    printf '%s  %s\n' "$hash" "$temporary/$name" | sha256sum -c -
    install -m 0644 "$temporary/$name" "$destination/$name"
}

# SHAKER is the CPC community's Gate Array and CRTC regression suite. Its
# authors ask emulator projects to cite the Compendium and not claim the
# official SHAKER seal without submitting results for verification.
fetch https://shaker.logonsystem.eu/Shaker_CSL/shaker27.dsk \
    shaker27.dsk \
    65eb43e1f99ea232a6cc1494e799880488130ba1876bfe9d67b347a924e7721b

echo "installed verified conformance media in $destination"
