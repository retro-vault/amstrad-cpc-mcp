#!/bin/sh
#
# Fetch the freely playable games used by the MCP screenshot showcase.
#
# GPL 3.0 License (see: LICENSE)
# Copyright (C) 2026 tomaz stih
#

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$script_dir/../.." && pwd)
output="$root/build/games"
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

for program in curl sha256sum unzip; do
    if ! command -v "$program" >/dev/null 2>&1; then
        echo "required program '$program' was not found" >&2
        exit 1
    fi
done

mkdir -p "$output"

fetch_archive() {
    slug=$1
    archive_hash=$2
    disk_path=$3
    disk_hash=$4
    archive="$output/$slug.zip"
    url="https://www.usebox.net/jjm/$slug/$slug.zip"

    if ! printf '%s  %s\n' "$archive_hash" "$archive" |
            sha256sum -c - >/dev/null 2>&1; then
        download="$temporary/$slug.zip"
        echo "fetching $slug"
        curl -fL --retry 3 -o "$download" "$url"
        printf '%s  %s\n' "$archive_hash" "$download" | sha256sum -c -
        install -m 0644 "$download" "$archive"
    fi

    unzip -oq "$archive" -d "$output"
    printf '%s  %s\n' "$disk_hash" "$output/$disk_path" |
        sha256sum -c -
}

fetch_archive \
    heart-of-salamanderland \
    86bfd99a7619c65575b2a2836fe319dee08a675dc4ca7518a87d0fdb60385bc8 \
    heart-of-salamanderland/heartosa.dsk \
    66269fd18477bb18fa317ffb30d3a66d3586097689007273dd3f93940f533a1c

fetch_archive \
    brick-rick \
    2672ecacee08dbabadacb465b7ba1a12f099c992c4d480471c6d82c942b8d034 \
    brick-rick/brickr.dsk \
    c5e0cc27f96a3c611d48228b7883d3e83e765ba438291a789423f9049593d8b0

fetch_archive \
    kitsunes-curse \
    ab43d0cab242d27cf57d5ebe8ec7e28ef5eb9d58a5e7cb9c8fea94a85c23084e \
    kitsunes-curse/kitcurs.dsk \
    4f1733deae1ae76a58b675998507e0fa0675d5a02f817d4f9b15a51ff2d6a8b8

fetch_archive \
    dawn-of-kernel \
    d8f44af36afed9f8a9027b9f7eb542d3764d692fe6afe14787694054dedc4e97 \
    dawn-of-kernel/dawn-of-kernel.dsk \
    88b61193cfc1da50eaea4c0129bf1bfed9cadb291369b4522a170485c2c0fdcb

fetch_archive \
    magica \
    7da3c530cfb8da2b396fea8898a74b54ddcca717c12a68045fee0deae55cd53b \
    magica/magica.dsk \
    0081f1ddb100188155ed32bce575fb161f33a98f05809a507aafdd27ca86eb6c

echo "verified showcase games under $output"
