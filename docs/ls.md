# `ls` - Minimal (flags: `-a`, `-l`)

Implementation: `src/ls/main.cpp:1`

## 1. What it does
Lists directory entries alphabetically. `-a` shows dotfiles, `-l` shows `mode size name` per entry (symlinks as `name -> target`).

## 2. Syscall
`opendir` / `readdir` (→ `getdents64`) to iterate, `lstat` per entry so symlinks are reported, not followed.

## 3. Flow
```
args: flags (-a/-l) + paths (default ".")
  |
  v
opendir(path) -- fails --> print "myls: <path>: <reason>", exit 1
  |
  v
readdir loop: collect names
  |  (skip leading-dot names unless -a)
  v
sort names (plain byte order)
  |
  v
-l? -- no --> print name
  |
  yes
  v
lstat(path/name) -- fails --> print error, continue next
  |
  v
format: mode_string + size + name
  |  (symlink? append " -> " + readlink target)
  v
print line
```

Step by step (`list_dir:42`): open the dir (auto-closed by `UniqueDir`), gather names while filtering dotfiles (`list_dir:52`), sort (`list_dir:55`), then either print names directly or `lstat` each full path and format the long line (`list_dir:64`). One bad entry never stops the rest; the exit code remembers it.

## 4. Concept
- A directory is a table mapping names to inodes. `readdir` gives you only the names; everything else (type, size, permissions) lives in the inode and needs `lstat`.
- `lstat` reads the link's own inode; `stat` follows the link to its target. `ls -l` must use `lstat`, otherwise a symlink would print as its target's type and size.
- Hidden files are pure convention: the kernel returns dotfiles like any other entry, userspace filters them. That is all `-a` toggles.
- Sorting happens in userspace after collecting every name, so memory grows with directory size. GNU also sorts; the cost is `O(n log n)` plus one `lstat` per entry in `-l` mode.
- `mode_string` (`mode_string:21`) decodes `st_mode`: first char is the file type (`d`/`l`/`-`), then the 9 `rwx` bits.

## 5. Interview Q
- `stat` vs `lstat` - when does it matter? (symlinks: `ls -l` must not follow)
- Why is `ls` slow on huge dirs? (`lstat` per entry + `O(n log n)` sort)

## 6. Try It
```bash
./build/bin/myls -a /tmp
./build/bin/myls -l /tmp
```
