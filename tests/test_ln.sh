#!/usr/bin/env bash
# myln minimal: hard link (default), -s symlink, -f force
set -euo pipefail
MYLN="${1:-./build/bin/myln}"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

pass=0; fail=0
ok()   { echo "PASS $1"; pass=$((pass+1)); }
bad()  { echo "FAIL $1"; fail=$((fail+1)); }

# 1. hard link: same inode, same content
echo hello > "$TMP/orig"
if $MYLN "$TMP/orig" "$TMP/hard" \
  && [ "$(stat -c %i "$TMP/orig")" = "$(stat -c %i "$TMP/hard")" ] \
  && cmp -s "$TMP/orig" "$TMP/hard"; then ok "hard"; else bad "hard"; fi

# 2. symlink: readlink shows target text
if $MYLN -s "$TMP/orig" "$TMP/sym" \
  && [ "$(readlink "$TMP/sym")" = "$TMP/orig" ] \
  && cmp -s "$TMP/orig" "$TMP/sym"; then ok "-s"; else bad "-s"; fi

# 3. dangling symlink is fine (nobody checks at creation)
if $MYLN -s /nonexistent-target-xyz "$TMP/dangle" \
  && [ "$(readlink "$TMP/dangle")" = "/nonexistent-target-xyz" ]; then ok "dangle"; else bad "dangle"; fi

# 4. existing dest without -f must fail, exit != 0
echo x > "$TMP/e1"; echo y > "$TMP/e2"
if $MYLN "$TMP/e1" "$TMP/e2" 2>/dev/null; then bad "eexist"; else ok "eexist"; fi

# 5. -f overwrites existing dest (hard)
if $MYLN -f "$TMP/e1" "$TMP/e2" \
  && [ "$(stat -c %i "$TMP/e1")" = "$(stat -c %i "$TMP/e2")" ]; then ok "-f"; else bad "-f"; fi

# 6. -sf overwrites existing dest (symlink)
echo z > "$TMP/e3"
if $MYLN -sf "$TMP/e1" "$TMP/e3" && [ -L "$TMP/e3" ]; then ok "-sf"; else bad "-sf"; fi

# 7. missing target for hard link must fail
if $MYLN "$TMP/nope" "$TMP/out" 2>/dev/null; then bad "enoent"; else ok "enoent"; fi

# 8. wrong arg count must fail
if $MYLN only-one 2>/dev/null; then bad "usage"; else ok "usage"; fi

echo "=== $pass passed, $fail failed ==="
[ "$fail" -eq 0 ]
