// MicroPython port configuration for VibeOS
// MINIMAL configuration - no floats, no extra modules

#include <stdint.h>

// Use minimal ROM level
#define MICROPY_CONFIG_ROM_LEVEL (MICROPY_CONFIG_ROM_LEVEL_MINIMUM)

// Core features
#define MICROPY_ENABLE_COMPILER           (1)
#define MICROPY_ENABLE_GC                 (1)
#define MICROPY_HELPER_REPL               (1)
#define MICROPY_REPL_AUTO_INDENT          (1)
#define MICROPY_ENABLE_EXTERNAL_IMPORT    (0)

// Disable ALL optional modules
#define MICROPY_PY_JSON                   (0)
#define MICROPY_PY_RE                     (0)
#define MICROPY_PY_RANDOM                 (0)
#define MICROPY_PY_MATH                   (0)
#define MICROPY_PY_CMATH                  (0)
#define MICROPY_PY_STRUCT                 (0)
#define MICROPY_PY_BINASCII               (0)
#define MICROPY_PY_HEAPQ                  (0)
#define MICROPY_PY_COLLECTIONS            (0)
#define MICROPY_PY_HASHLIB                (0)
#define MICROPY_PY_PLATFORM               (0)
#define MICROPY_PY_TIME                   (0)
#define MICROPY_PY_DEFLATE                (0)
#define MICROPY_PY_FRAMEBUF               (0)
#define MICROPY_PY_UCTYPES                (0)
#define MICROPY_PY_ASYNCIO                (0)

// Minimal sys module (REPL needs it for prompts)
#define MICROPY_PY_SYS                    (1)
#define MICROPY_PY_SYS_MODULES            (0)
#define MICROPY_PY_SYS_EXIT               (1)
#define MICROPY_PY_SYS_PATH               (0)
#define MICROPY_PY_SYS_ARGV               (0)
#define MICROPY_PY_SYS_PS1_PS2            (1)
#define MICROPY_PY_SYS_STDIO_BUFFER       (0)
#define MICROPY_PY_OS                     (0)
#define MICROPY_PY_IO                     (0)
#define MICROPY_PY_ERRNO                  (0)
#define MICROPY_PY_SELECT                 (0)
#define MICROPY_PY_THREAD                 (0)
#define MICROPY_PY_MACHINE                (0)
#define MICROPY_PY_NETWORK                (0)

// NO floats - avoids libgcc soft float dependencies
#define MICROPY_FLOAT_IMPL                (MICROPY_FLOAT_IMPL_NONE)
#define MICROPY_LONGINT_IMPL              (MICROPY_LONGINT_IMPL_MPZ)

// Memory
#define MICROPY_HEAP_SIZE                 (2 * 1024 * 1024)  // 2MB
#define MICROPY_ALLOC_PATH_MAX            (256)
#define MICROPY_ALLOC_PARSE_CHUNK_INIT    (32)

// No frozen modules
#undef MICROPY_QSTR_EXTRA_POOL

// Use setjmp for non-local returns (works on aarch64)
#define MICROPY_GCREGS_SETJMP             (1)

// No native code generation
#define MICROPY_EMIT_ARM                  (0)
#define MICROPY_EMIT_THUMB                (0)
#define MICROPY_EMIT_INLINE_THUMB         (0)

// Type definitions
typedef long mp_int_t;
typedef unsigned long mp_uint_t;
typedef long mp_off_t;

#define MP_SSIZE_MAX LONG_MAX

// State
#define MP_STATE_PORT MP_STATE_VM

// Board name
#define MICROPY_HW_BOARD_NAME "VibeOS"
#define MICROPY_HW_MCU_NAME "aarch64"

// No alloca on freestanding - we'll use stack or malloc
#define MICROPY_NO_ALLOCA (1)

// VibeOS console doesn't support VT100 escape codes
#define MICROPY_HAL_HAS_VT100 (0)

// Enable user C modules (for vibe module)
#define MICROPY_MODULE_BUILTIN_INIT (1)
