#!/usr/bin/python

# Copyright(C) 2013 Richard Hughes <richard@hughsie.com>
# Licensed under the GNU General Public License Version 2

from __future__ import print_function

from gi.repository import Colord
from gi.repository import Gio

import os
import locale

locale.setlocale(locale.LC_ALL, '')

# parse file
icc = Colord.Icc.new()
f = Gio.file_new_for_path('/usr/share/color/icc/colord/sRGB.icc')
cancellable = Gio.Cancellable.new()
icc.load_file(f, Colord.IccLoadFlags.METADATA, cancellable)

# get details about the profile
print("Filename:", icc.get_filename())
print("License:", icc.get_metadata_item("License"))

# get translated UTF-8 strings where available
locale = os.getenv("LANG")
print("Description:", icc.get_description(locale))
print("Model:", icc.get_model(locale))
print("Copyright:", icc.get_copyright(locale))
