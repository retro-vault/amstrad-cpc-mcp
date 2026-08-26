#!/bin/sh
#
# Reproduce the minimal, bootable CP/M 2.2 CPC6128 system disk.
#
# GPL 3.0 License (see: LICENSE)
# Copyright (C) 2026 tomaz stih
#

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM
archive="$temporary/cpmplus.zip"
source_disk="$temporary/cpmplue4.dsk"
result="$temporary/cpm-2.2-en.dsk"
blank="$temporary/blank-track.bin"

for program in cp curl dd install sha256sum tr unzip; do
    if ! command -v "$program" >/dev/null 2>&1; then
        echo "required program '$program' was not found" >&2
        exit 1
    fi
done

curl -fL --retry 3 -o "$archive" \
    https://ftp.nvg.ntnu.no/pub/cpc/utils/cpc/cpmplus.zip
printf '%s  %s\n' \
    92342890d77545e265f4de314526bc7b784466026067965c444c84e46df836e0 \
    "$archive" | sha256sum -c -

unzip -p "$archive" cpmplue4.dsk > "$source_disk"
printf '%s  %s\n' \
    6a45b70d51478d68ace1aaa85ba694fd26068c0c9300d12f426761b8740219f0 \
    "$source_disk" | sha256sum -c -

cp "$source_disk" "$result"
dd if=/dev/zero bs=4608 count=1 status=none | tr '\000' '\345' > "$blank"
track=2
while [ "$track" -lt 40 ]; do
    offset=$((256 + track * 4864 + 256))
    dd if="$blank" of="$result" bs=1 seek="$offset" conv=notrunc \
        status=none
    track=$((track + 1))
done

printf '%s  %s\n' \
    1cd34b92fd8045abbc39ba9854406a5545ff08c79a2cb2076fb0c40db85b9aac \
    "$result" | sha256sum -c -
install -m 0644 "$result" "$script_dir/cpm-2.2-en.dsk"
echo "reproduced $script_dir/cpm-2.2-en.dsk"
