#!/usr/bin/env bash
set -euo pipefail
# Keep the project's tested minichlink command; require an explicit serial port.
if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "Usage: $0 firmware.bin [serial_port] (or set CH32_PORT)" >&2
    exit 1
fi
firmware="$1"
port="${2:-${CH32_PORT:-}}"
if [[ ! -f "$firmware" || -z "$port" ]]; then
    echo "Provide an existing .bin file and your programmer's serial port." >&2
    exit 1
fi
"${MINICHLINK:-minichlink}" -c "$port" -w "$firmware" flash -b
