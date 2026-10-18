#!/bin/sh
set -eu
runner=$1
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
# Header occupies two bytes. LDA #'X'; STA $FD06; BRA -7.
printf '\\000\\000\\206X\\267\\375\\006\\040\\371' > "$tmp/code"
for level in 1 2; do
    case "$level" in
       1) size=4352;;
       2) size=5376;;
    esac
    dd if=/dev/zero bs=1 count="$size" of="$tmp/track" 2>/dev/null
    dd if="$tmp/code" of="$tmp/track" conv=notrunc 2>/dev/null
    output=$("$runner" --boottrack "$tmp/track" --level "$level" --cpu 6809 --instructions 2 </dev/null)
    [ "$output" = X ] || { echo "failed level $level serial boottrack entry" >&2; exit 1; }
done
# Reject a truncated boottrack rather than jumping into uninitialized memory.
if "$runner" --boottrack "$tmp/code" --level 1 --instructions 1 </dev/null 2>"$tmp/error"; then
  echo "accepted truncated boottrack" >&2
  exit 1
fi
grep -q 'boottrack size' "$tmp/error"
