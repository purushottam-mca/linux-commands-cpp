#!/usr/bin/env bash
set -euo pipefail
MYCAT="${1:-/tmp/build-cat/bin/mycat}"
PASS=0; FAIL=0
check() {
  local name="$1"; shift
  if diff -q "$@" > /dev/null; then echo "PASS $name"; PASS=$((PASS+1)); else echo "FAIL $name"; FAIL=$((FAIL+1)); diff -u "$@" || true; fi
}

# setup
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
echo -e "hello\nworld" > "$TMP/a.txt"
echo -n "no nl" > "$TMP/b.txt"
echo -e "a\n\nb" > "$TMP/c.txt"

check "bulk" <($MYCAT "$TMP/a.txt") <(cat "$TMP/a.txt")
check "-n" <($MYCAT -n "$TMP/a.txt") <(cat -n "$TMP/a.txt")
check "-b" <($MYCAT -b "$TMP/c.txt") <(cat -b "$TMP/c.txt")
check "-E" <($MYCAT -E "$TMP/a.txt") <(cat -E "$TMP/a.txt")
check "-nE" <($MYCAT -nE "$TMP/a.txt") <(cat -nE "$TMP/a.txt")
check "no nl -E" <($MYCAT -E "$TMP/b.txt") <(cat -E "$TMP/b.txt")
check "multi" <($MYCAT "$TMP/a.txt" "$TMP/b.txt") <(cat "$TMP/a.txt" "$TMP/b.txt")

printf "hi\n" | $MYCAT > "$TMP/out1"; printf "hi\n" | cat > "$TMP/out2"; check "stdin" "$TMP/out1" "$TMP/out2"

# exit code: nonexistent -> 1
if $MYCAT /nonexistent 2>/dev/null; then echo "FAIL exit"; FAIL=$((FAIL+1)); else echo "PASS exit"; PASS=$((PASS+1)); fi
# directory
if $MYCAT /tmp 2>/dev/null; then echo "FAIL dir"; FAIL=$((FAIL+1)); else echo "PASS dir"; PASS=$((PASS+1)); fi

# binary
head -c 4096 /dev/urandom > "$TMP/bin.dat"
check "binary" <($MYCAT "$TMP/bin.dat") <(cat "$TMP/bin.dat")

echo "=== $PASS passed, $FAIL failed ==="
[ "$FAIL" -eq 0 ]
