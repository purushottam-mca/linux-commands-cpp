#!/usr/bin/env bash
# mychmod minimal: octal MODE, -R, -v
set -euo pipefail
MYCHMOD="${1:-./build/bin/mychmod}"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

pass=0; fail=0
touch "$TMP/a"; chmod 644 "$TMP/a"
$MYCHMOD 755 "$TMP/a"
[ "$(stat -c %a "$TMP/a")" = "755" ] && { echo "PASS octal"; pass=$((pass+1)); } || { echo "FAIL octal"; fail=$((fail+1)); }

mkdir -p "$TMP/r/x"; touch "$TMP/r/x/f"
chmod -R 755 "$TMP/r"
$MYCHMOD -R 644 "$TMP/r" 2>/dev/null || true
chmod 755 "$TMP/r" "$TMP/r/x" 2>/dev/null || true
# verify -R reached the nested file (compare against fresh gnu run)
rm -rf "$TMP/r2"; mkdir -p "$TMP/r2/x"; touch "$TMP/r2/x/f"; chmod -R 755 "$TMP/r2"
$MYCHMOD -R 775 "$TMP/r" > /dev/null
chmod -R 775 "$TMP/r2" > /dev/null
if [ "$(stat -c %a "$TMP/r/x/f")" = "$(stat -c %a "$TMP/r2/x/f")" ]; then echo "PASS -R"; pass=$((pass+1)); else echo "FAIL -R"; fail=$((fail+1)); fi

touch "$TMP/v"; chmod 644 "$TMP/v"
if $MYCHMOD -v 600 "$TMP/v" 2>&1 | grep -q "changed"; then echo "PASS -v"; pass=$((pass+1)); else echo "FAIL -v"; fail=$((fail+1)); fi

if $MYCHMOD 755 /nonexistent 2>/dev/null; then echo "FAIL err"; fail=$((fail+1)); else echo "PASS err"; pass=$((pass+1)); fi
echo "=== $pass passed, $fail failed ==="
[ "$fail" -eq 0 ]
