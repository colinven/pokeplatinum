#!/bin/bash
# Run meson inside the build container. Uses the meson version the Makefile installs.
MESON_VER=$(sed -n 's/^MESON_VER := //p' Makefile)
./drun.sh ./subprojects/meson-$MESON_VER/meson.py "$@"
