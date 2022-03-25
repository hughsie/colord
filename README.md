colord
======

colord is a system service that makes it easy to manage, install and generate
color profiles to accurately color manage input and output devices.

This functionality is implemented as a system activated daemon called colord.
Being system activated means that it's only started when the user is using a
text mode or graphical tool.

What colord does:

* Provides a DBus API for other programs to query, e.g.
  "Get me the profiles for device $foo" or
  "Create a device and assign it profile $bar"

* Provides a persistent database backed store for device -> profile mapping.

* Provides the session for a way to set system settings, for instance
  setting the display profile for all users and all sessions.

See [the website](https://www.freedesktop.org/software/colord/) for more details.

Compiling for Linux
===================

Ensure the following packages are installed:

* dbus-devel
* libxslt
* docbook5-style-xsl
* gettext
* glib2-devel
* gobject-introspection-devel
* gtk-doc
* lcms2-devel
* libgudev1-devel
* libgusb-devel
* polkit-devel
* sqlite-devel
* systemd-devel
* vala-tools

Then just do `mkdir build && cd build && meson --prefix=/usr .. && ninja`

Compiling for Microsoft Windows
===============================

* Ensure wine is installed

* Compile with a lot of restricted options, e.g.

    mingw64-configure                               \
            --disable-argyllcms-sensor              \
            --disable-bash-completion               \
            --disable-polkit                        \
            --disable-print-profiles                \
            --disable-sane                          \
            --disable-session-example               \
            --disable-systemd                 \
            --disable-udev                          \
            --disable-unix                          \
            --without-pic                           \
            --with-systemdsystemunitdir=/tmp        \
            --with-udevrulesdir=/tmp && make

Contributing
============

Patches are accepted in form of pull requests for repository
https://github.com/hughsie/colord
