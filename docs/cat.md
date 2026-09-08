# `cat` Deep Dive: File I/O Fundamentals

- **Implementation:** [src/cat/main.cpp](../src/cat/main.cpp)
- **RAII file-descriptor wrapper:** [unique_fd.h](../src/common/unique_fd.h)

## Contents

1. [What `cat` Actually Does](#1-what-cat-actually-does)
2. [The Core Loop](#2-the-core-loop)
3. [Filesystem Mental Model](#3-filesystem-mental-model)
4. [System Calls Under the Hood](#4-system-calls-under-the-hood)
5. [Processes and Signals](#5-processes-and-signals)
6. [Memory and Buffering](#6-memory-and-buffering)
7. [Permissions and Exit Codes](#7-permissions-and-exit-codes)
8. [Edge Cases Covered](#8-edge-cases-covered)
9. [Performance Checks](#9-performance-checks)
10. [Interview Questions Worth Knowing](#10-interview-questions-worth-knowing)
11. [Follow-up Designs](#11-follow-up-designs)
12. [Try It Yourself](#12-try-it-yourself)
13. [Appendix: Glossary of Jargon](#13-appendix-glossary-of-jargon)

---

## 1. What `cat` Actually Does

`cat` is short for **concatenate** (join together). Its whole job fits in one sentence:

> **Read bytes from files - or from standard input - and write them to standard output, in order.**

It is a pure, streaming "data pump":

```text
open -> read -> write -> close
```

Those four operations, repeated until end-of-file, are *everything* `cat` does. It does **not** seek around inside files, does not interpret line endings (unless you ask for formatting), and does not touch file metadata.

### Quick examples

```bash
echo "hello" | ./build/bin/mycat      # prints: hello
./build/bin/mycat a.txt b.txt         # a.txt, then b.txt
./build/bin/mycat -n report.txt       # report.txt with line numbers
```

### Supported options

| Option | Meaning |
| --- | --- |
| `-n` | Number every output line |
| `-b` | Number only **non-empty** output lines (overrides `-n`, like GNU `cat`) |
| `-E` | Show `$` at the end of each line |
| `-` | A file name of `-` means "read standard input" |
| `--` | End of options, so a file literally named `-foo` can be opened |

Quick sanity check against GNU `cat`:

```bash
diff <(./build/bin/mycat -n file) <(cat -n file) && echo ok
```

---

## 2. The Core Loop

Every run of `mycat` is the same four-step loop, repeated until end-of-file:

```text
        start
          │
          ▼
  open(path, O_RDONLY | O_CLOEXEC)    ──►   fd = 3
          │                                 (or stdin, fd 0, if no file given)
          ▼
  read(fd, buf, 64 KiB)
          │
    ┌─────┴─────┐
    │           │
 n > 0        n == 0
 (data)       (EOF)
    │           │
    ▼           ▼
 write ALL  close(fd)
 n bytes    exit(0)
 to stdout
    │
    ▼
 read again
```

**Step by step:**

1. **`open`** - ask the kernel to open the path read-only (`O_RDONLY`). The kernel returns a *file descriptor* (FD): a small integer that stands for "this open file".
2. **`read`** - copy up to 64 KiB from the file into the program's buffer. Three outcomes:
   - `n > 0`: got data → write it, then read again.
   - `n == 0`: end of file → stop.
   - error `EINTR`: a signal interrupted the call → retry.
3. **`write`** - copy the buffer to standard output. A `write` may only accept *part* of the buffer (a *short write*), so the code loops until every byte is written.
4. **`close`** - release the FD. Here it is handled by `UniqueFd`, a small **RAII** wrapper, so the descriptor is cleaned up even on early error returns.

When no file is given, `mycat` skips `open` and reads from **fd 0** (standard input). Nothing magical happens there.

---

## 3. Filesystem Mental Model

`read`, `write`, and friends talk to the kernel, so it helps to know the kernel's picture of "an open file". There are three layers:

```text
Process
  │
  ├── file descriptor table            a small array of numbers:
  │        fd 0 ──► stdin              "which open files does
  │        fd 1 ──► stdout             this process know about?"
  │        fd 2 ──► stderr
  │        fd 3 ──► mycat file.txt
  │                │
  │                ▼
  ├── open file description            kernel-side state:
  │        • current read/write offset
  │        • status flags (O_RDONLY, O_CLOEXEC, ...)
  │                │
  │                ▼
  └── inode                            the file itself:
         • data blocks on disk
         • size, permissions, timestamps
```

- **File descriptor table** lives in the process. Each FD is just a pointer into the kernel's table of open file descriptions. Two descriptors in two processes can point at the *same* description (for example after `fork`), and two descriptions can point at the *same* inode (for example the same path opened twice).
- **Open file description** holds *state* such as the current offset. This is why two `open()` calls on the same file can read independently - each has its own offset.
- **Inode** is the on-disk object. The path you type is only a name the kernel resolves to an inode.

### The page cache

Reading from disk is slow, so the kernel keeps recently read blocks in a **page cache** in memory. For `cat` this means:

- Repeated reads of the same file get cheaper after the first disk access.
- The user-space 64 KiB buffer still matters: each `read` is a system call, and system calls have fixed overhead, so fewer, bigger reads beat many tiny ones.
- `fstat` reads cached inode metadata, so telling a regular file from a directory costs almost nothing and needs no guesswork from the path string.

---

## 4. System Calls Under the Hood

`mycat` uses exactly a handful of interesting system calls:

| Call | What it does | Why it matters here |
| --- | --- | --- |
| `open(path, flags)` | Open the input read-only (flags: `O_RDONLY` and `O_CLOEXEC`) | Gives clean errors: `ENOENT` = no such file, `EACCES` = permission denied, `EISDIR` = it's a directory. `O_CLOEXEC` auto-closes the FD if the process ever `exec`s, so no FDs leak into child programs |
| `read(fd, buf, 64 KiB)` | Fill the buffer | Returns `0` at end of file. Returns `EINTR` if a signal interrupted the call - the loop must **retry**, or data is quietly lost |
| `write(STDOUT_FILENO, ...)` | Copy bytes to stdout | May write only *part* of the buffer (a *short write*), so the code loops until everything is out. Handles `EINTR` (retry) and `EPIPE` (the reader went away - quit quietly) |
| `fstat(fd, ...)` | Ask for file metadata | Lets `mycat` reject directories up front with `EISDIR`, before trying to stream them |
| `close(fd)` | Release the descriptor | Handled by `UniqueFd`, so cleanup happens even on early returns |
| `signal(SIGPIPE, SIG_IGN)` | Set signal policy once at startup | Turns a broken pipe into an ordinary `write` error, so `mycat` can sit quietly at the front of a pipeline ([§5](#5-processes-and-signals)) |

### Why `read`/`write` instead of `fread`/`fwrite`?

The C library routines (`FILE*`) and C++ streams add their own buffering on top of these system calls. The POSIX calls make **buffering, retries, partial writes, and broken pipes explicit** - you can see exactly when data moves. That is more code, but it is precisely the code we are here to understand. `printf("hi")` hides all of it; `write(1, "hi", 2)` hides none of it.

---

## 5. Processes and Signals

`mycat` is **one process and one thread**. It does not `fork`, and it spawns no helpers. Everything happens in the main thread, in a loop.

### The broken-pipe scenario

Consider a pipeline:

```bash
mycat bigfile | head -n1
```

`head` reads one line and exits immediately. The next `write` from `mycat` hits a pipe with no reader, and the kernel raises **`SIGPIPE`**:

1. By default, `SIGPIPE` would *kill* the process before it could even look at the error.
2. `mycat` calls `signal(SIGPIPE, SIG_IGN)` at startup, asking to ignore the signal.
3. The kernel now reports the situation as an ordinary `write` error: `errno == EPIPE`.
4. The write loop sees `EPIPE` and stops - no endless wasted output, no alarming message, just a quiet exit.

This is exactly the behavior you want from a tool designed to sit at the front of a pipeline.

---

## 6. Memory and Buffering

### Why 64 KiB?

The buffer size is `kBufSize = 64 * 1024`. It is a practical middle ground:

- **4 KiB** (one disk page) means roughly 16× more system calls for the same data.
- **1 MiB** is not automatically faster and pushes more pressure onto CPU caches for data that is only going to be streamed onward.

With a 64 KiB buffer, a 1 GiB file costs about 16,000 `read` calls - a good balance between call overhead and memory use.

### Two code paths

| Path | When it runs | What it does |
| --- | --- | --- |
| **Bulk** - `cat_bulk` | No formatting options (`-n`, `-b`, `-E`) | Read a buffer, write it out unchanged. Best case is nearly a pure memory copy at disk or pipe speed. Roughly O(n) in input size, with about `filesize / 64K` reads |
| **Option** - `cat_with_options` | Any of `-n`, `-b`, `-E` | Scan for line boundaries, then build the formatted output (numbers, `$` markers). Also O(n), but with extra per-line work |

The buffer is a `std::vector<char>`. Could we `mmap` the file instead? Yes, but for a *sequential streaming* workload `mmap` adds kernel-side complexity (page faults, mapping bookkeeping) without helping, and it makes pipe and partial-input handling harder. The vector is the simplest tool that is exactly right here.

### Possible future experiments

- `copy_file_range(2)` - kernel-side copy with no user-space round trip.
- `sendfile(2)` - copy directly from one FD to another.
- `posix_fadvise(POSIX_FADV_SEQUENTIAL)` - hint the kernel to prefetch.

Try them, **measure** them, and keep them only if they win. Kernel bypass is not a personality trait.

---

## 7. Permissions and Exit Codes

`mycat` needs:

- **read** permission on every input file, and
- **write** permission on standard output.

There is no permission *change* anywhere - the code only ever reads from inputs and writes to stdout.

| Situation | Exit code |
| --- | --- |
| All inputs copied completely | `0` |
| At least one input failed (missing, unreadable, a directory, …) | `1`, after reporting the error and **continuing** with the remaining files |

This matches the useful part of GNU `cat`: report the bad file, keep going, exit non-zero only if something failed.

---

## 8. Edge Cases Covered

| Case | Behavior |
| --- | --- |
| No arguments | Reads standard input |
| A file named `-` | Reads standard input, even when it appears *between* real file names |
| `--` on the command line | Ends option parsing, so a file literally named `-foo` can be opened |
| Missing or inaccessible file | Prints `program name: file name: error message` (via `strerror`), continues with the rest, then exits `1` |
| Directory as input | Rejected with `EISDIR` (checked via `fstat` *before* streaming) |
| Empty files and binary data | Fine - the program does not assume text, so `\0` and other bytes pass through untouched |
| Final line without `\n` | Flushed as-is; no invented newline and no invented `$` marker |
| `SIGPIPE` / `EINTR` | Handled in the read/write loops (see [§5](#5-processes-and-signals)) |
| `-b` and `-n` together | `-b` wins, matching GNU `cat` |
| Huge files | Streamed with **constant memory** - the whole file never takes a vacation in RAM |

---

## 9. Performance Checks

Measure with a **real** file; a tiny file mostly measures process startup and noise.

```bash
# Rough runtime comparison vs GNU cat
time ./build/bin/mycat bigfile > /dev/null
time cat bigfile > /dev/null

# See how many system calls the buffer size buys us
strace -c ./build/bin/mycat bigfile > /dev/null
```

`strace -c` prints a summary like "~1600 reads, ~1600 writes" - a concrete feel for why a 64 KiB buffer beats 4 KiB.

---

## 10. Interview Questions Worth Knowing

Each question maps to a section above - a good way to turn "I read it" into "I know it".

1. **What is the difference between an FD, a `FILE*`, and a C++ stream?** An FD is the kernel's handle; `FILE*` and C++ streams add user-space buffering on top. ([§4](#4-system-calls-under-the-hood))
2. **What happens when `mycat bigfile | head -n1` closes the pipe?** `SIGPIPE` is ignored, `write` returns `EPIPE`, and `mycat` stops quietly. ([§5](#5-processes-and-signals))
3. **Why retry after `EINTR`?** The signal interrupted the system call before it did its work; not retrying would lose data and break the "read everything" contract. ([§4](#4-system-calls-under-the-hood))
4. **Why a 64 KiB buffer instead of reading one byte at a time?** Every system call has fixed overhead; one-byte reads would mean thousands of times more calls for the same data. ([§6](#6-memory-and-buffering))
5. **How would you handle a 100 GB file without loading it into memory?** Stream it in fixed-size chunks; memory stays constant no matter the file size. ([§6](#6-memory-and-buffering))
6. **Why pass `O_CLOEXEC` to `open`?** So the FD is closed automatically if the process ever `exec`s; otherwise child programs inherit descriptors the parent never meant to share. ([§4](#4-system-calls-under-the-hood))
7. **What does the kernel do with the page cache during `read`?** It checks whether the block is already cached - if so, it copies straight into your buffer (fast path); if not, it fetches from disk first, then copies. ([§3](#3-filesystem-mental-model))

---

## 11. Follow-up Designs

Each of these is `cat` plus one new idea - a good way to learn one kernel feature at a time.

| Command | Design sketch |
| --- | --- |
| `tac` (reverse lines) | Read from the end with `lseek`, then process backwards |
| `tail -f` | Keep the file open and use `inotify` to notice changes |
| `bat` or `less` | Add paging and terminal-aware rendering (ANSI colors, syntax highlighting) |
| Distributed `cat` for object storage | Fetch ranges in parallel, preserve output order |

---

## 12. Try It Yourself

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

./build/bin/mycat --help
./build/bin/mycat -n -E file.txt
echo "hi" | ./build/bin/mycat -n -

# Quick diff against GNU cat, with and without options
diff <(./build/bin/mycat file) <(cat file) && echo ok
diff <(./build/bin/mycat -n file) <(cat -n file) && echo ok

# Watch the machinery
strace -e trace=open,read,write ./build/bin/mycat file.txt
strace -c ./build/bin/mycat bigfile > /dev/null
```

---

## 13. Appendix: Glossary of Jargon

| Term | Plain meaning |
| --- | --- |
| **FD / file descriptor** | A small integer (0, 1, 2, 3, …) a process uses to name an open file. 0 = stdin, 1 = stdout, 2 = stderr |
| **EOF** | End of file; `read` signals it by returning `0` |
| **`errno`** | A per-thread error code set by a failed system call (`ENOENT`, `EACCES`, `EISDIR`, `EINTR`, `EPIPE`, …). `strerror(errno)` turns it into readable text |
| **`EINTR`** | "Interrupted by a signal" - the call did not complete; retry it |
| **`EPIPE`** | "Broken pipe" - the reader at the other end of the pipe went away |
| **`EISDIR`** | "It's a directory" - a directory cannot be streamed like a regular file |
| **`O_CLOEXEC`** | Open flag that closes the FD automatically if the process ever `exec`s a new program |
| **inode** | The on-disk object holding a file's data blocks, size, permissions, and timestamps; paths are names that resolve to inodes |
| **page cache** | The kernel's in-memory stash of recently read disk blocks, making repeat reads fast |
| **RAII** | "Resource Acquisition Is Initialization" - a C++ pattern where the constructor acquires a resource (opens the FD) and the destructor releases it (`UniqueFd` closes the FD) |
| **Short write** | When `write` accepts fewer bytes than asked, common on pipes, sockets, and under signals - which is why write loops exist |