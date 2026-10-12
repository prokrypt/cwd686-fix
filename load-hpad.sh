#!/bin/sh
# Test the dsi_hpad fix: boot with module_blacklist=panel_cwd686,rockchipdrm,
# then load the patched display driver and the 480-wide test panel driver.
# Usage: sudo ./load-hpad.sh HPAD [panel params...]   e.g. sudo ./load-hpad.sh 5
# Modules can't be unloaded safely, so reboot to change HPAD.
set -e
D=$(dirname "$(readlink -f "$0")")
HPAD=${1:?usage: load-hpad.sh HPAD [panel params...]}; shift
grep -q 'module_blacklist=[^ ]*rockchipdrm' /proc/cmdline || { echo "rockchipdrm not blacklisted; not loading"; exit 1; }
lsmod | grep -q '^rockchipdrm_test ' && { echo "already loaded; reboot to change"; exit 1; }
# dependencies of rockchipdrm (modinfo -F depends rockchipdrm)
modprobe -a drm drm_kms_helper drm_display_helper drm_dma_helper dw_mipi_dsi dw_hdmi analogix_dp
insmod "$D/rockchipdrm-test.ko" dsi_hpad="$HPAD"
insmod "$D/panel-cwd686-test.ko" "$@"
sleep 3
dmesg | grep -iE "cwd686|dsi_hpad|rockchip-drm" | tail -4
cat /sys/class/drm/card*-DSI-1/modes
systemctl reset-failed getty@tty1 2>/dev/null || true
systemctl restart getty@tty1
