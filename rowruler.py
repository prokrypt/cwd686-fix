#!/usr/bin/env python3
"""Write a landscape test image (PPM) with numbered rows, for reading roll and
offset from a photo of the panel.
Usage: rowruler.py WIDTH HEIGHT OUT.ppm

- Every 16th row is a white line across the left 200 px, with its row number
  in a 3x5 pixel font scaled 2x. Rows 0, 1, 2, 3 and H-1..H-4 are coloured
  full-width lines (red, green, blue, yellow) so the exact edges are visible.
- A white block near the top (rows 40-89, x 300-499) shows whether the image
  is flipped vertically.
"""
import sys

w, h, out = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3]
GREY, WHITE = b"\x80\x80\x80", b"\xff\xff\xff"
EDGE = [b"\xff\x00\x00", b"\x00\xff\x00", b"\x00\x00\xff", b"\xff\xff\x00"]
FONT = {  # 3x5 digits, one string per row
    "0": ["111", "101", "101", "101", "111"], "1": ["010", "110", "010", "010", "111"],
    "2": ["111", "001", "111", "100", "111"], "3": ["111", "001", "111", "001", "111"],
    "4": ["101", "101", "111", "001", "001"], "5": ["111", "100", "111", "001", "111"],
    "6": ["111", "100", "111", "101", "111"], "7": ["111", "001", "001", "001", "001"],
    "8": ["111", "101", "111", "101", "111"], "9": ["111", "101", "111", "001", "111"],
}

img = bytearray(GREY * w * h)

def put(x, y, px):
    if 0 <= x < w and 0 <= y < h:
        o = (y * w + x) * 3
        img[o:o + 3] = px

def text(x, y, s, px, scale=2):
    for ch in s:
        for r, bits in enumerate(FONT[ch]):
            for c, b in enumerate(bits):
                if b == "1":
                    for dy in range(scale):
                        for dx in range(scale):
                            put(x + c * scale + dx, y + r * scale + dy, px)
        x += 4 * scale

for y in range(0, h, 16):
    for x in range(200):
        put(x, y, WHITE)
    text(210, y + 2 if y + 12 < h else y - 11, str(y), WHITE)

for i in range(4):
    for x in range(w):
        put(x, i, EDGE[i])
        put(x, h - 1 - i, EDGE[i])

for y in range(40, 90):
    for x in range(300, 500):
        put(x, y, WHITE)

with open(out, "wb") as f:
    f.write(b"P6\n%d %d\n255\n" % (w, h) + bytes(img))
