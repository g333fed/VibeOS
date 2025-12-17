/*
 * VibeOS Help Viewer
 * Comprehensive documentation browser
 */

#include "../lib/vibe.h"
#include "../lib/gfx.h"

static kapi_t *api;
static int window_id = -1;
static uint32_t *win_buffer;
static int win_w, win_h;
static gfx_ctx_t gfx;

// ============ Colors ============
#define BLACK   0x00000000
#define WHITE   0x00FFFFFF
#define GRAY    0x00808080

// ============ Layout ============
#define SIDEBAR_W       180
#define SECTION_ITEM_H  24
#define PADDING         12
#define LINE_HEIGHT     18

// ============ Help Sections ============
typedef struct {
    const char *title;
    const char *content;  // Multi-line content (newline-separated)
} help_section_t;

static help_section_t sections[] = {
    {
        "About VibeOS",
        "VibeOS - A Retro Operating System\n"
        "\n"
        "VibeOS is a hobby operating system built from scratch for ARM64\n"
        "(aarch64) architecture, targeting QEMU's virt machine and\n"
        "Raspberry Pi Zero 2W hardware.\n"
        "\n"
        "Design Philosophy:\n"
        "- Retro aesthetic inspired by Mac System 7 and Apple Lisa\n"
        "- 1-bit black & white visual style\n"
        "- Simple, educational, nostalgic\n"
        "- Monolithic kernel architecture (like Windows 3.1)\n"
        "\n"
        "Features:\n"
        "- Graphical user interface with windows, menus, mouse\n"
        "- FAT32 filesystem with persistent storage\n"
        "- Networking stack (Ethernet, IP, TCP/UDP, DNS)\n"
        "- HTTPS support via TLS 1.2\n"
        "- Audio playback (WAV and MP3)\n"
        "- Python scripting via MicroPython\n"
        "- Multitasking with cooperative scheduling\n"
        "- Rich developer API (C and Python)\n"
        "\n"
        "Version: 1.0\n"
        "Built with: aarch64-elf-gcc\n"
        "License: Hobby project, open source\n"
    },
    {
        "Getting Started",
        "Welcome to VibeOS!\n"
        "\n"
        "The Desktop:\n"
        "When you boot VibeOS, you'll see the desktop with a menu bar at\n"
        "the top and a dock at the bottom.\n"
        "\n"
        "Menu Bar:\n"
        "- Apple menu: About VibeOS, Quit\n"
        "- File menu: New, Open (varies by app)\n"
        "- Edit menu: Cut, Copy, Paste (varies by app)\n"
        "\n"
        "The Dock:\n"
        "Click any icon in the dock to launch an application:\n"
        "- Terminal: Command-line shell\n"
        "- TextEdit: Simple text editor\n"
        "- Music: MP3/WAV music player\n"
        "- Browser: Web browser with HTTP/HTTPS support\n"
        "- Calculator: Desktop calculator\n"
        "- System Monitor: View CPU, memory, uptime\n"
        "\n"
        "Windows:\n"
        "- Drag the title bar to move windows\n"
        "- Click the close box (top-left) to close\n"
        "- Mouse wheel scrolls in scrollable areas\n"
        "\n"
        "First Steps:\n"
        "1. Open Terminal and try 'ls' to list files\n"
        "2. Try 'cd /bin' to explore the filesystem\n"
        "3. Run 'ping 1.1.1.1' to test networking\n"
        "4. Open Browser and visit example.com\n"
        "5. Create a file with TextEdit\n"
    },
    {
        "Using VibeOS",
        "Applications:\n"
        "\n"
        "Terminal:\n"
        "- Full POSIX-style shell with job control\n"
        "- Commands: ls, cd, pwd, cat, echo, mkdir, rm, touch, vi\n"
        "- Utilities: ping, fetch, date, snake, tetris, doom\n"
        "- Redirects: echo \"hello\" > file.txt\n"
        "- Tab completion and command history\n"
        "\n"
        "TextEdit:\n"
        "- Create and edit text files\n"
        "- File > Save As to save with a new name\n"
        "- Full keyboard support\n"
        "\n"
        "Browser:\n"
        "- Enter URL in address bar and press Enter\n"
        "- Supports HTTP and HTTPS\n"
        "- Basic HTML rendering (text, links, images)\n"
        "- Click links to navigate\n"
        "- Mouse wheel to scroll\n"
        "\n"
        "Music Player:\n"
        "- Plays MP3 and WAV files from /music directory\n"
        "- Click album to see tracks\n"
        "- Double-click track to play\n"
        "- Play/Pause/Stop controls\n"
        "- Volume slider\n"
        "\n"
        "Calculator:\n"
        "- Standard desktop calculator\n"
        "- Click buttons or use keyboard\n"
        "- Supports +, -, *, /, sqrt, %\n"
        "\n"
        "Filesystem:\n"
        "Directory structure:\n"
        "  /bin       - Programs and utilities\n"
        "  /etc       - Configuration files\n"
        "  /home/user - Your home directory\n"
        "  /tmp       - Temporary files\n"
        "  /music     - Audio files\n"
        "\n"
        "The filesystem is FAT32 and persistent. Files you create are\n"
        "saved to disk.img and survive reboots.\n"
    },
    {
        "Developer Docs",
        "VibeOS API - C Language\n"
        "\n"
        "Programs receive a kapi_t struct with kernel functions:\n"
        "\n"
        "Memory:\n"
        "  void *malloc(unsigned long size);\n"
        "  void free(void *ptr);\n"
        "\n"
        "I/O:\n"
        "  void puts(const char *s);\n"
        "  void putchar(char c);\n"
        "  char getchar(void);           // blocking\n"
        "  char getchar_nb(void);        // non-blocking, returns 0 if none\n"
        "  int printf(const char *fmt, ...);\n"
        "\n"
        "Files:\n"
        "  void *open(const char *path, const char *mode);\n"
        "  int close(void *file);\n"
        "  int read(void *file, void *buf, int size);\n"
        "  int write(void *file, const void *buf, int size);\n"
        "  int seek(void *file, int offset, int whence);\n"
        "  void *opendir(const char *path);\n"
        "  const char *readdir(void *dir);\n"
        "  void closedir(void *dir);\n"
        "  int stat(const char *path, void *stat_out);\n"
        "  int mkdir(const char *path);\n"
        "  int unlink(const char *path);\n"
        "\n"
        "Processes:\n"
        "  void yield(void);             // cooperative multitasking\n"
        "  void exit(int code);\n"
        "  int spawn(const char *path);\n"
        "  int exec(const char *path);   // replace current process\n"
        "  int exec_args(const char *path, int argc, char **argv);\n"
        "  unsigned long get_pid(void);\n"
        "  void sleep_ms(unsigned long ms);\n"
        "\n"
        "Windowing:\n"
        "  int create_window(const char *title, int x, int y, int w, int h);\n"
        "  void *get_window_buffer(int wid, int *w, int *h);\n"
        "  void update_window(int wid);\n"
        "  void close_window(int wid);\n"
        "  int poll_event(int wid, int *type, int *x, int *y, int *key);\n"
        "\n"
        "Event types:\n"
        "  EVENT_MOUSE_DOWN  = 1\n"
        "  EVENT_MOUSE_UP    = 2\n"
        "  EVENT_MOUSE_MOVE  = 3\n"
        "  EVENT_KEY_DOWN    = 4\n"
        "  EVENT_CLOSE       = 5\n"
        "  EVENT_MOUSE_WHEEL = 6\n"
        "\n"
        "Network:\n"
        "  int socket(int domain, int type, int protocol);\n"
        "  int connect(int sockfd, const char *host, int port);\n"
        "  int send(int sockfd, const void *buf, int len, int flags);\n"
        "  int recv(int sockfd, void *buf, int len, int flags);\n"
        "  int close_socket(int sockfd);\n"
        "\n"
        "Audio:\n"
        "  void play_audio(const int16_t *samples, uint32_t count,\n"
        "                  uint32_t sample_rate, int channels);\n"
        "  void stop_audio(void);\n"
        "  int is_audio_playing(void);\n"
        "\n"
        "Time:\n"
        "  unsigned long get_uptime_ms(void);\n"
        "  unsigned long get_uptime_ticks(void);  // 100Hz timer ticks\n"
        "  unsigned long get_rtc_time(void);      // Unix timestamp\n"
        "\n"
        "See /lib/tcc/include/vibe.h for full API reference.\n"
    },
    {
        "Python API",
        "VibeOS API - Python (MicroPython)\n"
        "\n"
        "Import the vibe module:\n"
        "  import vibe\n"
        "\n"
        "Console I/O:\n"
        "  print(\"Hello\")              # Output text\n"
        "  input(\"Prompt: \")           # Read line (blocking)\n"
        "\n"
        "Files:\n"
        "  f = open(\"/path/to/file\", \"r\")  # Open file\n"
        "  data = f.read()                  # Read all\n"
        "  f.close()                        # Close\n"
        "  \n"
        "  with open(\"file.txt\", \"w\") as f:\n"
        "      f.write(\"hello\\n\")\n"
        "\n"
        "Process:\n"
        "  vibe.yield()                     # Cooperative yield\n"
        "  vibe.exit(0)                     # Exit program\n"
        "  vibe.spawn(\"/bin/ls\")           # Spawn process\n"
        "  vibe.sleep_ms(1000)              # Sleep 1 second\n"
        "\n"
        "Windowing:\n"
        "  wid = vibe.create_window(\"Title\", x, y, w, h)\n"
        "  buf = vibe.get_window_buffer(wid)  # Returns (buffer, width, height)\n"
        "  vibe.update_window(wid)\n"
        "  vibe.close_window(wid)\n"
        "  \n"
        "  evt = vibe.poll_event(wid)  # Returns (type, x, y, key) or None\n"
        "\n"
        "Drawing:\n"
        "  # buffer is a list of 32-bit ARGB pixels\n"
        "  buffer[y * width + x] = 0x00FFFFFF  # White pixel\n"
        "  buffer[y * width + x] = 0x00000000  # Black pixel\n"
        "\n"
        "Network:\n"
        "  import socket\n"
        "  s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)\n"
        "  s.connect((\"example.com\", 80))\n"
        "  s.send(b\"GET / HTTP/1.0\\r\\n\\r\\n\")\n"
        "  data = s.recv(1024)\n"
        "  s.close()\n"
        "\n"
        "Time:\n"
        "  vibe.get_uptime_ms()        # Milliseconds since boot\n"
        "  vibe.get_rtc_time()         # Unix timestamp\n"
        "\n"
        "Example Program:\n"
        "  #!/bin/micropython\n"
        "  import vibe\n"
        "  \n"
        "  wid = vibe.create_window(\"Hello\", 100, 100, 300, 200)\n"
        "  buf, w, h = vibe.get_window_buffer(wid)\n"
        "  \n"
        "  # Fill with white\n"
        "  for i in range(w * h):\n"
        "      buf[i] = 0x00FFFFFF\n"
        "  \n"
        "  vibe.update_window(wid)\n"
        "  \n"
        "  while True:\n"
        "      evt = vibe.poll_event(wid)\n"
        "      if evt and evt[0] == 5:  # EVENT_CLOSE\n"
        "          break\n"
        "      vibe.yield()\n"
        "  \n"
        "  vibe.close_window(wid)\n"
        "\n"
        "See /user/lib/vibe.py for Python module reference.\n"
    },
    {
        "FAQ",
        "Frequently Asked Questions\n"
        "\n"
        "Q: What hardware does VibeOS run on?\n"
        "A: VibeOS runs on QEMU's virt machine (aarch64) and Raspberry Pi\n"
        "   Zero 2W. QEMU is the primary development platform.\n"
        "\n"
        "Q: Can I run Linux programs on VibeOS?\n"
        "A: No. VibeOS is not Linux-compatible. Programs must be compiled\n"
        "   specifically for VibeOS using the VibeOS API.\n"
        "\n"
        "Q: Is VibeOS POSIX-compliant?\n"
        "A: No. VibeOS has a POSIX-like shell and some POSIX-like APIs,\n"
        "   but it's not a full POSIX system.\n"
        "\n"
        "Q: Does VibeOS have memory protection?\n"
        "A: No. VibeOS uses a flat memory model with no MMU. All code runs\n"
        "   in kernel space. Think Windows 3.1, not modern OSes.\n"
        "\n"
        "Q: How does multitasking work?\n"
        "A: Cooperative multitasking. Programs call yield() to give up the\n"
        "   CPU. The scheduler uses round-robin scheduling.\n"
        "\n"
        "Q: Can I add more RAM?\n"
        "A: In QEMU, yes. Use -m flag: qemu-system-aarch64 -m 512M ...\n"
        "   VibeOS detects RAM size from the device tree at boot.\n"
        "\n"
        "Q: Why 1-bit black & white graphics?\n"
        "A: Aesthetic choice. VibeOS aims for retro Mac System 7 vibes.\n"
        "   The framebuffer is actually 32-bit color, but we only use\n"
        "   black (0x00000000) and white (0x00FFFFFF).\n"
        "\n"
        "Q: Can I connect to WiFi?\n"
        "A: Not currently. VibeOS has Ethernet networking via virtio-net\n"
        "   in QEMU. WiFi drivers are not implemented.\n"
        "\n"
        "Q: How do I transfer files to/from VibeOS?\n"
        "A: Mount disk.img on your host OS (macOS: hdiutil attach disk.img)\n"
        "   and copy files. Unmount before running QEMU.\n"
        "\n"
        "Q: Can I port X to VibeOS?\n"
        "A: Maybe! If X is written in C and doesn't require POSIX-specific\n"
        "   features, you can try porting it. Start with small programs.\n"
        "\n"
        "Q: Where's the source code?\n"
        "A: VibeOS is a hobby project. The entire OS source is in the\n"
        "   /kernel and /user directories.\n"
        "\n"
        "Q: Why does DOOM run at 35 FPS?\n"
        "A: DOOM's internal timer is tied to a 35Hz tic rate. That's the\n"
        "   original game's design, not a VibeOS limitation.\n"
        "\n"
        "Q: Can I write GUI apps in Python?\n"
        "A: Yes! See the Python API section. The browser is written in\n"
        "   Python as a demonstration.\n"
    },
    {
        "Hardware Support",
        "Supported Hardware:\n"
        "\n"
        "Primary Platform: QEMU virt machine (aarch64)\n"
        "- CPU: Cortex-A72 emulation\n"
        "- RAM: 256MB - 4GB+ (auto-detected)\n"
        "- Display: ramfb framebuffer (800x600)\n"
        "- Storage: virtio-blk block device\n"
        "- Input: virtio-input keyboard and mouse/tablet\n"
        "- Network: virtio-net Ethernet\n"
        "- Audio: virtio-sound\n"
        "- RTC: PL031 real-time clock\n"
        "- UART: PL011 serial console\n"
        "- Interrupts: GIC-400 (Generic Interrupt Controller)\n"
        "\n"
        "Secondary Platform: Raspberry Pi Zero 2W\n"
        "- CPU: BCM2710A1 (Cortex-A53 quad-core)\n"
        "- RAM: 512MB\n"
        "- Display: Framebuffer (HDMI or composite)\n"
        "- Storage: SD card via EMMC controller\n"
        "- Input: USB keyboard via DWC2 USB host controller\n"
        "- GPIO: BCM2835-compatible GPIO\n"
        "- UART: Mini UART (UART1)\n"
        "- No networking on Pi (no Ethernet hardware)\n"
        "- No audio on Pi (not implemented)\n"
        "\n"
        "Device Drivers:\n"
        "- Virtio block: Read/write sectors\n"
        "- Virtio network: Ethernet, ARP, IP, TCP, UDP, ICMP\n"
        "- Virtio keyboard: PS/2-style scancodes\n"
        "- Virtio mouse/tablet: Absolute positioning\n"
        "- Virtio sound: PCM audio playback\n"
        "- FAT32: Full read/write with long filename support\n"
        "- PL011 UART: Serial console\n"
        "- PL031 RTC: Real-time clock\n"
        "- GIC-400: Interrupt routing and handling\n"
        "- BCM EMMC: SD card controller (Pi only)\n"
        "- DWC2 USB: USB 2.0 Full-Speed host (Pi only)\n"
        "\n"
        "Boot Process:\n"
        "1. Boot ROM loads bootloader at 0x0 or 0x80000 (Pi)\n"
        "2. Bootloader sets up stack, clears BSS, copies .data\n"
        "3. Bootloader transitions EL3 -> EL1\n"
        "4. Jump to kernel_main()\n"
        "5. Kernel parses device tree for RAM size\n"
        "6. Initialize heap, drivers, filesystem\n"
        "7. Mount FAT32 from disk.img or SD card\n"
        "8. Spawn init process (/bin/desktop)\n"
        "9. Desktop loads and shows dock\n"
        "\n"
        "Memory Map (QEMU):\n"
        "  0x00000000 - Flash (bootloader)\n"
        "  0x08000000 - GIC (interrupts)\n"
        "  0x09000000 - UART\n"
        "  0x0A000000 - RTC\n"
        "  0x0A003E00 - Virtio devices\n"
        "  0x40000000 - RAM start\n"
        "  0x40200000 - Kernel .text/.data/.bss\n"
        "  0x41000000 - Heap and process memory\n"
        "  0x4F000000 - Kernel stack\n"
        "\n"
        "Memory Map (Pi):\n"
        "  0x00000000 - RAM start\n"
        "  0x00008000 - Kernel load address\n"
        "  0x3F000000 - Peripherals (GPIO, UART, etc.)\n"
        "  0x3F980000 - DWC2 USB controller\n"
        "  0x3F300000 - EMMC controller\n"
    },
    {
        "Limitations",
        "Known Limitations:\n"
        "\n"
        "Architecture:\n"
        "- No memory protection (no MMU, flat memory model)\n"
        "- No preemptive multitasking (cooperative only)\n"
        "- No virtual memory or paging\n"
        "- No privilege separation (all code runs in EL1)\n"
        "- No process isolation (shared address space)\n"
        "\n"
        "Filesystem:\n"
        "- FAT32 only (no ext4, NTFS, etc.)\n"
        "- No symbolic links\n"
        "- No file permissions or ownership\n"
        "- Maximum file size: 4GB (FAT32 limit)\n"
        "- Maximum disk size: 2TB (FAT32 limit, we use 64MB)\n"
        "\n"
        "Networking:\n"
        "- Ethernet only (no WiFi)\n"
        "- No IPv6 (IPv4 only)\n"
        "- No DHCP (static IP: 10.0.2.15)\n"
        "- TLS 1.2 only (no TLS 1.3)\n"
        "- No certificate verification (trust on first use)\n"
        "- Basic TCP (no congestion control, retransmits)\n"
        "\n"
        "Display:\n"
        "- Fixed 800x600 resolution\n"
        "- 1-bit aesthetic (only black and white used)\n"
        "- No hardware acceleration\n"
        "- No VSync (can tear during drawing)\n"
        "\n"
        "Input:\n"
        "- No mouse acceleration or smoothing\n"
        "- No keyboard layout selection (US only)\n"
        "- No copy/paste between host and VibeOS\n"
        "\n"
        "Audio:\n"
        "- Playback only (no recording)\n"
        "- WAV and MP3 formats only\n"
        "- One stream at a time (no mixing)\n"
        "- Sample rate limited to 48kHz\n"
        "\n"
        "Programming:\n"
        "- No dynamic linking (static linking only)\n"
        "- No shared libraries\n"
        "- No debugger or profiler\n"
        "- Limited C library (no full libc)\n"
        "- Python is MicroPython (limited stdlib)\n"
        "\n"
        "Hardware:\n"
        "- QEMU virt and Pi Zero 2W only\n"
        "- No SMP (single-core only, even on quad-core Pi)\n"
        "- No USB on QEMU (virtio devices instead)\n"
        "- No GPU acceleration\n"
        "\n"
        "Performance:\n"
        "- Interpreted Python is slow\n"
        "- FAT32 has no caching (slow I/O)\n"
        "- Network stack is basic (slow throughput)\n"
        "- Framebuffer is software-rendered\n"
        "\n"
        "Stability:\n"
        "- Buggy programs can crash the kernel\n"
        "- No kernel panic recovery (hard reset required)\n"
        "- No filesystem journaling (corruption possible on crash)\n"
        "- Memory leaks in long-running processes\n"
        "\n"
        "These are not bugs - they're design choices for a simple,\n"
        "educational hobby OS. VibeOS is not trying to be Linux.\n"
    },
};

