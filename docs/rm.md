# `rm` - Minimal (flags: `-r`, `-f`)

Implementation: `src/rm/main.cpp:1`

## 1. What it does
Removes files with `unlink`. Directories need `-r` (depth-first: empty then `rmdir`). `-f` ignores missing files.

## 2. Syscall
`unlink()` for files/symlinks, `rmdir()` for empty dirs, `lstat` to tell them apart.

## 3. Flow
```
args: files (-- ends flag parsing, so a file named "-f" works)
  |
  v
lstat(path) -- missing --> -f? skip silently : print error
  |
  v
is dir? -- yes --> -r? no: "Is a directory" error
  |                       yes: recurse below
  no
  v
unlink(path)  [file or symlink: gone in one call]
  |
  recurse (remove_recursive:21):
  opendir --> for each child except "." and "..": recurse
  |
  v
  closedir, then rmdir(path)  [dir must be empty first]
```

Step by step: `main:82` classifies every path with `lstat` (a symlink is never a dir, so `rm link` unlinks the link, never its target). `remove_recursive:21` handles one node: missing + `-f` is fine, non-dir is one `unlink` (line 29), a dir is emptied child by child, then the handle is closed (`dir.reset()`, line 45) and `rmdir` removes the empty shell (line 46). Every failure sets `err` but the loop continues - one bad path never blocks the rest.

## 4. Concept
- `unlink` removes a *name*, not data. The file's bytes are freed only when its link count reaches zero AND no process still has it open. That is why `rm` on an open log file frees disk space only after the holder exits.
- `rmdir` refuses non-empty directories (`ENOTEMPTY`). That single kernel rule is the whole reason `-r` must empty depth-first: there is no "recursive delete" syscall.
- `lstat` vs `stat` is load-bearing here: with `stat`, `rm symlinked-dir` would see a directory and start deleting the *target's* contents. With `lstat` it sees a link and unlinks just that.
- `-f` means "missing is not an error" - implemented as a skip wherever `lstat` fails with `ENOENT`, at both levels (`main:85`, `remove_recursive:24`).
- Closing the directory handle before `rmdir` is tidy and portable: the remove happens on a pathname, and holding it open across the call serves no purpose.

## 5. Interview Q
- What happens when you `rm` an open file? (name gone, process keeps reading via its FD)
- Why does `rm` need `-r` for dirs? (`rmdir` only works on empty dirs)

## 6. Try It
```bash
./build/bin/myrm file
./build/bin/myrm -r dir
```
