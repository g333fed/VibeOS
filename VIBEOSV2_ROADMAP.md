# VibeOS v2 Roadmap: The Firefox Challenge

**Goal**: Build a Unix-compatible kernel that can run Firefox.

**Experiment**: Can Claude make a *good* OS?

---

## What We're Building

**VibeOS v2 is a kernel.** Like Linux, but ours.

It runs:
- **VibeOS userspace** (ours): vibesh, desktop, textedit, files, term, coreutils
- **External software** (theirs): Firefox, GCC, Mesa, etc.

This is NOT GNU/VibeOS. We keep our own identity - the retro Mac aesthetic, our shell, our apps. But we're Unix-compatible enough to run the hard stuff we don't want to rewrite.

---

## Architecture Decisions (Locked In)

| Decision | Choice | Rationale |
|----------|--------|-----------|
| MMU | Full virtual memory | Per-process address spaces, required for fork/isolation |
| Scheduling | Preemptive, round-robin, 10ms | Simple, standard, works |
| Kernel | Monolithic | Simpler, faster, Linux did fine |
| HAL | Built during Pi port | Real hardware forces abstraction |
| libc | musl (MIT license) | Clean, portable, not GPL |
| Process model | Full Unix | fork, exec, wait, signals, process groups - software expects it |
| IPC | pipes → signals → unix sockets → shmem | Build up in order of need |
| Filesystem | FAT32 (ext2 later if needed) | Works, macOS-mountable, Firefox doesn't care |
| Display | Retro desktop as compositor | Firefox renders to buffer, we composite into window |
| Security | User/kernel separation only | Protection without complexity |

---

## Userspace: Ours vs External

### Ours (VibeOS Native)
We write and maintain these. They define VibeOS's personality.

- vibesh (shell)
- desktop (retro compositor with Mac System 7 aesthetic)
- term (terminal emulator)
- textedit (text editor)
- files (file manager)
- sysmon (system monitor)
- calc (calculator)
- music (music player)
- coreutils: ls, cat, echo, mkdir, rm, cp, mv, pwd, touch, etc.
- Other apps we want to build

### External (Ported)
Complex software we don't want to rewrite. Use as tools or when needed.

| Software | License | Purpose |
|----------|---------|---------|
| musl | MIT | C library |
| TCC | LGPL | Bootstrap compiler |
| GCC/Clang | GPL/Apache | Production compiler (used as tool) |
| Mesa | MIT | OpenGL implementation |
| Firefox | MPL | Web browser |
| libc++ | Apache/MIT | C++ standard library |
| zlib, libpng, etc. | Various permissive | Support libraries |

**Note on GPL**: GCC is GPL but we only use it as a tool to compile code - it doesn't infect our code. We don't link against GPL libraries.

---

## The Display Architecture

We keep the retro framebuffer desktop. Firefox renders inside it.

```
┌─────────────────────────────────────────────┐
│ [V] File  Edit  View                   12:34│  ← Our menu bar
├─────────────────────────────────────────────┤
│ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ │  ← Our checkerboard desktop
│ ░░ ┌─────────────────────────────────┐ ░░░░ │
│ ░░ │ ▣ Firefox                       │ ░░░░ │  ← Our window chrome
│ ░░ ├─────────────────────────────────┤ ░░░░ │
│ ░░ │                                 │ ░░░░ │
│ ░░ │   Firefox renders here          │ ░░░░ │  ← Firefox draws to buffer
│ ░░ │   (thinks it's Wayland)         │ ░░░░ │  ← We composite into window
│ ░░ │                                 │ ░░░░ │
│ ░░ └─────────────────────────────────┘ ░░░░ │
│ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ │
│  ┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐              │  ← Our dock
└─────────────────────────────────────────────┘
```

How it works:
1. Firefox calls Wayland APIs → our shim intercepts
2. Shim creates GL context that renders to memory buffer (Mesa pbuffer)
3. Firefox renders to that buffer
4. Our desktop takes the buffer, blits it into window content area
5. Our desktop composites window onto framebuffer

Firefox thinks it's talking to Wayland. It's actually feeding pixels to our retro compositor.

---

## Phase 0: Bootstrap Toolchain

Port TCC, then bootstrap GCC/G++.

```
TCC (on VibeOS)
    ↓ compiles
GCC Stage 1 (slow, unoptimized)
    ↓ compiles
GCC Stage 2 (optimized)
    ↓ compiles
G++, libstdc++, everything
```

- [ ] Port TCC to VibeOS (~30K lines)
- [ ] Compile binutils (as, ld) with TCC
- [ ] Compile GCC with TCC
- [ ] Compile GCC with GCC (stage 2)
- [ ] Compile G++
- [ ] Compile make

