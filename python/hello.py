# Hello World for VibeOS
import vibe

vibe.set_color(vibe.GREEN, vibe.BLACK)
vibe.puts("Hello from Python!\n")
vibe.set_color(vibe.WHITE, vibe.BLACK)
vibe.puts("Press any key to exit...\n")

while not vibe.has_key():
    vibe.yield()

vibe.getc()
