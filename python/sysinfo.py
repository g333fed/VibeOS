# System Info for VibeOS
import vibe

vibe.clear()
vibe.set_color(vibe.CYAN, vibe.BLACK)
vibe.puts("=== VibeOS System Info ===\n\n")

vibe.set_color(vibe.WHITE, vibe.BLACK)

# Screen info
w, h = vibe.screen_size()
vibe.puts("Screen: " + str(w) + "x" + str(h) + " pixels\n")

# Memory info
free = vibe.mem_free()
used = vibe.mem_used()
total = free + used
vibe.puts("Memory: " + str(used // 1024) + " KB used, " + str(free // 1024) + " KB free\n")

# Uptime
ms = vibe.uptime_ms()
secs = ms // 1000
mins = secs // 60
hrs = mins // 60
vibe.puts("Uptime: " + str(hrs) + "h " + str(mins % 60) + "m " + str(secs % 60) + "s\n")

vibe.puts("\n")
vibe.set_color(vibe.GREEN, vibe.BLACK)
vibe.puts("Python is running on bare metal!\n")

vibe.set_color(vibe.WHITE, vibe.BLACK)
vibe.puts("\nPress any key to exit...\n")

while not vibe.has_key():
    vibe.yield()
vibe.getc()
