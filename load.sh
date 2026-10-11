#!/bin/sh
# After booting with module_blacklist=panel_cwd686 on the kernel command line:
# load the test panel driver with register/timing params, then bring sway back.
# Usage: sudo ./load.sh [params...]   e.g. sudo ./load.sh c1=0,0x0C,0x16,0x04,0,0x28,0x96,0x18
# The module can't be unloaded safely (rockchipdrm oopses), so reboot to change params.
set -e
D=$(dirname "$(readlink -f "$0")")
grep -q module_blacklist=panel_cwd686 /proc/cmdline || { echo "stock driver not blacklisted; not loading"; exit 1; }
lsmod | grep -q '^panel_cwd686_test ' && { echo "already loaded; reboot to change params"; exit 1; }
insmod "$D/panel-cwd686-test.ko" "$@"
sleep 3
dmesg | grep -i cwd686 | tail -3
cat /sys/class/drm/card*-DSI-1/modes
systemctl reset-failed getty@tty1 2>/dev/null || true
systemctl restart getty@tty1