**TCC needs from VibeOS:**
- File I/O: open, read, write, close, lseek, stat
- Memory: malloc, free, realloc
- Strings: memcpy, memset, strlen, strcmp
- Printf: sprintf, fprintf
- Process: fork, exec, waitpid (for invoking as/ld)

---

## Phase 1: POSIX Kernel

Full Unix process model. This is where v1 becomes v2.

### MMU & Virtual Memory
- [ ] Page table management (4KB pages, 4-level on aarch64)
- [ ] Per-process address spaces (each process sees 0x0-based memory)
- [ ] Kernel mapped into high address of every process
- [ ] Page fault handler
- [ ] Copy-on-write for fork()
- [ ] Demand paging (optional, nice to have)

### File Descriptors
- [ ] Per-process fd table
- [ ] open(), close(), read(), write(), lseek()
- [ ] dup(), dup2()
- [ ] stdin/stdout/stderr (fd 0, 1, 2)
- [ ] stat(), fstat(), lstat()
- [ ] fcntl()
- [ ] pipe()

### Process Control
- [ ] fork()
- [ ] exec() family (execve, execvp, etc.)
- [ ] wait(), waitpid()
- [ ] getpid(), getppid()
- [ ] exit(), _exit()
- [ ] Process groups, sessions (for job control)

### Signals
- [ ] Signal delivery infrastructure
- [ ] signal(), sigaction()
- [ ] kill(), raise()
- [ ] Core signals: SIGINT, SIGTERM, SIGKILL, SIGCHLD, SIGSEGV, SIGPIPE, SIGSTOP, SIGCONT

### Memory
- [ ] mmap(), munmap()
- [ ] mprotect()
- [ ] brk(), sbrk()

### Preemptive Scheduling
- [ ] Timer interrupt triggers scheduler
- [ ] Round-robin with 10ms time slice
- [ ] Priority levels (optional: real-time, normal, background)

### Other
- [ ] getenv(), setenv(), environ
- [ ] getcwd(), chdir()
- [ ] getuid(), getgid() (fake single user, always return 1000)
- [ ] time(), gettimeofday()
- [ ] ioctl()

---

## Phase 2: pthreads

Real multithreading. Firefox spawns many threads.

- [ ] Per-thread stacks (allocated in process address space)
- [ ] Thread-local storage (TLS)
- [ ] pthread_create(), pthread_join(), pthread_exit()
- [ ] pthread_mutex_*()
- [ ] pthread_cond_*()
- [ ] pthread_key_*() (TLS keys)
- [ ] pthread_once()
- [ ] Thread-safe malloc (locking or per-thread arenas)

---

## Phase 3: musl Port

Port musl libc. Provides full C standard library.

### What musl gives us:
- stdio (FILE*, printf, scanf, fopen, etc.)
- stdlib (atoi, qsort, rand, etc.)
- string (all of it)
- ctype (isalpha, etc.)
- math (sin, cos, sqrt, pow, etc.)
- time (strftime, localtime, etc.)
- setjmp/longjmp
- errno
- wchar (wide characters)
- And everything else in C99/C11

### What we provide to musl:
musl needs ~20 syscall stubs. We implement these in the kernel:
- read, write, open, close, lseek
- mmap, munmap, mprotect, brk
- fork, execve, wait4, exit
- kill, sigaction, sigprocmask
- getpid, getuid, gettimeofday
- ioctl, fcntl, stat, fstat
- etc.

---

## Phase 4: C++ Runtime

Port libc++ (Apache/MIT licensed, not GPL).

- [ ] libunwind (stack unwinding)
- [ ] libc++abi (C++ ABI - RTTI, exceptions)
- [ ] libc++ (STL)

This gives us:
- new/delete
- Exceptions (throw/catch)
- RTTI (dynamic_cast, typeid)
- STL (vector, string, map, etc.)
- std::thread, std::mutex (wraps our pthreads)

---

## Phase 5: Display System (Wayland Shim)

Minimal Wayland implementation that feeds buffers to our desktop.

### Wayland Shim (~2-3K lines)
- [ ] wl_display - connection handling
- [ ] wl_surface - drawable surface
- [ ] wl_buffer - pixel buffer (backed by shmem)
- [ ] wl_shm - shared memory
- [ ] wl_keyboard, wl_pointer - input (translate from our events)
- [ ] xdg_surface, xdg_toplevel - window management

### Integration with Desktop
- [ ] Desktop accepts wl_buffer from client
- [ ] Desktop composites buffer into window content area
- [ ] Desktop sends input events back as Wayland events

### EGL Platform
- [ ] EGL platform that creates pbuffer (render-to-memory) contexts
- [ ] Connects Mesa GL to Wayland surfaces

---

## Phase 6: OpenGL (Mesa softpipe)

Software OpenGL via Mesa. Start with softpipe (no LLVM).

- [ ] Port Mesa with softpipe driver
- [ ] OpenGL ES 2.0 (WebRender minimum)
- [ ] GLSL shader compiler (built into Mesa)
- [ ] EGL implementation

