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

dtp94
-----
This module installs the `libcolorhug.so` shared library and used to interface
with the DTP94 colorimeter sensor.

This library is used by the daemon only and is currently unsuitable for other
programs to use. This library is is unversioned and **does not** have the same
ABI and API guarantees of libcolord.

huey
----
This module installs the `libcolorhug.so` shared library and used to interface
with the Huey colorimeter sensor.

This library is used by the daemon only and is currently unsuitable for other
programs to use. This library is is unversioned and **does not** have the same
ABI and API guarantees of libcolord.

munki
-----
This module installs the `libcolorhug.so` shared library and used to interface
with the ColorMunki photospectrometer sensor.

This library is used by the daemon only and is currently unsuitable for othe
programs to use. This library is is unversioned and **does not** have the same
ABI and API guarantees of libcolord.

The library is incomplete and requires a lot more work before it it useful.
