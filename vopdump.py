#!/usr/bin/env python3
"""Read-only dump of the RK3399 VOP line timing and window-0 position, plus the
DSI packet size, to see where the image sits within each transmitted line.
Only reads a VOP whose runtime PM status is "active" (reading a powered-down
VOP can hang the SoC). Usage: sudo ./vopdump.py
"""
import mmap, os, struct

def regs(base, offs):
    fd = os.open("/dev/mem", os.O_RDONLY | os.O_SYNC)
    m = mmap.mmap(fd, 0x1000, mmap.MAP_SHARED, mmap.PROT_READ, offset=base)
    out = {n: struct.unpack_from("<I", m, o)[0] for n, o in offs.items()}
    m.close(); os.close(fd)
    return out

def hilo(v):
    return v >> 16, v & 0xffff

VOP = {"DSP_HTOTAL_HS_END": 0x188, "DSP_HACT_ST_END": 0x18c, "POST_DSP_HACT_INFO": 0x170,
       "POST_SCL_CTRL": 0x180, "WIN0_CTRL0": 0x30, "WIN0_ACT_INFO": 0x48,
       "WIN0_DSP_INFO": 0x4c, "WIN0_DSP_ST": 0x50, "DSP_BG": 0x18}
for name, base in (("vopl", 0xff8f0000), ("vopb", 0xff900000)):
    try:
        st = open(f"/sys/devices/platform/{base:x}.vop/power/runtime_status").read().strip()
    except OSError:
        st = "unknown"
    if st != "active":
        print(f"{name} {base:#x}: runtime_status {st}, not reading")
        continue
    r = regs(base, VOP)
    ht, hse = hilo(r["DSP_HTOTAL_HS_END"]); hst, hen = hilo(r["DSP_HACT_ST_END"])
    pst, pen = hilo(r["POST_DSP_HACT_INFO"]); wy, wx = hilo(r["WIN0_DSP_ST"])
    print(f"{name}: htotal {ht} hs_end {hse} | hact {hst}..{hen} ({hen - hst} px) | "
          f"post {pst}..{pen} | win0 en {r['WIN0_CTRL0'] & 1} dsp_st x {wx} y {wy} "
          f"dsp_info {r['WIN0_DSP_INFO']:#x} act_info {r['WIN0_ACT_INFO']:#x} | "
          f"post_scl {r['POST_SCL_CTRL']:#x} bg {r['DSP_BG']:#x}")
d = regs(0xff960000, {"VID_PKT_SIZE": 0x3c, "HSA": 0x48, "HBP": 0x4c, "HLINE": 0x50})
print("dsi:", d)