static const int section_count = sizeof(sections) / sizeof(sections[0]);
static int selected_section = 0;
static int content_scroll = 0;

// Scrollbar dragging state
static int dragging_scrollbar = 0;
static int drag_start_y = 0;
static int drag_start_scroll = 0;

// Dirty flags
static int dirty_all = 1;

// ============ Drawing Helpers ============

#define fill_rect(x, y, w, h, c)     gfx_fill_rect(&gfx, x, y, w, h, c)
#define draw_char(x, y, ch, fg, bg)  gfx_draw_char(&gfx, x, y, ch, fg, bg)
#define draw_string(x, y, s, fg, bg) gfx_draw_string(&gfx, x, y, s, fg, bg)
#define draw_rect(x, y, w, h, c)     gfx_draw_rect(&gfx, x, y, w, h, c)
#define draw_hline(x, y, w, c)       gfx_draw_hline(&gfx, x, y, w, c)
#define draw_vline(x, y, h, c)       gfx_draw_vline(&gfx, x, y, h, c)

// Draw text clipped to width
static void draw_text_clip(int x, int y, const char *s, uint32_t fg, uint32_t bg, int max_w) {
    int drawn = 0;
    while (*s && drawn + 8 <= max_w) {
        draw_char(x, y, *s, fg, bg);
        x += 8;
        drawn += 8;
        s++;
    }
}

