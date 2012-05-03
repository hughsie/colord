#!/usr/bin/python

# Copyright (C) 2011-2012 Richard Hughes <richard@hughsie.com>
# Licensed under the GNU General Public License Version 2

from gi.repository import Colord
from gi.repository import Gio

# connect to colord
cancellable = Gio.Cancellable.new();
client = Colord.Client.new()
client.connect_sync(cancellable)

# find the device with the correct parameters
device = client.find_device_by_property_sync("XRANDR_name", "LVDS1", cancellable)
device.connect_sync(cancellable)

# get the default profile for the device
profile = device.get_default_profile()
profile.connect_sync(cancellable)

print "default profile filename", profile.get_filename()
