/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
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

/**
 * SECTION:cd-usb
 * @short_description: GLib mainloop integration for libusb
 *
 * This object can be used to integrate libusb into the GLib event loop.
 */

#include "config.h"

#include <glib-object.h>
#include <sys/poll.h>
#include <libusb-1.0/libusb.h>

#include "cd-usb.h"

static void     cd_usb_finalize	(GObject     *object);

#define CD_USB_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_USB, CdUsbPrivate))

typedef struct {
	GSource			 source;
	GSList			*pollfds;
} CdUsbSource;

/**
 * CdUsbPrivate:
 *
 * Private #CdUsb data
 **/
struct _CdUsbPrivate
{
	gboolean			 connected;
	CdUsbSource			*source;
	libusb_device_handle		*handle;
	libusb_context			*ctx;
};

enum {
	PROP_0,
	PROP_CONNECTED,
	PROP_LAST
};

G_DEFINE_TYPE (CdUsb, cd_usb, G_TYPE_OBJECT)

/**
 * cd_usb_get_connected:
 **/
gboolean
cd_usb_get_connected (CdUsb *usb)
{
	g_return_val_if_fail (CD_IS_USB (usb), FALSE);
	return usb->priv->connected;
}

/**
 * cd_libusb_pollfd_add:
 **/
static void
cd_libusb_pollfd_add (CdUsb *usb, int fd, short events)
{
	CdUsbSource *source = usb->priv->source;
	GPollFD *pollfd = g_slice_new (GPollFD);
	pollfd->fd = fd;
	pollfd->events = 0;
	pollfd->revents = 0;
	if (events & POLLIN)
		pollfd->events |= G_IO_IN;
	if (events & POLLOUT)
		pollfd->events |= G_IO_OUT;

	source->pollfds = g_slist_prepend (source->pollfds, pollfd);
	g_source_add_poll ((GSource *) source, pollfd);
}

/**
 * cd_libusb_pollfd_added_cb:
 **/
static void
cd_libusb_pollfd_added_cb (int fd, short events, void *user_data)
{
	CdUsb *usb = user_data;
	cd_libusb_pollfd_add (usb, fd, events);
}

/**
 * cd_libusb_pollfd_remove:
 **/
static void
cd_libusb_pollfd_remove (CdUsb *usb, int fd)
{
	CdUsbSource *source = usb->priv->source;
	GPollFD *pollfd;
	GSList *elem = source->pollfds;

	g_debug ("remove pollfd %i", fd);

	/* nothing to see here, move along */
	if (elem == NULL) {
		g_warning("cannot remove from list as list is empty?");
		return;
	}

	/* find the pollfd in the list */
	do {
		pollfd = elem->data;
		if (pollfd->fd != fd)
			continue;

		g_source_remove_poll ((GSource *) source, pollfd);
		g_slice_free (GPollFD, pollfd);
		source->pollfds = g_slist_delete_link (source->pollfds, elem);
		return;
	} while ((elem = g_slist_next(elem)));
	g_warning ("couldn't find fd %d in list", fd);
}

/**
 * cd_libusb_pollfd_remove_all:
 **/
static void
cd_libusb_pollfd_remove_all (CdUsb *usb)
{
	CdUsbSource *source = usb->priv->source;
	GPollFD *pollfd;
	GSList *elem;

	/* never connected */
	if (source == NULL) {
		g_debug ("never attached to a context");
		return;
	}

	/* nothing to see here, move along */
	elem = source->pollfds;
	if (elem == NULL)
		return;

	/* rip apart all the pollfd's */
	g_debug ("ripping");
	do {
		pollfd = elem->data;
		g_source_remove_poll ((GSource *) source, pollfd);
		g_slice_free (GPollFD, pollfd);
		source->pollfds = g_slist_delete_link (source->pollfds, elem);
	} while ((elem = g_slist_next(elem)));
}

/**
 * cd_libusb_pollfd_removed_cb:
 **/
static void
cd_libusb_pollfd_removed_cb (int fd, void *user_data)
{
	CdUsb *usb = user_data;
	cd_libusb_pollfd_remove (usb, fd);
}

/**
 * cd_usb_source_prepare:
 *
 * Called before all the file descriptors are polled.
 * As we are a file descriptor source, the prepare function returns FALSE.
 * It sets the returned timeout to -1 to indicate that it doesn't mind
 * how long the poll() call blocks.
 *
 * No, we're not going to support FreeBSD.
 **/
static gboolean
cd_usb_source_prepare (GSource *source, gint *timeout)
{
	*timeout = -1;
	return FALSE;
}

/**
 * cd_usb_source_check:
 *
 * In the check function, it tests the results of the poll() call to see
 * if the required condition has been met, and returns TRUE if so.
 **/
static gboolean
cd_usb_source_check (GSource *source)
{
	CdUsbSource *cd_source = (CdUsbSource *) source;
	GPollFD *pollfd;
	GSList *elem = cd_source->pollfds;

	/* no fds */
	if (elem == NULL)
		return FALSE;

	/* check each pollfd */
	do {
		pollfd = elem->data;
		if (pollfd->revents)
			return TRUE;
	} while ((elem = g_slist_next (elem)));

	return FALSE;
}