// ============ UI Drawing ============

static void draw_sidebar(void) {
    // Sidebar background
    fill_rect(0, 0, SIDEBAR_W, win_h, WHITE);
    draw_vline(SIDEBAR_W - 1, 0, win_h, BLACK);

    // Title
    draw_string(PADDING, 8, "Help Topics", BLACK, WHITE);
    draw_hline(PADDING, 26, SIDEBAR_W - 2 * PADDING, BLACK);

    // Section list
    int y = 32;
    for (int i = 0; i < section_count; i++) {
        int item_y = y + i * SECTION_ITEM_H;

        // Highlight selected section
        if (i == selected_section) {
            fill_rect(4, item_y, SIDEBAR_W - 8, SECTION_ITEM_H - 2, BLACK);
            draw_text_clip(8, item_y + 4, sections[i].title, WHITE, BLACK, SIDEBAR_W - 16);
        } else {
            draw_text_clip(8, item_y + 4, sections[i].title, BLACK, WHITE, SIDEBAR_W - 16);
        }
    }
}

// Wrapped line cache (regenerated when section or width changes)
static char wrapped_lines[500][100];  // Max 500 wrapped lines, 100 chars each
static int wrapped_line_count = 0;
static int last_section = -1;
static int last_content_w = 0;

// Generate wrapped lines from current section content
static void generate_wrapped_lines(int max_chars_per_line) {
    wrapped_line_count = 0;
    const char *text = sections[selected_section].content;

    char line_buf[256];
    const char *p = text;

    while (*p) {
        // Read one source line
        int i = 0;
        while (*p && *p != '\n' && i < 255) {
            line_buf[i++] = *p++;
        }
        line_buf[i] = 0;
        if (*p == '\n') p++;

        // Wrap this line if needed
        if (i == 0) {
            // Empty line
            wrapped_lines[wrapped_line_count][0] = 0;
            wrapped_line_count++;
        } else if (i <= max_chars_per_line) {
            // Fits on one line
            for (int j = 0; j < i && j < 99; j++) {
                wrapped_lines[wrapped_line_count][j] = line_buf[j];
            }
            wrapped_lines[wrapped_line_count][i] = 0;
            wrapped_line_count++;
        } else {
            // Need to wrap
            int start = 0;
            while (start < i && wrapped_line_count < 500) {
                int end = start + max_chars_per_line;
                if (end > i) end = i;

                // Try to break at a space
                if (end < i) {
                    int break_pos = end;
                    while (break_pos > start && line_buf[break_pos] != ' ') {
                        break_pos--;
                    }
                    if (break_pos > start) {
                        end = break_pos;
                    }
                }

                // Copy wrapped line
                int len = end - start;
                for (int j = 0; j < len && j < 99; j++) {
                    wrapped_lines[wrapped_line_count][j] = line_buf[start + j];
                }
                wrapped_lines[wrapped_line_count][len] = 0;
                wrapped_line_count++;

                // Skip spaces at start of next line
                start = end;
                while (start < i && line_buf[start] == ' ') start++;
            }
        }

        if (wrapped_line_count >= 500) break;
    }
}

