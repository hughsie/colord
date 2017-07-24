Lbraries shipped with colord:
=============================

colord
------

This module installs the libcolord.so` and `libcolordprivate.so` shared libraries.
The former used by client programs, but the latter is designed
to be used by things that link, or may link into the colord daemon directly or
from a plugin.

This library is is versioned and has the ABI guarantees of the colord branch.

compat
------
This optional shim module installs the `libcolordcompat.so` shared library and
used by ArgyllCMS to store the device to profile mapping data.
This library is unversioned and does not install any headers. Clients are
expected to simply dlopen() the library and dlsym() the functions if required.

Long term, users of `libcolordcompat.so` probably ought to be switched to using
the real `libcolord.so` which has ABI and API stability.

colorhug
--------
This module contains the `libcolorhug.so` shared library and is used to
interface with the ColorHug colorimeter sensor.
This library is used by the daemon, and also ships a pkg-config file and
headers which makes it suitable for other programs to us.

This library is is versioned and has the same ABI guarantees of libcolord.
