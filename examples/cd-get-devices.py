#!/usr/bin/python

# Copyright (C) 2013 Richard Hughes <richard@hughsie.com>
# Licensed under the GNU General Public License Version 2

from gi.repository import Colord
from gi.repository import Gio

# connect to colord
cancellable = Gio.Cancellable.new();
client = Colord.Client.new()
client.connect_sync(cancellable)

# find the device with the correct parameters
devices = client.get_devices_sync(cancellable)

for d in devices:

    # get the default profile for the device
    d.connect_sync(cancellable)
    p = d.get_default_profile()
    if not p:
        continue
    p.connect_sync(cancellable)
    print d.get_id(), "has default profile filename", p.get_filename()
    metadata = p.get_metadata()
    if metadata.has_key('SCREEN_brightness'):
        print "and device sets brightness", metadata['SCREEN_brightness']
