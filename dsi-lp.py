#!/usr/bin/env python3
"""Toggle the RK3399 DSI host's "low power during horizontal blanking" bits live.

The dw-mipi-dsi driver always sets LP_HBP_EN and LP_HFP_EN (bits 12, 13 of
DSI_VID_MODE_CFG at 0xff960038), so the data lanes drop to LP mode in every
horizontal blanking period and must do a fresh start-of-transmission before
each line's pixel packet. Testing whether that is where the cwd686 panel loses
the first 5 pixels of every line.

Usage: sudo ./dsi-lp.py hs    # keep lanes in HS during HBP/HFP (clear bits)
       sudo ./dsi-lp.py lp    # restore the driver's setting (set bits)
       sudo ./dsi-lp.py       # just show the register
Changes last until the next modeset (sway mode change, output off/on, reboot).
"""
import mmap, os, struct, sys, time

REG = 0x38
BITS = (1 << 12) | (1 << 13)  # LP_HBP_EN, LP_HFP_EN

fd = os.open("/dev/mem", os.O_RDWR | os.O_SYNC)
m = mmap.mmap(fd, 0x1000, mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE, offset=0xff960000)
r = lambda: struct.unpack_from("<I", m, REG)[0]

before = r()
arg = sys.argv[1] if len(sys.argv) > 1 else ""
if arg == "hs":
    struct.pack_into("<I", m, REG, before & ~BITS)
elif arg == "lp":
    struct.pack_into("<I", m, REG, before | BITS)
time.sleep(0.1)
print(f"VID_MODE_CFG {before:#06x} -> {r():#06x}")
