/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011-2012 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2012 Evan Nemerson <evan@coeus-group.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

//valac -o cd-vala-gtk-demo cd-vala-gtk-demo --pkg colord-gtk --pkg colord --pkg gtk+-3.0

private static int main (string[] args) {
	Gtk.init (ref args);

	var window = new Cd.Window ();
	var dialog = new Gtk.MessageDialog (null, Gtk.DialogFlags.MODAL, Gtk.MessageType.INFO, Gtk.ButtonsType.OK, "Hello world");
	dialog.map.connect ((dlg) => {
			window.get_profile.begin (dlg, null, (obj, async_res) => {
					try {
						Cd.Profile profile = window.get_profile.end (async_res);
						GLib.debug ("screen profile to use %s", profile.get_filename ());
					} catch ( GLib.Error e ) {
						GLib.warning ("failed to get output profile: %s", e.message);
					}
				});
		});
	dialog.run ();

	return 0;
}
