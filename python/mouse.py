# Mouse Demo for VibeOS
import vibe

w, h = vibe.screen_size()
vibe.fill_rect(0, 0, w, h, vibe.BLACK)
vibe.draw_string(10, 10, "Mouse Demo - Click to draw, press Q to quit", vibe.WHITE, vibe.BLACK)

colors = [vibe.RED, vibe.GREEN, vibe.BLUE, vibe.YELLOW, vibe.CYAN, vibe.MAGENTA, vibe.WHITE]
color_idx = 0

while True:
    # Check for quit
    if vibe.has_key():
        c = vibe.getc()
        if c == 113 or c == 81:  # q or Q
            break
        # Change color on any other key
        color_idx = (color_idx + 1) % len(colors)

    # Get mouse state
    mx, my = vibe.mouse_pos()
    buttons = vibe.mouse_buttons()

    # Draw if button pressed
    if buttons & 1:  # Left button
        vibe.fill_rect(mx - 5, my - 5, 10, 10, colors[color_idx])

    vibe.yield()

vibe.clear()
