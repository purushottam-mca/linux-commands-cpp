#!/usr/bin/env bash
# myls minimal: -a, -l
set -euo pipefail
MYLS="${1:-./build/bin/myls}"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/dir"
echo hello > "$TMP/dir/a.txt"
echo hidden > "$TMP/dir/.hidden"
ln -s a.txt "$TMP/dir/link.txt"

pass=0; fail=0
check() {
  local name="$1"; shift
  if diff -q "$@" > /dev/null; then echo "PASS $name"; pass=$((pass+1)); else echo "FAIL $name"; fail=$((fail+1)); diff -u "$@" || true; fi
}
check "basic" <($MYLS "$TMP/dir" | sort) <(ls "$TMP/dir" | sort)
check "-a" <($MYLS -a "$TMP/dir" | sort) <(ls -a "$TMP/dir" | sort)
check "-l count" <($MYLS -l "$TMP/dir" | wc -l) <(ls -l "$TMP/dir" | tail -n +2 | wc -l)
check "-la" <($MYLS -la "$TMP/dir" | wc -l) <(ls -la "$TMP/dir" | tail -n +2 | wc -l)
echo "=== $pass passed, $fail failed ==="
[ "$fail" -eq 0 ]
