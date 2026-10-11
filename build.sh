#!/bin/sh
# Build panel-cwd686.ko in tmpfs so nothing lands on the microSD root.
set -e
B=/tmp/claude/cwd686-build
mkdir -p "$B"
cp "$(dirname "$0")"/panel-cwd686.c "$(dirname "$0")"/Makefile "$B"/
make -s -C /usr/lib/modules/$(uname -r)/build M="$B" modules
echo "$B/panel-cwd686-test.ko"