/**
 * cd_usb_source_dispatch:
 **/
static gboolean
cd_usb_source_dispatch (GSource *source,
			GSourceFunc callback,
			gpointer user_data)
{
	gint rc;
	struct timeval tv = { 0, 0 };
	CdUsb *usb = user_data;
	rc = libusb_handle_events_timeout (usb->priv->ctx, &tv);
	if (rc < 0) {
		g_warning ("failed to handle event: %s [%i]",
			   libusb_strerror (rc), rc);
	}
	return TRUE;
}

/**
 * cd_usb_source_finalize:
 *
 * Called when the source is finalized.
 **/
static void
cd_usb_source_finalize (GSource *source)
{
	GPollFD *pollfd;
	CdUsbSource *cd_source = (CdUsbSource *) source;
	GSList *elem = cd_source->pollfds;

	if (elem != NULL) {
		do {
			pollfd = elem->data;
			g_source_remove_poll ((GSource *) cd_source, pollfd);
			g_slice_free (GPollFD, pollfd);
			cd_source->pollfds = g_slist_delete_link (cd_source->pollfds, elem);
		} while ((elem = g_slist_next (elem)));
	}

	g_slist_free (cd_source->pollfds);
}

static GSourceFuncs cd_usb_source_funcs = {
	cd_usb_source_prepare,
	cd_usb_source_check,
	cd_usb_source_dispatch,
	cd_usb_source_finalize,
	NULL, NULL
	};

/**
 * cd_usb_attach_to_context:
 * @usb:  a #CdUsb instance
 * @context: a #GMainContext or %NULL
 *
 * Connects up usb-1 with the GLib event loop. This functionality
 * allows you to submit async requests using usb, and the callbacks
 * just kinda happen at the right time.
 **/
gboolean
cd_usb_attach_to_context (CdUsb *usb,
			  GMainContext *context,
			  GError **error)
{
	guint i;
	gboolean ret = FALSE;
	const struct libusb_pollfd **pollfds;
	CdUsbPrivate *priv = usb->priv;

	/* load libusb if we've not done this already */
	ret = cd_usb_load (usb, error);
	if (!ret)
		goto out;

	/* create new CdUsbSource */
	if (priv->source == NULL) {
		priv->source = (CdUsbSource *) g_source_new (&cd_usb_source_funcs,
							     sizeof(CdUsbSource));
		priv->source->pollfds = NULL;

		/* assign user_data */
		g_source_set_callback ((GSource *)priv->source, NULL, usb, NULL);

		/* attach to the mainloop */
		g_source_attach ((GSource *)priv->source, context);
	}

	/* watch the fd's already created */
	pollfds = libusb_get_pollfds (usb->priv->ctx);
	for (i=0; pollfds[i] != NULL; i++)
		cd_libusb_pollfd_add (usb, pollfds[i]->fd, pollfds[i]->events);

	/* watch for PollFD changes */
	libusb_set_pollfd_notifiers (priv->ctx,
				     cd_libusb_pollfd_added_cb,
				     cd_libusb_pollfd_removed_cb,
				     usb);
out:
	return ret;
}

/**
 * cd_usb_load:
 * @usb:  a #CdUsb instance
 * @error:  a #GError, or %NULL
 *
 * Connects to libusb. You normally don't have to call this method manually.
 *
 * Return value: %TRUE for success
 **/
gboolean
cd_usb_load (CdUsb *usb, GError **error)
{
	gboolean ret = FALSE;
	gint rc;
	CdUsbPrivate *priv = usb->priv;

	/* already done */
	if (priv->ctx != NULL) {
		ret = TRUE;
		goto out;
	}

	/* init */
	rc = libusb_init (&priv->ctx);
	if (rc < 0) {
		g_set_error (error, CD_USB_ERROR,
			     CD_USB_ERROR_INTERNAL,
			     "failed to init libusb: %s [%i]",
			     libusb_strerror (rc), rc);
		goto out;
	}

	/* enable logging */
	libusb_set_debug (priv->ctx, 3);

	/* success */
	ret = TRUE;
	priv->connected = TRUE;
out:
	return ret;
}

/**
 * cd_usb_get_device_handle:
 * @usb:  a #CdUsb instance
 *
 * Gets the low-level device handle
 *
 * Return value: The #libusb_device_handle or %NULL. Do not unref this value.
 **/
libusb_device_handle *
cd_usb_get_device_handle (CdUsb *usb)
{
	return usb->priv->handle;
}

/**
 * cd_usb_connect:
 * @usb:  a #CdUsb instance
 * @vendor_id: the vendor ID to connect to
 * @product_id: the product ID to connect to
 * @configuration: the configuration index to use, usually '1'
 * @interface: the configuration interface to use, usually '0'
 * @error:  a #GError, or %NULL
 *
 * Connects to a specific device.
 *
 * Return value: %TRUE for success
 **/
