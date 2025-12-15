# Graphics Demo for VibeOS
import vibe

w, h = vibe.screen_size()

# Clear screen to blue
vibe.fill_rect(0, 0, w, h, vibe.BLUE)

# Draw some rectangles
vibe.fill_rect(50, 50, 200, 150, vibe.RED)
vibe.fill_rect(100, 100, 200, 150, vibe.GREEN)
vibe.fill_rect(150, 150, 200, 150, vibe.YELLOW)

# Draw title
vibe.draw_string(250, 20, "VibeOS Python Graphics!", vibe.WHITE, vibe.BLUE)

# Draw info
vibe.draw_string(400, 400, "Screen: " + str(w) + "x" + str(h), vibe.WHITE, vibe.BLUE)
vibe.draw_string(400, 420, "Press any key to exit", vibe.CYAN, vibe.BLUE)

# Wait for key
while not vibe.has_key():
    vibe.yield()

vibe.getc()
vibe.clear()
