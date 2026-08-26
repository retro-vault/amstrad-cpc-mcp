#!/bin/sh
#
# Boot the verified firmware for every classic model and pin its first screen.
#
# GPL 3.0 License (see: LICENSE)
# Copyright (C) 2026 tomaz stih
#

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$script_dir/../.." && pwd)
server="$root/bin/bin/amstrad-cpc-mcp"
output="$root/build/firmware-smoke"

if [ ! -x "$server" ]; then
    echo "release server is missing; run make first" >&2
    exit 1
fi

for file in cpc464-os cpc464-basic cpc664-os cpc664-basic \
            cpc6128-os cpc6128-basic amsdos; do
    if [ ! -f "$root/data/roms/$file.rom" ]; then
        echo "firmware is missing; run make roms first" >&2
        exit 1
    fi
done

mkdir -p "$output"

boot()
{
    model=$1
    ram=$2
    expected=$3
    report="$output/cpc$model.jsonl"
    picture="$output/cpc$model.png"

    requests()
    {
        printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"firmware-smoke","version":"1"}}}'
        printf '%s\n' '{"jsonrpc":"2.0","method":"notifications/initialized","params":{}}'
        printf '%s\n' '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"run","arguments":{"frames":100}}}'
        printf '%s\n' "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/call\",\"params\":{\"name\":\"screenshot\",\"arguments\":{\"path\":\"$picture\"}}}"
        printf '%s\n' '{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"status","arguments":{}}}'
    }

    if [ "$model" = 464 ]; then
        requests | "$server" --model cpc464 \
            --os-rom "$root/data/roms/cpc464-os.rom" \
            --basic-rom "$root/data/roms/cpc464-basic.rom" > "$report"
    else
        requests | "$server" --model "cpc$model" \
            --os-rom "$root/data/roms/cpc$model-os.rom" \
            --basic-rom "$root/data/roms/cpc$model-basic.rom" \
            --amsdos-rom "$root/data/roms/amsdos.rom" > "$report"
    fi

    rg -q "\"model\":\"cpc$model\"" "$report"
    rg -q "\"ram_bytes\":$ram" "$report"
    rg -q '"rom_loaded":true' "$report"
    printf '%s  %s\n' "$expected" "$picture" | sha256sum -c -
}

boot 464 65536 \
    93a5b2d3847962da38a4621b998e241f3dd2e84b741999b2b69b26bdaf39a4f1
boot 664 65536 \
    b7815dd28c18bd628ec98c280e47a6b06e229fefdbcd7b683659ef400116550a
boot 6128 131072 \
    464d876eea23432e908586eb2e53191ac70161a45152537d2dc252d1d74c8319

echo "all three verified firmware sets reached their pinned BASIC screen"
