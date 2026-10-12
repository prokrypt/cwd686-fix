# cwd686-fix

A fix for the **top 5 rows being cut off** on the ClockworkPi DevTerm's
built-in screen (cwd686 panel, Chipone ICNL9707 controller), tested on the
DevTerm A06 (RK3399) with the `linux-clockworkpi-a06` 6.2.7 kernel.

With the fix, all 480 rows of the 1280x480 landscape picture are visible, and
userspace still sees a plain 1280x480 output (no 1280x485 hacks, no xrandr
transforms).

## The problem

The panel is natively 480x1280 portrait and is rotated 90° by the compositor.
Every DevTerm build loses the top ~5 rows of the landscape picture: A04, A06,
and the Pi/CM modules alike
([forum thread](https://forum.clockworkpi.com/t/a04-screen-display-vertical-offset/7829)).

Existing workarounds:

- yatli's `linux-clockworkpi-a06` sets the panel to `hdisplay = 485`. The
  picture fills the screen, but the compositor now has a 1280x485 output whose
  top 5 rows are drawn and never shown.
- On X11: the 485 mode plus `xrandr --fb 1280x480 --transform 1,0,0,0,1,-5,0,0,1`.

## The cause

**The panel drops the last 5 pixels of every DSI video line it receives.**

The screen is rotated, so the end of each native line is the top of the
landscape picture. That's where the missing rows go. The panel shows only the
first 480 pixels of each line, and only if the line is long enough for the
5 lost pixels to come out of it. That's why 485 "works".

None of the panel's own registers change this (see below), so it happens at
the link level, either in the panel's receiver or somewhere between it and the
glass.

## The fix

`0001-drm-rockchip-dsi-add-dsi_hpad.patch` adds a `dsi_hpad` module parameter
to the Rockchip DSI driver (`rockchipdrm`). With `dsi_hpad=N`, each line sent
to the panel is N pixels longer than the mode userspace asked for:

- The extra pixels are taken from the front porch, so the line start, total
  line length and refresh rate don't change.
- The VOP still sizes the planes from the requested mode, so the N extra
  pixels at the end of each line are background (black). Those are exactly
  the pixels the panel throws away.
- Userspace keeps seeing the panel's real 480x1280 mode.

It goes with a panel driver set back to the panel's real 480 width
(`install/panel-cwd686.c`, which is yatli's driver with 485 → 480).

```
options rockchipdrm dsi_hpad=5
```

## Installing (DevTerm A06, linux-clockworkpi-a06 6.2.7-1)

The modules must be built for your exact kernel. To build:

1. Get the kernel's `drivers/gpu/drm/rockchip` sources, e.g. from a v6.2.7
   tree, and apply `0001-drm-rockchip-dsi-add-dsi_hpad.patch` (`patch -p1`
   from the kernel tree root).
2. Build out of tree against `/usr/lib/modules/$(uname -r)/build`. Copy the
   rockchip directory and add `obj-m += rockchipdrm.o` to its Makefile. This
   takes ~1.5 minutes on the A06.
3. Build `install/panel-cwd686.c` the same way, with `obj-m += panel-cwd686.o`.
4. Put both `.ko` files in `install/`, then run:

```
sudo install/install.sh     # modules -> /usr/lib/modules/<ver>/updates/,
                            # /etc/modprobe.d/cwd686.conf, depmod, mkinitcpio -P
sudo reboot
```

To undo: `sudo install/uninstall.sh`, then reboot.

A kernel package update installs a new module directory and drops the fix.
Rebuild the modules for the new kernel.

## The hunt (or: our sacrifices)

Every one of these took a reboot, and the DevTerm boots from a microSD card,
so every test cost a little piece of its soul.

- **Panel timing on the SoC side** (porches, sync widths, htotal): no change.
  One overly long line (htotal 900, 46 Hz) wedged the panel controller so
  badly that neither DSI off/on nor suspend/resume brought it back. It needed
  a full power-off.
- **Panel TCON register 0xC1**: every driver sends 6 bytes where the
  [datasheet](https://github.com/clockworkpi/DevTerm/raw/main/Schematics/ICNL9707_Datasheet.pdf)
  wants 8. We sent it correctly: no change.
- **0xC2 (resolution select), MADCTL, ClockworkPi's Allwinner D1 init table,
  burst vs non-burst**: no change.
  - MADCTL did flip the image and roll it by ~120 rows, proving the top glass
    rows *can* light up.
- **Keeping the DSI lanes in HS during blanking**: the screen went black at once.
- **Unloading the panel module live** oopses the kernel. The DSI has no
  hot-swap button.
- **The first `dsi_hpad`** padded the *start* of each line, because we assumed
  "start of line = top of screen". The result was the image cut off at the top
  and a fresh black strip at the bottom.
  - Version one also re-applied the padding on every atomic check until the
    machine froze.
- **The breakthrough**: set the VOP background colour register to **red**
  live, so the padding pixels became visible. The red stripe turned up at the
  bottom. On this panel, the start of each line is the *bottom* of the
  landscape picture. Moving the padding to the end of the line fixed it on
  the first try.
  - Result: red row 0 at the top, red row 479 at the bottom. Communism.

## Tools in this repo

| File | What it does |
|---|---|
| `0001-drm-rockchip-dsi-add-dsi_hpad.patch` | The fix, against Linux v6.2.7 `drivers/gpu/drm/rockchip/dw-mipi-dsi-rockchip.c` |
| `install/` | Final 480-wide panel driver source, modprobe option, install/uninstall scripts |
| `panel-cwd686.c`, `build.sh`, `Makefile` | Test panel driver with module parameters (timings, 0xC1/0xC2 bytes, MADCTL, burst, init table), built as `panel_cwd686_test` |
| `load.sh`, `load-hpad.sh` | Load the test modules after booting with `module_blacklist=` |
| `ruler.py`, `rowruler.py` | Test images: edge staircases / numbered rows with coloured edge lines |
| `vopdump.py` | Read-only dump of the RK3399 VOP line/window registers and the DSI packet size |
| `dsi-lp.py` | Toggle the DSI host's LP-during-blanking bits live (blanked our screen; kept for science) |
| `panel-cwd686.orig.c`, `yatli-0004.patch`, `clockworkpi-kernel-004-panel.patch` | Original drivers, for reference |

## Credits

- yatli for `linux-clockworkpi-a06` and the original 485 investigation.
- ClockworkPi for the panel driver and the datasheet.
- The DevTerm forum for documenting that this hits every model.
- Debugged by prokrypt and Claude, one reboot at a time.
