#!/bin/sh
if [ -z $MESON_INSTALL_PREFIX ]; then
    echo 'This is meant to be ran from Meson only!'
    exit 1
fi

localstatedir=$1
daemon_user=$2

echo 'Creating stateful directory'
mkdir -p ${DESTDIR}${localstatedir}/lib/colord/icc
ls -l ${DESTDIR}${localstatedir}/lib
if [ `id -u` = 0 ] ; then
	chown ${daemon_user} ${DESTDIR}${localstatedir}/lib/colord
	chmod 0755 ${DESTDIR}${localstatedir}/lib/colord
fi
