# `ln` - Minimal (flags: `-s`, `-f`)

Implementation: `src/ln/main.cpp:1`

## 1. What it does
Creates another name for a file. Default = hard link (`myln a b`); `-s` = symlink; `-f` removes an existing dest first.

## 2. Syscall
`link()` adds a directory entry pointing at the same inode; `symlink()` creates a new inode holding a path string.

## 3. Flow
```
args: must be exactly TARGET + LINK_NAME
  |
  v
-f? -- yes --> unlink(LINK) first (ignore failure: nothing there is fine)
  |
  v
-s? -- yes --> symlink(TARGET, LINK)   [new inode holding the path text]
  |
  no
  v
link(TARGET, LINK)  [extra directory entry -> same inode]
  |
  v
any failure --> print "myln: <link>: <reason>", exit 1
  (EEXIST without -f, EPERM for dir hard links, ENOENT for missing target)
```

Step by step (`main:17`): parse `-s`/`-f`, require exactly 2 operands (`main:38`), optionally clear the destination (`main:46`), then exactly one syscall (`main:47`). A hard link is a single dentry insert - no data moves - so there is genuinely nothing else to do.

## 4. Concept
- A hard link is just a second name for the same inode. `stat` on both names shows the same `st_ino`, and `st_nlink` counts how many names point at the inode. Deleting one name decrements the count; data is freed only when the count hits zero and nobody has the file open.
- Hard links cannot cross filesystems (an inode number is only meaningful on its own disk → `EXDEV`) and cannot point at directories (`EPERM` - that would let you build directory loops the filesystem cannot handle).
- A symlink is a separate tiny file whose content is a path string (`/nonexistent` is fine - nobody checks at creation). If the target is later deleted, the link dangles. `readlink` reads that text back.
- `-f` is `unlink` + create. It is not atomic, but for a learning implementation it shows the mechanism plainly.

## 5. Interview Q
- Hard vs symlink? (shared inode vs path string; hard links can't cross filesystems or link dirs)
- What happens to data when the last hard link is unlinked? (freed once no open FDs remain)

## 6. Try It
```bash
./build/bin/myln orig hard && ls -i orig hard
./build/bin/myln -s orig sym && readlink sym
```