static void draw_content(void) {
    int content_x = SIDEBAR_W + PADDING;
    int content_y = PADDING;
    int content_w = win_w - SIDEBAR_W - 2 * PADDING - 20;  // Reserve space for scrollbar
    int content_h = win_h - 2 * PADDING;

    // Regenerate wrapped lines if section or width changed
    int max_chars = content_w / 8;  // 8 pixels per character
    if (selected_section != last_section || content_w != last_content_w) {
        generate_wrapped_lines(max_chars);
        last_section = selected_section;
        last_content_w = content_w;
    }

    // Background
    fill_rect(SIDEBAR_W, 0, win_w - SIDEBAR_W, win_h, WHITE);

    // Title
    draw_string(content_x, content_y, sections[selected_section].title, BLACK, WHITE);
    draw_hline(content_x, content_y + 18, content_w + 20, BLACK);

    // Content area
    int text_y = content_y + 26;
    int max_visible_lines = (content_h - 30) / LINE_HEIGHT;
    int total_lines = wrapped_line_count;

    // Clamp scroll
    if (content_scroll > total_lines - max_visible_lines) {
        content_scroll = total_lines - max_visible_lines;
    }
    if (content_scroll < 0) {
        content_scroll = 0;
    }

    // Draw visible lines
    for (int i = 0; i < max_visible_lines && (content_scroll + i) < total_lines; i++) {
        draw_string(content_x, text_y + i * LINE_HEIGHT, wrapped_lines[content_scroll + i], BLACK, WHITE);
    }

    // Draw scrollbar if needed
    if (total_lines > max_visible_lines) {
        int scrollbar_x = win_w - 16;
        int scrollbar_h = win_h - 2 * PADDING;
        int thumb_h = (max_visible_lines * scrollbar_h) / total_lines;
        if (thumb_h < 20) thumb_h = 20;
        int thumb_y = PADDING + (content_scroll * (scrollbar_h - thumb_h)) / (total_lines - max_visible_lines);

        // Track
        draw_rect(scrollbar_x, PADDING, 12, scrollbar_h, BLACK);
        // Thumb
        fill_rect(scrollbar_x + 1, thumb_y, 10, thumb_h, GRAY);
    }
}

