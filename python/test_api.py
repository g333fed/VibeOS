# Full vibe API test script
import vibe

def test_section(name):
    vibe.set_color(vibe.CYAN, vibe.BLACK)
    vibe.puts("\n=== " + name + " ===\n")
    vibe.set_color(vibe.WHITE, vibe.BLACK)

def ok(msg):
    vibe.set_color(vibe.GREEN, vibe.BLACK)
    vibe.puts("[OK] ")
    vibe.set_color(vibe.WHITE, vibe.BLACK)
    vibe.puts(msg + "\n")

def fail(msg):
    vibe.set_color(vibe.RED, vibe.BLACK)
    vibe.puts("[FAIL] ")
    vibe.set_color(vibe.WHITE, vibe.BLACK)
    vibe.puts(msg + "\n")

vibe.clear()
vibe.set_color(vibe.YELLOW, vibe.BLACK)
vibe.puts("VibeOS Python API Test Suite\n")
vibe.puts("============================\n")

# Console
test_section("Console I/O")
vibe.putc(65)  # 'A'
vibe.putc(10)  # newline
ok("putc")
vibe.puts("Hello from puts!\n")
ok("puts")
rows, cols = vibe.console_size()
ok("console_size: " + str(rows) + "x" + str(cols))

# Timing
test_section("Timing")
ms = vibe.uptime_ms()
ok("uptime_ms: " + str(ms) + "ms")
vibe.sleep_ms(10)
ok("sleep_ms(10)")

# RTC
test_section("RTC / DateTime")
ts = vibe.timestamp()
ok("timestamp: " + str(ts))
dt = vibe.datetime()
ok("datetime: " + str(dt[0]) + "-" + str(dt[1]) + "-" + str(dt[2]) + " " + str(dt[3]) + ":" + str(dt[4]) + ":" + str(dt[5]))

# Filesystem
test_section("Filesystem")
cwd = vibe.getcwd()
ok("getcwd: " + cwd)
entries = vibe.listdir("/")
ok("listdir(/): " + str(len(entries)) + " entries")
for name, is_dir in entries:
    vibe.puts("  " + ("D " if is_dir else "F ") + name + "\n")

# Test file read
f = vibe.open("/etc/motd")
if f:
    size = vibe.file_size(f)
    data = vibe.read(f, size)
    ok("open+read /etc/motd: " + str(len(data)) + " bytes")
else:
    fail("open /etc/motd")

# Test file create/write/delete
test_file = "/tmp/pytest.txt"
f = vibe.create(test_file)
if f:
    written = vibe.write(f, "Hello from Python!")
    ok("create+write: " + str(written) + " bytes")
    # Read it back
    f2 = vibe.open(test_file)
    if f2:
        data = vibe.read(f2, 100)
        ok("read back: " + str(data))
    # Delete it
    if vibe.delete(test_file):
        ok("delete")
    else:
        fail("delete")
else:
    fail("create")

# Process
test_section("Process")
procs = vibe.ps()
ok("ps: " + str(len(procs)) + " processes")
for pid, name, state in procs:
    vibe.puts("  [" + str(pid) + "] " + name + " (state=" + str(state) + ")\n")

# System Info
test_section("System Info")
ok("mem_used: " + str(vibe.mem_used() // 1024) + " KB")
ok("mem_free: " + str(vibe.mem_free() // 1024) + " KB")
ok("ram_total: " + str(vibe.ram_total() // 1024 // 1024) + " MB")
ok("disk_total: " + str(vibe.disk_total()) + " KB")
ok("disk_free: " + str(vibe.disk_free()) + " KB")
cpu = vibe.cpu_info()
ok("cpu_info: " + cpu[0] + " @ " + str(cpu[1]) + "MHz, " + str(cpu[2]) + " cores")

# USB
test_section("USB Devices")
usb = vibe.usb_devices()
ok("usb_devices: " + str(len(usb)) + " devices")
for vid, pid, name in usb:
    vibe.puts("  " + name + " (VID=" + str(vid) + " PID=" + str(pid) + ")\n")

# Graphics (quick test)
test_section("Graphics")
w, h = vibe.screen_size()
ok("screen_size: " + str(w) + "x" + str(h))

# Network
test_section("Networking")
ip = vibe.get_ip()
ok("get_ip: " + str((ip >> 24) & 0xFF) + "." + str((ip >> 16) & 0xFF) + "." + str((ip >> 8) & 0xFF) + "." + str(ip & 0xFF))

# Try DNS
dns_ip = vibe.dns_resolve("example.com")
if dns_ip:
    ok("dns_resolve(example.com): " + str((dns_ip >> 24) & 0xFF) + "." + str((dns_ip >> 16) & 0xFF) + "." + str((dns_ip >> 8) & 0xFF) + "." + str(dns_ip & 0xFF))
else:
    fail("dns_resolve failed")

# Sound (just check functions exist)
test_section("Sound")
ok("sound_is_playing: " + str(vibe.sound_is_playing()))

# LED (Pi only, no-op on QEMU)
test_section("LED (Pi only)")
vibe.led_toggle()
ok("led_toggle (no-op on QEMU)")

# Constants
test_section("Constants")
ok("Colors: BLACK=" + str(vibe.BLACK) + " WHITE=" + str(vibe.WHITE) + " RED=" + str(vibe.RED))
ok("Events: CLOSE=" + str(vibe.WIN_EVENT_CLOSE) + " KEY=" + str(vibe.WIN_EVENT_KEY))
ok("Mouse: LEFT=" + str(vibe.MOUSE_LEFT) + " RIGHT=" + str(vibe.MOUSE_RIGHT))

# Done
vibe.puts("\n")
vibe.set_color(vibe.GREEN, vibe.BLACK)
vibe.puts("=== ALL TESTS COMPLETE ===\n")
vibe.set_color(vibe.WHITE, vibe.BLACK)
vibe.puts("Press any key to exit...\n")

while not vibe.has_key():
    vibe.sched_yield()
vibe.getc()
