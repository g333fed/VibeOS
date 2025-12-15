# Bouncing Ball Demo for VibeOS
import vibe

w, h = vibe.screen_size()

# Ball state
x = w // 2
y = h // 2
dx = 5
dy = 3
size = 20

while True:
    # Check for quit
    if vibe.has_key():
        vibe.getc()
        break

    # Clear old ball
    vibe.fill_rect(x, y, size, size, vibe.BLACK)

    # Move ball
    x = x + dx
    y = y + dy

    # Bounce off walls
    if x <= 0 or x >= w - size:
        dx = -dx
    if y <= 0 or y >= h - size:
        dy = -dy

    # Keep in bounds
    if x < 0:
        x = 0
    if x > w - size:
        x = w - size
    if y < 0:
        y = 0
    if y > h - size:
        y = h - size

    # Draw ball
    vibe.fill_rect(x, y, size, size, vibe.RED)

    # Draw instructions
    vibe.draw_string(10, 10, "Bouncing Ball - Press any key to quit", vibe.WHITE, vibe.BLACK)

    vibe.sleep_ms(16)  # ~60fps

vibe.clear()