static void redraw_all(void) {
    draw_sidebar();
    draw_content();
    api->window_invalidate(window_id);
    dirty_all = 0;
}

// ============ Event Handling ============

// Get scrollbar bounds (returns 0 if no scrollbar)
static int get_scrollbar_bounds(int *x, int *y, int *h, int *thumb_y, int *thumb_h) {
    int content_h = win_h - 2 * PADDING;
    int max_visible_lines = (content_h - 30) / LINE_HEIGHT;
    int total_lines = wrapped_line_count;

    if (total_lines <= max_visible_lines) {
        return 0;  // No scrollbar needed
    }

    int scrollbar_x = win_w - 16;
    int scrollbar_y = PADDING;
    int scrollbar_h = win_h - 2 * PADDING;
    int th = (max_visible_lines * scrollbar_h) / total_lines;
    if (th < 20) th = 20;
    int ty = PADDING + (content_scroll * (scrollbar_h - th)) / (total_lines - max_visible_lines);

    if (x) *x = scrollbar_x;
    if (y) *y = scrollbar_y;
    if (h) *h = scrollbar_h;
    if (thumb_y) *thumb_y = ty;
    if (thumb_h) *thumb_h = th;

    return 1;
}

static void handle_click(int mx, int my) {
    // Check scrollbar click
    int sb_x, sb_y, sb_h, thumb_y, thumb_h;
    if (get_scrollbar_bounds(&sb_x, &sb_y, &sb_h, &thumb_y, &thumb_h)) {
        // Check if clicked on scrollbar area
        if (mx >= sb_x && mx < sb_x + 12) {
            // Clicked on thumb?
            if (my >= thumb_y && my < thumb_y + thumb_h) {
                dragging_scrollbar = 1;
                drag_start_y = my;
                drag_start_scroll = content_scroll;
                return;
            }
            // Clicked on track - jump to that position
            else {
                int content_h = win_h - 2 * PADDING;
                int max_visible_lines = (content_h - 30) / LINE_HEIGHT;
                int total_lines = wrapped_line_count;

                // Calculate target scroll position
                int relative_y = my - sb_y;
                content_scroll = (relative_y * total_lines) / sb_h;

                // Clamp
                if (content_scroll < 0) content_scroll = 0;
                if (content_scroll > total_lines - max_visible_lines) {
                    content_scroll = total_lines - max_visible_lines;
                }

                dirty_all = 1;
                return;
            }
        }
    }

    // Check sidebar clicks
    if (mx < SIDEBAR_W) {
        int clicked_section = (my - 32) / SECTION_ITEM_H;
        if (clicked_section >= 0 && clicked_section < section_count) {
            selected_section = clicked_section;
            content_scroll = 0;  // Reset scroll when changing section
            dirty_all = 1;
        }
    }
}

