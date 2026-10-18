#!/bin/sh
set -eu
runner=$1
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
# Header occupies two bytes. LDA #'X'; STA $FD06; BRA -7.
printf '\000\000\206X\267\375\006\040\371' > "$tmp/code"
for level in 1 2; do
    case "$level" in
       1) size=4352;;
       2) size=5376;;
    esac
    dd if=/dev/zero bs=1 count="$size" of="$tmp/track" 2>/dev/null
    dd if="$tmp/code" of="$tmp/track" conv=notrunc 2>/dev/null
    output=$(printf "s\ns\nq\n" | "$runner" --boottrack "$tmp/track" --level "$level" --cpu 6809 --instructions 2)
    echo "$output" | grep -q "PC=" || { echo "failed level $level serial boottrack entry" >&2; exit 1; }
    # Unpadded boottracks (e.g. the real Level 1 image is 4224 bytes)
    # must be accepted and loaded at the same entry point.
    truncate -s $((size - 128)) "$tmp/track"
    output=$(printf "s\ns\nq\n" | "$runner" --boottrack "$tmp/track" --level "$level" --cpu 6809 --instructions 2)
    echo "$output" | grep -q "PC=" || { echo "failed level $level short boottrack" >&2; exit 1; }
done
# Reject oversized images instead of silently truncating them.
dd if=/dev/zero bs=1 count=4353 of="$tmp/track" 2>/dev/null
if "$runner" --boottrack "$tmp/track" --level 1 --instructions 1 </dev/null 2>"$tmp/error"; then
    echo "accepted oversized boottrack" >&2
    exit 1
fi
grep -q 'boottrack size' "$tmp/error"
# The empty file is not a valid boottrack.
: > "$tmp/track"
if "$runner" --boottrack "$tmp/track" --level 1 --instructions 1 </dev/null 2>"$tmp/error"; then
    echo "accepted empty boottrack" >&2
    exit 1
fi
