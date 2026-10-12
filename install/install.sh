#!/bin/sh
# Install the cwd686 fix for the running kernel: patched rockchipdrm (dsi_hpad)
# and a 480-wide panel_cwd686 into updates/ (depmod prefers it over kernel/),
# the modprobe option, then rebuild the initramfs. Run with sudo.
set -e
D=$(dirname "$(readlink -f "$0")")
U=/usr/lib/modules/$(uname -r)/updates
install -d "$U"
install -m644 "$D/rockchipdrm.ko" "$D/panel-cwd686.ko" "$U/"
install -m644 "$D/cwd686.conf" /etc/modprobe.d/cwd686.conf
depmod -a "$(uname -r)"
modinfo -n rockchipdrm panel_cwd686
mkinitcpio -P
