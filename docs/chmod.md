# `chmod` - Minimal (octal mode; flags: `-R`, `-v`)

Implementation: `src/chmod/main.cpp:1`

## 1. What it does
Sets permission bits, e.g. `mychmod 755 file`. `-R` applies to a tree, `-v` prints each change.

## 2. Syscall
`chmod(path, mode)` - changes only inode metadata, no data I/O.

## 3. Flow
```
args: flags MUST come before MODE (else "755" looks like flags)
  |
  v
parse MODE as octal -- invalid --> error "use octal, e.g. 755", exit 1
  |
  v
for each FILE:
  |
  v
-R and real dir? -- no --> chmod(path, mode) [+ print if -v]
  |
  yes
  v
lstat: symlink? --> treat as file (chmod follows it to target)
  |
  real dir
  v
recurse into every child FIRST
  |
  v
chmod the directory itself LAST (bottom-up)
```

Step by step: `main:74` parses flags only while no mode is seen yet, so the first non-flag becomes MODE and the rest are files. `parse_octal:21` accepts 1-4 octal digits up to `07777`; validating once up front avoids a half-chmodded tree when the mode has a typo. `apply_one:33` does the single `chmod()` call and keeps going on error. `apply_recursive:49` recurses into children (line 67) before chmodding the directory itself (line 70).

## 4. Concept
- `mode_t` is 12 bits: 9 `rwx` bits (3 per owner/group/other) plus setuid/setgid/sticky. Each octal digit maps to one `rwx` triple: `7 = rwx`, `5 = r-x`, `4 = r--`. So `755` = owner everything, everyone else read+execute.
- `chmod` touches only metadata. File contents, owner, and timestamps are unchanged.
- `umask` does NOT affect `chmod` (it only masks bits on file creation). Whatever mode you pass is what gets set.
- Bottom-up order matters: `chmod -R 644 dir` removes `x` from `dir`, and without `x` on a directory you cannot reach anything inside it. Children-first keeps the parent traversable until we are done inside it.
- On a symlink, `chmod` follows the link and changes the target. That is why the recursion check uses `lstat`: a symlink-to-dir must not be descended into.

## 5. Interview Q
- What does `755` mean? (`rwxr-xr-x`: owner all, others read+execute)
- Why bottom-up for `chmod -R 644`? (parent dir needs `x` to reach children)

## 6. Try It
```bash
./build/bin/mychmod 755 file && stat -c %a file
./build/bin/mychmod -v -R 755 dir
```