static void handle_mouse_move(int mx, int my) {
    if (dragging_scrollbar) {
        int delta_y = my - drag_start_y;

        int sb_x, sb_y, sb_h, thumb_y, thumb_h;
        if (get_scrollbar_bounds(&sb_x, &sb_y, &sb_h, &thumb_y, &thumb_h)) {
            int content_h = win_h - 2 * PADDING;
            int max_visible_lines = (content_h - 30) / LINE_HEIGHT;
            int total_lines = wrapped_line_count;

            // Convert pixel delta to scroll delta
            int scroll_range = total_lines - max_visible_lines;
            int pixel_range = sb_h - thumb_h;

            if (pixel_range > 0) {
                int new_scroll = drag_start_scroll + (delta_y * scroll_range) / pixel_range;

                // Clamp
                if (new_scroll < 0) new_scroll = 0;
                if (new_scroll > scroll_range) new_scroll = scroll_range;

                if (new_scroll != content_scroll) {
                    content_scroll = new_scroll;
                    dirty_all = 1;
                }
            }
        }
    }
}

static void handle_mouse_up(int mx, int my) {
    dragging_scrollbar = 0;
}

static void handle_scroll(int delta) {
    content_scroll -= delta * 3;  // Scroll 3 lines at a time

    // Clamping is done in draw_content()
    if (content_scroll < 0) {
        content_scroll = 0;
    }

    dirty_all = 1;
}

