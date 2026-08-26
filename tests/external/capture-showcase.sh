#!/bin/sh
#
# Boot five games over MCP and capture the same raster for both CPC monitors.
#
# GPL 3.0 License (see: LICENSE)
# Copyright (C) 2026 tomaz stih
#

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$script_dir/../.." && pwd)
server="$root/bin/bin/amstrad-cpc-mcp"
games="$root/build/games"
reports="$root/build/game-showcase"
screenshots="$root/docs/screenshots"

for file in "$server" "$root/data/roms/cpc6128-os.rom" \
            "$root/data/roms/cpc6128-basic.rom" \
            "$root/data/roms/amsdos.rom"; do
    if [ ! -f "$file" ]; then
        echo "required file '$file' is missing; run make roms release" >&2
        exit 1
    fi
done

mkdir -p "$reports" "$screenshots"

capture_game() {
    slug=$1
    disk=$2
    run_name=$3
    before_key=$4
    key=$5
    after_key=$6
    report="$reports/$slug.jsonl"
    color="$screenshots/$slug-color.png"
    green="$screenshots/$slug-green.png"

    if [ ! -f "$games/$disk" ]; then
        echo "required disk '$games/$disk' is missing; run make showcase-games" >&2
        exit 1
    fi

    {
        printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"game-showcase","version":"1"}}}'
        printf '%s\n' '{"jsonrpc":"2.0","method":"notifications/initialized","params":{}}'
        printf '%s\n' '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"run","arguments":{"frames":100}}}'
        printf '%s\n' "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/call\",\"params\":{\"name\":\"press_keys\",\"arguments\":{\"text\":\"run\\\"$run_name\\\"\\n\",\"hold_frames\":2,\"gap_frames\":1}}}"
        printf '%s\n' "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"tools/call\",\"params\":{\"name\":\"run\",\"arguments\":{\"frames\":$before_key}}}"
        if [ "$key" != none ]; then
            printf '%s\n' "{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"tools/call\",\"params\":{\"name\":\"press_keys\",\"arguments\":{\"keys\":[\"$key\"],\"hold_frames\":3,\"gap_frames\":1}}}"
            printf '%s\n' "{\"jsonrpc\":\"2.0\",\"id\":6,\"method\":\"tools/call\",\"params\":{\"name\":\"run\",\"arguments\":{\"frames\":$after_key}}}"
        fi
        printf '%s\n' "{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"tools/call\",\"params\":{\"name\":\"screenshot\",\"arguments\":{\"path\":\"$color\",\"include_border\":false,\"monitor\":\"color\"}}}"
        printf '%s\n' "{\"jsonrpc\":\"2.0\",\"id\":8,\"method\":\"tools/call\",\"params\":{\"name\":\"screenshot\",\"arguments\":{\"path\":\"$green\",\"include_border\":false,\"monitor\":\"green\"}}}"
        printf '%s\n' '{"jsonrpc":"2.0","id":9,"method":"tools/call","params":{"name":"disk","arguments":{"action":"status"}}}'
    } | "$server" --model cpc6128 --crtc 0 \
        --os-rom "$root/data/roms/cpc6128-os.rom" \
        --basic-rom "$root/data/roms/cpc6128-basic.rom" \
        --amsdos-rom "$root/data/roms/amsdos.rom" \
        --load "$games/$disk" > "$report"

    rg -q 'sent .* CPC key press' "$report"
    rg -q '"monitor":"color"' "$report"
    rg -q '"monitor":"green"' "$report"
    rg -q 'disk inserted in drive A' "$report"
    test -s "$color"
    test -s "$green"
    echo "captured $slug"
}

# Heart is advanced to gameplay; Dawn is advanced to its title artwork. The
# other deterministic captures use their interactive title screens.
capture_game heart-of-salamanderland \
    heart-of-salamanderland/heartosa.dsk HEARTOSA 1000 SPACE 200
capture_game brick-rick brick-rick/brickr.dsk BRICKR 1000 none 0
capture_game kitsunes-curse kitsunes-curse/kitcurs.dsk KITCURS 1000 none 0
capture_game dawn-of-kernel \
    dawn-of-kernel/dawn-of-kernel.dsk KERNEL 1800 SPACE 400
capture_game magica magica/magica.dsk MAGICA 1000 none 0

echo "saved ten MCP screenshots under $screenshots"
