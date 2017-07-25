#!/bin/sh
set -e
export LC_ALL=C.UTF-8
mkdir -p build && cd build
rm * -rf
meson .. \
    -Denable-vala=false \
    -Denable-print-profiles=false \
    -Denable-argyllcms-sensor=false \
    -Denable-sane=true \
    -Denable-libcolordcompat=true $@
ninja -v || bash
ninja test -v
DESTDIR=/tmp/install-ninja ninja install
cd ..
