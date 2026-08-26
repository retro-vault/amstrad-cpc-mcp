#!/bin/sh
#
# Fetch the primary component manuals and CPC conformance references used
# while implementing and checking the emulator. Files stay under build/.
#
# GPL 3.0 License (see: LICENSE)
# Copyright (C) 2026 tomaz stih
#

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$script_dir/../.." && pwd)
destination=${1:-"$root/build/references"}
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

mkdir -p "$destination"

fetch()
{
    name=$1
    hash=$2
    url=$3
    curl -fsSL --retry 3 -A 'Mozilla/5.0' "$url" \
        -o "$temporary/$name"
    printf '%s  %s\n' "$hash" "$temporary/$name" | sha256sum -c -
    install -m 0644 "$temporary/$name" "$destination/$name"
}

fetch zilog-z80-um0080.pdf \
    e3c83da5a5d8e372364c20fa53665e6fbb165ec6ac38c8c1eebc359603447b5e \
    https://www.zilog.com/docs/z80/um0080.pdf
fetch gi-ay-3-8910.pdf \
    a783dc24e16326852c27d0d216d6f92493596245f9c5bef55913ca7a145ed0f7 \
    https://map.grauw.nl/resources/sound/generalinstrument_ay-3-8910.pdf
fetch motorola-6845.pdf \
    84ae68617c23e3b3057dce86f4a04a5be5d890d108bce8e9bb928bfdd2759dcf \
    https://archive.decromancer.ca/bitsavers.org/components/motorola/_dataSheets/6845.pdf
fetch intel-8255.pdf \
    1f2df669139b8f51b7b6cd7eb6b854a0f5f37a8a59943a02f2141b32e3ecc5c4 \
    https://deramp.com/downloads/intel/8255.pdf
fetch nec-upd765.pdf \
    e8a37f1242d4e26b400a524519b0b2efea8a68b83b4770e7a081bb82f6edfebc \
    https://hxc2001.com/download/datasheet/floppy/thirdparty/FDC/NEC/UPD765_Datasheet_OCRed.pdf
fetch amstrad-firmware.pdf \
    8d329b109113fcffcb2ef14a8d37fdb20a50eb91f6ce4e5f7f50f4065d16d67d \
    https://cpctech.cpcwiki.de/docs/manual/s968se01.pdf
fetch crtc-compendium-1.10.pdf \
    a07abe697004dec6a79b90523e70fabb8339ee375a6d0fbb52bf758cb42d81f8 \
    https://shaker.logonsystem.eu/ACCC1.10-EN.pdf
fetch z80-cpc-timing-2013.pdf \
    949f7e9750a45c427c7430b8c8e06e86bfbc348157b7dc83a1afba6bd0007ab9 \
    'https://wiki.octoate.de/lib/exe/fetch.php/amstradcpc%3Az80_cpc_timings_cheat_sheet.20131019.pdf'

echo "installed verified reference documents in $destination"