Later, if performance matters:
- [ ] Port LLVM
- [ ] Enable llvmpipe (JIT shaders, much faster)

---

## Phase 7: Support Libraries

Libraries Firefox needs. All permissively licensed.

### Must Have
- [ ] zlib (compression)
- [ ] libpng, libjpeg (images)
- [ ] FreeType or keep stb_truetype (fonts)
- [ ] fontconfig (font discovery)
- [ ] harfbuzz (text shaping)
- [ ] SQLite (storage)
- [ ] libffi (foreign function interface)

### Can Maybe Stub/Skip
- [ ] dbus (IPC - might be able to stub)
- [ ] PulseAudio (audio - shim to our virtio-sound)
- [ ] ICU (i18n - Firefox might have fallbacks)

### Already Have (sort of)
- [ ] Networking - we have TCP/UDP/TLS, might need libcurl shim
- [ ] Audio - we have virtio-sound, need PulseAudio API shim

---

## Phase 8: Rust

Firefox uses Rust for WebRender, Stylo, and other components.

- [ ] Create VibeOS target triple: `aarch64-unknown-vibeos`
- [ ] Port Rust std library to VibeOS (uses our libc/pthreads)
- [ ] Cross-compile rustc for VibeOS (or use cross-compilation forever)
- [ ] Build Firefox's Rust components:
  - [ ] WebRender (GPU renderer)
  - [ ] Stylo (CSS engine)
  - [ ] encoding_rs
  - [ ] Various crates

---

## Phase 9: Firefox

The boss fight.

- [ ] Configure Firefox build for VibeOS target
- [ ] Build SpiderMonkey (JS engine)
- [ ] Build Gecko (layout engine)
- [ ] Build WebRender
- [ ] Build Necko (networking) or stub to our stack
- [ ] Link everything
- [ ] Debug for weeks/months
- [ ] Watch it render google.com
- [ ] Celebrate

---

## IPC Priority Order

Build these in order:

1. **Pipes** - Required for shell (`ls | grep`), simple, ~200 lines
2. **Signals** - Required for Ctrl+C, job control, child notification, ~400 lines
3. **Unix domain sockets** - Many programs use for local IPC, ~500 lines
4. **Shared memory** (shmem) - Performance optimization, Wayland uses it, ~300 lines

---

## Estimated Scope

| Phase | New Code | Ported Code | Difficulty |
|-------|----------|-------------|------------|
| 0 - Toolchain | ~1K glue | ~30K TCC | Medium |
| 1 - POSIX Kernel | ~8K | - | Hard |
| 2 - pthreads | ~2K | - | Hard |
| 3 - musl | ~500 syscall stubs | ~100K musl | Medium |
| 4 - C++ runtime | ~100 glue | ~200K libc++ | Medium |
| 5 - Wayland shim | ~3K | - | Medium |
| 6 - Mesa | ~1K glue | ~300K Mesa | Hard |
| 7 - Libraries | ~2K glue | ~500K various | Tedious |
| 8 - Rust | ~1K | Rust std | Hard |
| 9 - Firefox | ~1K glue | 25M Firefox | The Summit |

**Our new code**: ~20K lines
**Ported code**: A lot, but we're not writing it

---

## Milestones

Each milestone is a checkpoint where real software works.

| Milestone | What Works | Unlocks |
|-----------|-----------|---------|
| M1: POSIX basics | fork, exec, pipe, signals | Shell scripts, simple Unix tools |
| M2: pthreads | Multithreading | Threaded applications |
| M3: musl | Full libc | Most C programs |
| M4: C++ | STL, exceptions | Most C++ programs |
| M5: TCC self-host | Compiler runs on VibeOS | Can build software natively |
| M6: GCC | Full compiler | Can build anything |
| M7: Mesa | Software OpenGL | GL applications |
| M8: Wayland shim | GUI apps render to our desktop | GUI programs in retro aesthetic |
| M9: Firefox builds | Firefox compiles | We're close |
| M10: Firefox runs | Firefox renders a page | Victory |

---

## Current Status

**Phase**: Pre-v2 (finishing v1, Pi port planned)

**Next major step**: MMU implementation (biggest single change)

---

## Open Questions

- How to handle Firefox's multiprocess architecture? (content process, GPU process)
- Do we need actual works-correctly Wayland, or can Firefox use X11 via XWayland?
- Can we skip some Rust components and use C++ fallbacks?
- How much can we stub vs actually implement?

---

## Notes

- v1 → Pi port builds HAL naturally (see V1_END_ROADMAP.md)
- v2 starts after Pi port, with cleaner abstracted codebase
- Cross-compile at first, self-hosting is stretch goal
- Each phase unlocks more software - incremental progress
- The retro aesthetic stays. Firefox renders inside our world.
