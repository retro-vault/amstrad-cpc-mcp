#!/bin/sh
#
# Fetch the freely redistributable English CPC firmware and split each
# combined image into the sockets consumed by amstrad-cpc-mcp.
#
# GPL 3.0 License (see: LICENSE)
# Copyright (C) 2026 tomaz stih
#

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
download_dir=$(mktemp -d)
trap 'rm -rf "$download_dir"' EXIT HUP INT TERM
base=https://raw.githubusercontent.com/cpcitor/cpcec/master

fetch()
{
    name=$1
    hash=$2
    curl -fsSL "$base/$name" -o "$download_dir/$name"
    printf '%s  %s\n' "$hash" "$download_dir/$name" | sha256sum -c -
}

fetch cpc464.rom 00960d9bf75b2b90856c970f1aa078e1e2aa028b2c104f1dded0262f5d37b15e
fetch cpc664.rom 1fcb20cf169f170774bf94954db9372c95edd038a5cb8e5199774552b93f8747
fetch cpc6128.rom 31c3668c67bea027dab698ece233c9434d9324f9ba7dac84db58f400b6689562
fetch cpcados.rom ea65e0fb44ee93ede4b6c507509b7e5ddf497fb7155023bea91ef229469fa04d

for model in 464 664 6128; do
    dd if="$download_dir/cpc$model.rom" \
       of="$script_dir/cpc$model-os.rom" bs=16384 count=1 status=none
    dd if="$download_dir/cpc$model.rom" \
       of="$script_dir/cpc$model-basic.rom" bs=16384 skip=1 count=1 \
       status=none
done
install -m 0644 "$download_dir/cpcados.rom" "$script_dir/amsdos.rom"

echo "installed verified CPC464, CPC664 and CPC6128 ROMs in $script_dir"
