#!/usr/bin/env python3
"""Write a landscape edge-ruler image (PPM) for measuring panel clipping.
Usage: ruler.py WIDTH HEIGHT OUT.ppm [BG_HEX]   (WIDTH x HEIGHT = sway output size;
BG_HEX e.g. 80 for a grey background, so rows the panel blanks show up black)

Rows 0..15 at the top, and rows H-1..H-16 at the bottom, are bars starting at
the left edge. Bar i is (i+1)*64 px long, coloured red/green/blue/yellow in
turn. Dashed white vertical ticks every 64 px. If the topmost visible bar
reaches tick N, then N-1 rows are clipped at that edge.
"""
import sys

w, h, out = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3]
STEP, N = 64, 16
COLORS = [b"\xff\x00\x00", b"\x00\xff\x00", b"\x00\x00\xff", b"\xff\xff\x00"]
WHITE, BLACK = b"\xff\xff\xff", b"\x00\x00\x00"
BG = bytes([int(sys.argv[4], 16)] * 3) if len(sys.argv) > 4 else BLACK

img = bytearray(BG * w * h)

def put(x, y, px):
    o = (y * w + x) * 3
    img[o:o + 3] = px

for i in range(N):
    for y in (i, h - 1 - i):
        for x in range(min(w, (i + 1) * STEP)):
            put(x, y, COLORS[i % 4])
for x in range(STEP, w, STEP):
    for y in range(N + 4, h - N - 4):
        if (y // 6) % 2 == 0:
            put(x, y, WHITE)
# Solid white block near the top only, so a vertically flipped image is
# recognisable: the block then shows up near the bottom edge.
for y in range(40, 90):
    for x in range(300, 500):
        put(x, y, WHITE)
for x in range(w):
    put(x, N + 2, WHITE)
    put(x, h - 1 - N - 2, WHITE)

with open(out, "wb") as f:
    f.write(b"P6\n%d %d\n255\n" % (w, h) + bytes(img))
