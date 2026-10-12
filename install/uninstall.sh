#!/bin/sh
# Undo install.sh: back to the stock (485-wide) modules. Run with sudo.
set -e
U=/usr/lib/modules/$(uname -r)/updates
rm -f "$U/rockchipdrm.ko" "$U/panel-cwd686.ko" /etc/modprobe.d/cwd686.conf
depmod -a "$(uname -r)"
mkinitcpio -P
