#!/bin/sh
#
# Exercise firmware, keyboard, uPD765A and DSK paths by launching SHAKER 2.7.
#
# GPL 3.0 License (see: LICENSE)
# Copyright (C) 2026 tomaz stih
#

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$script_dir/../.." && pwd)
server="$root/bin/bin/amstrad-cpc-mcp"
disk="$root/build/external-tests/shaker27.dsk"
output="$root/build/shaker-smoke"
picture="$output/module-a.png"
report="$output/session.jsonl"

for file in "$server" "$disk" "$root/data/roms/cpc6128-os.rom" \
            "$root/data/roms/cpc6128-basic.rom" \
            "$root/data/roms/amsdos.rom"; do
    if [ ! -f "$file" ]; then
        echo "required file '$file' is missing; run make roms external-tests" >&2
        exit 1
    fi
done

mkdir -p "$output"
{
    printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"shaker-smoke","version":"1"}}}'
    printf '%s\n' '{"jsonrpc":"2.0","method":"notifications/initialized","params":{}}'
    printf '%s\n' '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"run","arguments":{"frames":100}}}'
    printf '%s\n' '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"press_keys","arguments":{"text":"run\"shake27a.bin\"\n","hold_frames":2,"gap_frames":1}}}'
    printf '%s\n' '{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"run","arguments":{"frames":400}}}'
    printf '%s\n' "{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"tools/call\",\"params\":{\"name\":\"screenshot\",\"arguments\":{\"path\":\"$picture\"}}}"
    printf '%s\n' '{"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"disk","arguments":{"action":"status"}}}'
} | "$server" --model cpc6128 --crtc 0 \
    --os-rom "$root/data/roms/cpc6128-os.rom" \
    --basic-rom "$root/data/roms/cpc6128-basic.rom" \
    --amsdos-rom "$root/data/roms/amsdos.rom" --load "$disk" > "$report"

rg -q 'sent 18 CPC key press' "$report"
rg -q 'disk inserted in drive A' "$report"
printf '%s  %s\n' \
    36c9cae9fb050ea0de482dcc898f52a08e3154546fd3200609731a5f04590c17 \
    "$picture" | sha256sum -c -

echo "SHAKER 2.7 module A reached its pinned menu screen"
echo "this is a launch smoke test, not a SHAKER Approved result"
