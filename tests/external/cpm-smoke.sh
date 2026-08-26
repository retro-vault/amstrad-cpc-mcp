#!/bin/sh
#
# Boot the bundled CP/M 2.2 disk through the public MCP workflow.
#
# GPL 3.0 License (see: LICENSE)
# Copyright (C) 2026 tomaz stih
#

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$script_dir/../.." && pwd)
server="$root/bin/bin/amstrad-cpc-mcp"
disk="$root/data/disks/cpm-2.2-en.dsk"
output="$root/build/cpm-smoke"
picture="$output/prompt.png"
report="$output/session.jsonl"

for file in "$server" "$disk" "$root/data/roms/cpc6128-os.rom" \
            "$root/data/roms/cpc6128-basic.rom" \
            "$root/data/roms/amsdos.rom"; do
    if [ ! -f "$file" ]; then
        echo "required file '$file' is missing; run make roms release" >&2
        exit 1
    fi
done

mkdir -p "$output"
cd "$root"
{
    printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"cpm-smoke","version":"1"}}}'
    printf '%s\n' '{"jsonrpc":"2.0","method":"notifications/initialized","params":{}}'
    printf '%s\n' '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"cpm","arguments":{}}}'
    printf '%s\n' "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/call\",\"params\":{\"name\":\"screenshot\",\"arguments\":{\"path\":\"$picture\",\"include_border\":false,\"monitor\":\"green\"}}}"
} | "$server" --model cpc6128 --crtc 0 \
    --os-rom "$root/data/roms/cpc6128-os.rom" \
    --basic-rom "$root/data/roms/cpc6128-basic.rom" \
    --amsdos-rom "$root/data/roms/amsdos.rom" > "$report"

rg -q 'ran the CP/M 2.2 boot sequence' "$report"
rg -q '"mode":"cpm"' "$report"
rg -q '"version":"2.2"' "$report"
rg -q '"disk_motor":false' "$report"
printf '%s  %s\n' \
    13d62f4664ae593227e2fe5896f0e7bd2885c0d46f3691400827e350d53f76c7 \
    "$picture" | sha256sum -c -

echo "bundled CP/M 2.2 reached its pinned A> prompt through MCP"
