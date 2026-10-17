#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PORT="${1:-/dev/ttyUSB0}"
BAUD="${2:-115200}"
CACHE="${XDG_CACHE_HOME:-$HOME/.cache}/playr/ccache"
mkdir -p "$CACHE"

docker run --rm -v "$DIR:/project" -w /project \
    -v "$CACHE:/ccache" -e CCACHE_DIR=/ccache \
    --user "$(id -u):$(id -g)" -e HOME=/tmp \
    --device "$PORT" --group-add "$(stat -c %g "$PORT")" \
    sle118/squeezelite-esp32-idfv448:4.4.8 \
    bash -c ". \"\$IDF_PATH/export.sh\" >/dev/null && idf.py build && python3 - \"$PORT\" \"$BAUD\" << 'PY'
import os, subprocess, sys, tempfile

port, baud = sys.argv[1], sys.argv[2]

def flash_part(offset, path, reset_after=False):
    after = 'hard_reset' if reset_after else 'no_reset'
    cmd = [
        'esptool.py', '--chip', 'esp32', '-p', port, '-b', baud,
        '--before', 'default_reset', '--after', after,
        'write_flash', '--flash_mode', 'dio', '--flash_freq', '40m', '--flash_size', 'detect',
        hex(offset), path
    ]
    for attempt in range(1, 5):
        print(f'[FLASH] Writing {path} @ {hex(offset)} (attempt {attempt})...')
        if subprocess.call(cmd) == 0:
            return
    sys.exit(1)

flash_part(0x1000, 'build/bootloader/bootloader.bin')
flash_part(0x8000, 'build/partition_table/partition-table.bin')

with open('build/oled_wled.bin', 'rb') as f:
    data = f.read()

CHUNK = 0x20000  # 128 KB slices (4KB sector aligned)
num_chunks = (len(data) + CHUNK - 1) // CHUNK
for i in range(num_chunks):
    chunk_data = data[i * CHUNK : (i + 1) * CHUNK]
    tmp_path = f'/tmp/oled_wled_chunk_{i}.bin'
    with open(tmp_path, 'wb') as tf:
        tf.write(chunk_data)
    flash_part(0x10000 + i * CHUNK, tmp_path, reset_after=(i == num_chunks - 1))
print('[FLASH] All slices written and verified!')
PY"
