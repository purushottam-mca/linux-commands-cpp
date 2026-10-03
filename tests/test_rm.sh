#!/usr/bin/env bash
# myrm minimal: unlink files, -r for dirs (depth-first), -f ignores missing
set -euo pipefail
MYRM="${1:-./build/bin/myrm}"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

pass=0; fail=0
ok()  { echo "PASS $1"; pass=$((pass+1)); }
bad() { echo "FAIL $1"; fail=$((fail+1)); }

# 1. plain file
touch "$TMP/f1"
if $MYRM "$TMP/f1" && [ ! -e "$TMP/f1" ]; then ok "file"; else bad "file"; fi

# 2. symlink removed, target kept (lstat must not follow)
echo data > "$TMP/target"; ln -s target "$TMP/link"
if $MYRM "$TMP/link" && [ ! -e "$TMP/link" ] && [ -f "$TMP/target" ]; then ok "symlink"; else bad "symlink"; fi

# 3. symlink-to-dir: unlinks the link, never touches target contents
mkdir -p "$TMP/realdir"; echo keep > "$TMP/realdir/keep"
ln -s "$TMP/realdir" "$TMP/dirlink"
if $MYRM "$TMP/dirlink" && [ ! -e "$TMP/dirlink" ] && [ -f "$TMP/realdir/keep" ]; then ok "dirlink"; else bad "dirlink"; fi

# 4. dir without -r must fail and survive
mkdir -p "$TMP/d1"; touch "$TMP/d1/a"
if $MYRM "$TMP/d1" 2>/dev/null; then bad "no-r"; elif [ -d "$TMP/d1" ]; then ok "no-r"; else bad "no-r"; fi

# 5. -r removes nested tree
mkdir -p "$TMP/tree/sub/deep"; echo x > "$TMP/tree/sub/deep/f"; echo y > "$TMP/tree/top"
if $MYRM -r "$TMP/tree" && [ ! -e "$TMP/tree" ]; then ok "-r"; else bad "-r"; fi

# 6. missing without -f fails
if $MYRM "$TMP/nope" 2>/dev/null; then bad "enoent"; else ok "enoent"; fi

# 7. missing with -f is silent success
if $MYRM -f "$TMP/nope" 2>/dev/null; then ok "-f"; else bad "-f"; fi

# 8. -rf on missing is silent success
if $MYRM -rf "$TMP/nope" 2>/dev/null; then ok "-rf-missing"; else bad "-rf-missing"; fi

# 9. one bad path does not block the rest
touch "$TMP/good"
if $MYRM "$TMP/alsono" "$TMP/good" 2>/dev/null; then bad "continue"; elif [ ! -e "$TMP/good" ]; then ok "continue"; else bad "continue"; fi

# 10. file named -f works via --
touch "$TMP/-f"
if (cd "$TMP" && $MYRM -- -f) && [ ! -e "$TMP/-f" ]; then ok "dashfile"; else bad "dashfile"; fi

# 11. no operands fails
if $MYRM 2>/dev/null; then bad "usage"; else ok "usage"; fi

echo "=== $pass passed, $fail failed ==="
[ "$fail" -eq 0 ]
