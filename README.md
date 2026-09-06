# Linux Commands: C++ Reimplementation

Rebuilding 10 Linux commands from scratch with **Modern C++20 and POSIX** to learn Linux internals and system design.

## Command List
- `mycat` :- (`-n -b -E`, bulk + line mode, SIGPIPE/EINTR, 64K buffer, RAII `UniqueFd`)
- `myls` 
- `mychmod`
- `myln`
- `myrm`
- `mycp`
- `mymv`
- `myfind`
- `mytar`
- `myrsync`

### Build
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4

./build/bin/mycat --help
bash tests/test_cat.sh ./build/bin/mycat
```

### Structure
```
src/common/include/common/  # UniqueFd, error helpers
src/cat/                    # cat command
docs/cat.md                 # deep dive
tests/test_cat.sh           # golden tests vs GNU cat
```