gboolean
cd_usb_connect (CdUsb *usb,
		guint vendor_id,
		guint product_id,
		guint configuration,
		guint interface,
		GError **error)
{
	gint rc;
	gboolean ret = FALSE;
	CdUsbPrivate *priv = usb->priv;

	/* already connected */
	if (priv->handle != NULL) {
		g_set_error_literal (error, CD_USB_ERROR,
				     CD_USB_ERROR_INTERNAL,
				     "already connected to a device");
		goto out;
	}

	/* load libusb if we've not done this already */
	ret = cd_usb_load (usb, error);
	if (!ret)
		goto out;

	/* open device */
	priv->handle = libusb_open_device_with_vid_pid (priv->ctx,
							vendor_id,
							product_id);
	if (priv->handle == NULL) {
		g_set_error (error, CD_USB_ERROR,
			     CD_USB_ERROR_INTERNAL,
			     "failed to find device %04x:%04x",
			     vendor_id, product_id);
		ret = FALSE;
		goto out;
	}

	/* set configuration and interface */
	rc = libusb_set_configuration (priv->handle, configuration);
	if (rc < 0) {
		g_set_error (error, CD_USB_ERROR,
			     CD_USB_ERROR_INTERNAL,
			     "failed to set configuration 0x%02x: %s [%i]",
			     configuration,
			     libusb_strerror (rc), rc);
		ret = FALSE;
		goto out;
	}
	rc = libusb_claim_interface (priv->handle, interface);
	if (rc < 0) {
		g_set_error (error, CD_USB_ERROR,
			     CD_USB_ERROR_INTERNAL,
			     "failed to claim interface 0x%02x: %s [%i]",
			     interface,
			     libusb_strerror (rc), rc);
		ret = FALSE;
		goto out;
	}
out:
	return ret;
}

/**
 * cd_usb_disconnect:
 * @usb:  a #CdUsb instance
 * @error:  a #GError, or %NULL
 *
 * Disconnecs from the current device.
 *
 * Return value: %TRUE for success
 **/
gboolean
cd_usb_disconnect (CdUsb *usb,
		   GError **error)
{
	gboolean ret = FALSE;
	CdUsbPrivate *priv = usb->priv;

	/* already connected */
	if (priv->handle == NULL) {
		g_set_error_literal (error, CD_USB_ERROR,
				     CD_USB_ERROR_INTERNAL,
				     "not connected to a device");
		goto out;
	}

	/* just close */
	libusb_close (priv->handle);
	priv->handle = NULL;

	/* disconnect the event source */
	libusb_set_pollfd_notifiers (usb->priv->ctx,
				     NULL, NULL, NULL);
	cd_libusb_pollfd_remove_all (usb);

	/* success */
	ret = TRUE;
out:
	return ret;
}

/**
 * cd_usb_get_property:
 **/
static void
cd_usb_get_property (GObject *object,
		     guint prop_id,
		     GValue *value,
		     GParamSpec *pspec)
{
	CdUsb *usb = CD_USB (object);
	CdUsbPrivate *priv = usb->priv;

	switch (prop_id) {
	case PROP_CONNECTED:
		g_value_set_boolean (value, priv->connected);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * cd_usb_set_property:
 **/
static void
cd_usb_set_property (GObject *object,
		     guint prop_id,
		     const GValue *value,
		     GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * cd_usb_class_init:
 **/
static void
cd_usb_class_init (CdUsbClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = cd_usb_finalize;
	object_class->get_property = cd_usb_get_property;
	object_class->set_property = cd_usb_set_property;

	/**
	 * CdUsb:connected:
	 */
	pspec = g_param_spec_boolean ("connected", NULL, NULL,
				      FALSE,
				      G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_CONNECTED, pspec);

	g_type_class_add_private (klass, sizeof (CdUsbPrivate));
}

/**
 * cd_usb_init:
 **/
static void
cd_usb_init (CdUsb *usb)
{
	usb->priv = CD_USB_GET_PRIVATE (usb);
}

/**
 * cd_usb_finalize:
 **/
static void
cd_usb_finalize (GObject *object)
{
	CdUsb *usb = CD_USB (object);
	CdUsbPrivate *priv = usb->priv;

	if (priv->ctx != NULL) {
		libusb_set_pollfd_notifiers (usb->priv->ctx,
					     NULL, NULL, NULL);
		cd_libusb_pollfd_remove_all (usb);
		libusb_exit (priv->ctx);
	}
	if (priv->handle != NULL)
		libusb_close (priv->handle);
	if (priv->source != NULL)
		g_source_destroy ((GSource *) priv->source);

	G_OBJECT_CLASS (cd_usb_parent_class)->finalize (object);
}

/**
 * cd_usb_new:
 *
 * Return value: a new #CdUsb object.
 **/
CdUsb *
cd_usb_new (void)
{
	CdUsb *usb;
	usb = g_object_new (CD_TYPE_USB, NULL);
	return CD_USB (usb);
}