static void handle_key(int key) {
    int content_h = win_h - 2 * PADDING;
    int max_visible_lines = (content_h - 30) / LINE_HEIGHT;
    int total_lines = wrapped_line_count;

    if (key == 0x101) {  // KEY_DOWN
        content_scroll++;
        if (content_scroll > total_lines - max_visible_lines) {
            content_scroll = total_lines - max_visible_lines;
        }
        if (content_scroll < 0) content_scroll = 0;
        dirty_all = 1;
    } else if (key == 0x100) {  // KEY_UP
        content_scroll--;
        if (content_scroll < 0) content_scroll = 0;
        dirty_all = 1;
    } else if (key == 0x19) {  // Page Down (Ctrl+Y or similar)
        content_scroll += max_visible_lines - 1;
        if (content_scroll > total_lines - max_visible_lines) {
            content_scroll = total_lines - max_visible_lines;
        }
        if (content_scroll < 0) content_scroll = 0;
        dirty_all = 1;
    } else if (key == 0x18) {  // Page Up (Ctrl+X or similar)
        content_scroll -= max_visible_lines - 1;
        if (content_scroll < 0) content_scroll = 0;
        dirty_all = 1;
    }
}

// ============ Main ============

int main(kapi_t *k, int argc, char **argv) {
    api = k;

    // Create window
    window_id = api->window_create(60, 40, 700, 500, "VibeOS Help");
    if (window_id < 0) {
        api->puts("Failed to create window\n");
        return 1;
    }

    // Get window buffer
    win_buffer = api->window_get_buffer(window_id, &win_w, &win_h);
    if (!win_buffer) {
        api->window_destroy(window_id);
        return 1;
    }

    // Initialize graphics context
    gfx_init(&gfx, win_buffer, win_w, win_h, api->font_data);

    // Initial draw
    redraw_all();

    // Event loop
    int running = 1;
    while (running) {
        int event_type, data1, data2, data3;
        while (api->window_poll_event(window_id, &event_type, &data1, &data2, &data3)) {
            if (event_type == 5) {  // WIN_EVENT_CLOSE
                running = 0;
            } else if (event_type == 1) {  // WIN_EVENT_MOUSE_DOWN
                handle_click(data1, data2);
            } else if (event_type == 2) {  // WIN_EVENT_MOUSE_UP
                handle_mouse_up(data1, data2);
            } else if (event_type == 3) {  // WIN_EVENT_MOUSE_MOVE
                handle_mouse_move(data1, data2);
            } else if (event_type == 4) {  // WIN_EVENT_KEY_DOWN
                handle_key(data3);  // data3 is the key code
            } else if (event_type == 6) {  // WIN_EVENT_MOUSE_WHEEL
                handle_scroll(data2);  // data2 is delta for wheel events
            } else if (event_type == 8) {  // WIN_EVENT_RESIZE
                // Re-fetch buffer with new dimensions
                win_buffer = api->window_get_buffer(window_id, &win_w, &win_h);
                gfx_init(&gfx, win_buffer, win_w, win_h, api->font_data);
                dirty_all = 1;
            }
        }

        if (dirty_all) {
            redraw_all();
        }

        api->yield();
    }

    api->window_destroy(window_id);
    return 0;
}
