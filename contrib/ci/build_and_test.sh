#!/bin/sh
set -e

./autogen.sh $@
make
make install DEST=/tmp/install_root/
