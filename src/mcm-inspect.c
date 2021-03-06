/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2010 Richard Hughes <richard@hughsie.com>
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

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libmate-desktop/mate-rr.h>
#include <locale.h>

#include "egg-debug.h"

#include "mcm-utils.h"
#include "mcm-profile.h"
#include "mcm-xserver.h"
#include "mcm-screen.h"

/**
 * mcm_inspect_print_data_info:
 **/
static gboolean
mcm_inspect_print_data_info (const gchar *title, const guint8 *data, gsize length)
{
	McmProfile *profile = NULL;
	GError *error = NULL;
	gboolean ret;

	/* parse the data */
	profile = mcm_profile_new ();
	ret = mcm_profile_parse_data (profile, data, length, &error);
	if (!ret) {
		egg_warning ("failed to parse data: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* print title */
	g_print ("%s\n", title);

	/* TRANSLATORS: this is the ICC profile description stored in an atom in the XServer */
	g_print (" - %s %s\n", _("Description:"), mcm_profile_get_description (profile));

	/* TRANSLATORS: this is the ICC profile copyright */
	g_print (" - %s %s\n", _("Copyright:"), mcm_profile_get_copyright (profile));
out:
	if (profile != NULL)
		g_object_unref (profile);
	return ret;
}

/**
 * mcm_inspect_show_x11_atoms:
 **/
static gboolean
mcm_inspect_show_x11_atoms (void)
{
	gboolean ret;
	guint8 *data = NULL;
	guint8 *data_tmp;
	gsize length;
	McmXserver *xserver = NULL;
	MateRROutput **outputs;
	guint i;
	McmScreen *screen = NULL;
	const gchar *output_name;
	gchar *title;
	GError *error = NULL;
	guint major;
	guint minor;

	/* setup object to access X */
	xserver = mcm_xserver_new ();

	/* get profile from XServer */
	ret = mcm_xserver_get_root_window_profile_data (xserver, &data, &length, &error);
	if (!ret) {
		egg_warning ("failed to get XServer profile data: %s", error->message);
		g_error_free (error);
		/* non-fatal */
		error = NULL;
	} else {
		/* TRANSLATORS: the root window of all the screens */
		mcm_inspect_print_data_info (_("Root window profile:"), data, length);
	}

	/* get profile from XServer */
	ret = mcm_xserver_get_protocol_version (xserver, &major, &minor, &error);
	if (!ret) {
		egg_warning ("failed to get XServer protocol version: %s", error->message);
		g_error_free (error);
		/* non-fatal */
		error = NULL;
	} else {
		/* TRANSLATORS: the root window of all the screens */
		g_print ("%s %i.%i\n", _("Root window protocol version:"), major, minor);
	}

	/* coldplug devices */
	screen = mcm_screen_new ();
	outputs = mcm_screen_get_outputs (screen, &error);
	if (outputs == NULL) {
		ret = FALSE;
		egg_warning ("failed to get outputs: %s", error->message);
		g_error_free (error);
		goto out;
	}
	for (i=0; outputs[i] != NULL; i++) {

		/* get output name */
		output_name = mate_rr_output_get_name (outputs[i]);
		title = g_strdup_printf (_("Output profile '%s':"), output_name);

		/* get profile from XServer */
		ret = mcm_xserver_get_output_profile_data (xserver, output_name, &data_tmp, &length, &error);
		if (!ret) {
			egg_warning ("failed to get output profile data: %s", error->message);
			/* TRANSLATORS: this is when the profile has not been set */
			g_print ("%s %s\n", title, _("not set"));
			g_error_free (error);
			/* non-fatal */
			error = NULL;
		} else {
			/* TRANSLATORS: the output, i.e. the flat panel */
			mcm_inspect_print_data_info (title, data_tmp, length);
			g_free (data_tmp);
		}
		g_free (title);
	}
out:
	g_free (data);
	if (screen != NULL)
		g_object_unref (screen);
	if (xserver != NULL)
		g_object_unref (xserver);
	return ret;
}

/**
 * mcm_inspect_show_profiles_for_device:
 **/
static gboolean
mcm_inspect_show_profiles_for_device (const gchar *device_id)
{
	gboolean ret = FALSE;
	const gchar *description;
	const gchar *filename;
	guint i = 0;
	GDBusConnection *connection;
	GError *error = NULL;
	GVariant *args = NULL;
	GVariant *response = NULL;
	GVariantIter *iter = NULL;

	/* get a session bus connection */
	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (connection == NULL) {
		/* TRANSLATORS: no DBus session bus */
		g_print ("%s %s\n", _("Failed to connect to session bus:"), error->message);
		g_error_free (error);
		goto out;
	}

	/* execute sync method */
	args = g_variant_new ("(ss)", device_id, ""),
	response = g_dbus_connection_call_sync (connection,
						MCM_DBUS_SERVICE,
						MCM_DBUS_PATH,
						MCM_DBUS_INTERFACE,
						"GetProfilesForDevice",
						args,
						G_VARIANT_TYPE ("(a(ss))"),
						G_DBUS_CALL_FLAGS_NONE,
						-1, NULL, &error);
	if (response == NULL) {
		/* TRANSLATORS: the DBus method failed */
		g_print ("%s %s\n", _("The request failed:"), error->message);
		g_error_free (error);
		goto out;
	}

	/* unpack the array */
	g_variant_get (response, "(a(ss))", &iter);
	if (g_variant_iter_n_children (iter) == 0) {
		/* TRANSLATORS: no profile has been asigned to this device */
		g_print ("%s\n", _("There are no ICC profiles for this device"));
		goto out;
	}

	/* TRANSLATORS: this is a list of profiles suitable for the device */
	g_print ("%s %s\n", _("Suitable profiles for:"), device_id);

	/* for each entry in the array */
	while (g_variant_iter_loop (iter, "(ss)", &filename, &description))
		g_print ("%i.\t%s\n\t%s\n", ++i, description, filename);

	/* success */
	ret = TRUE;
out:
 	if (iter != NULL)
 		g_variant_iter_free (iter);
	if (args != NULL)
		g_variant_unref (args);
 	if (response != NULL)
 		g_variant_unref (response);
 	return ret;
}

/**
 * mcm_inspect_show_profiles_for_file:
 **/
static gboolean
mcm_inspect_show_profiles_for_file (const gchar *filename)
{
	gboolean ret = FALSE;
	const gchar *description;
	guint i = 0;
	GDBusConnection *connection;
	GError *error = NULL;
	GVariant *args = NULL;
	GVariant *response = NULL;
	GVariantIter *iter = NULL;

	/* get a session bus connection */
	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (connection == NULL) {
		/* TRANSLATORS: no DBus session bus */
		g_print ("%s %s\n", _("Failed to connect to session bus:"), error->message);
		g_error_free (error);
		goto out;
	}

	/* execute sync method */
	args = g_variant_new ("(ss)", filename, ""),
	response = g_dbus_connection_call_sync (connection,
						MCM_DBUS_SERVICE,
						MCM_DBUS_PATH,
						MCM_DBUS_INTERFACE,
						"GetProfilesForFile",
						args,
						G_VARIANT_TYPE ("(a(ss))"),
						G_DBUS_CALL_FLAGS_NONE,
						-1, NULL, &error);
	if (response == NULL) {
		/* TRANSLATORS: the DBus method failed */
		g_print ("%s %s\n", _("The request failed:"), error->message);
		g_error_free (error);
		goto out;
	}

	/* unpack the array */
	g_variant_get (response, "(a(ss))", &iter);
	if (g_variant_iter_n_children (iter) == 0) {
		/* TRANSLATORS: no profile has been asigned to this device */
		g_print ("%s\n", _("There are no ICC profiles assigned to this file"));
		goto out;
	}

	/* TRANSLATORS: this is a list of profiles suitable for the device */
	g_print ("%s %s\n", _("Suitable profiles for:"), filename);

	/* for each entry in the array */
	while (g_variant_iter_loop (iter, "(ss)", &filename, &description))
		g_print ("%i.\t%s\n\t%s\n", ++i, description, filename);

	/* success */
	ret = TRUE;
out:
 	if (iter != NULL)
 		g_variant_iter_free (iter);
	if (args != NULL)
		g_variant_unref (args);
 	if (response != NULL)
 		g_variant_unref (response);
 	return ret;
}

/**
 * mcm_inspect_show_profiles_for_devices:
 **/
static gboolean
mcm_inspect_show_profiles_for_devices (void)
{
	gboolean ret = FALSE;
	GDBusConnection *connection;
	GError *error = NULL;
	guint i;
	const gchar **devices = NULL;
	GVariant *response = NULL;
	GVariant *response_child = NULL;

	/* get a session bus connection */
	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (connection == NULL) {
		/* TRANSLATORS: no DBus session bus */
		g_print ("%s %s\n", _("Failed to connect to session bus:"), error->message);
		g_error_free (error);
		goto out;
	}

	/* execute sync method */
	response = g_dbus_connection_call_sync (connection,
						MCM_DBUS_SERVICE,
						MCM_DBUS_PATH,
						MCM_DBUS_INTERFACE,
						"GetDevices",
						NULL,
						G_VARIANT_TYPE ("(as)"),
						G_DBUS_CALL_FLAGS_NONE,
						-1, NULL, &error);
	if (response == NULL) {
		/* TRANSLATORS: the DBus method failed */
		g_print ("%s %s\n", _("The request failed:"), error->message);
		g_error_free (error);
		goto out;
	}

	/* print each device */
	response_child = g_variant_get_child_value (response, 0);
	devices = g_variant_get_strv (response_child, NULL);
	for (i=0; devices[i] != NULL; i++) {
		ret = mcm_inspect_show_profiles_for_device (devices[i]);
		if (!ret)
			goto out;
	}

	/* success */
	ret = TRUE;
out:
	g_free (devices);
	if (response != NULL)
		g_variant_unref (response);
	if (response_child != NULL)
		g_variant_unref (response_child);
	return ret;
}

/**
 * mcm_inspect_show_profile_for_window:
 **/
static gboolean
mcm_inspect_show_profile_for_window (guint xid)
{
	gboolean ret = FALSE;
	GDBusConnection *connection;
	GError *error = NULL;
	const gchar *profile;
	GVariant *args = NULL;
	GVariant *response = NULL;
	GVariant *response_child = NULL;
	GVariantIter *iter = NULL;

	/* get a session bus connection */
	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (connection == NULL) {
		/* TRANSLATORS: no DBus session bus */
		g_print ("%s %s\n", _("Failed to connect to session bus:"), error->message);
		g_error_free (error);
		goto out;
	}

	/* execute sync method */
	args = g_variant_new ("(u)", xid),
	response = g_dbus_connection_call_sync (connection,
						MCM_DBUS_SERVICE,
						MCM_DBUS_PATH,
						MCM_DBUS_INTERFACE,
						"GetProfileForWindow",
						args,
						G_VARIANT_TYPE ("(s)"),
						G_DBUS_CALL_FLAGS_NONE,
						-1, NULL, &error);
	if (response == NULL) {
		/* TRANSLATORS: the DBus method failed */
		g_print ("%s %s\n", _("The request failed:"), error->message);
		g_error_free (error);
		goto out;
	}

	/* print each device */
	response_child = g_variant_get_child_value (response, 0);
	profile = g_variant_get_string (response_child, NULL);

	/* no data */
	if (profile == NULL) {
		/* TRANSLATORS: no profile has been asigned to this window */
		g_print ("%s\n", _("There are no ICC profiles for this window"));
		goto out;
	}

	/* TRANSLATORS: this is a list of profiles suitable for the device */
	g_print ("%s %i\n", _("Suitable profiles for:"), xid);
	g_print ("1.\t%s\n\t%s\n", "this is a title", profile);

	/* success */
	ret = TRUE;
out:
	if (iter != NULL)
 		g_variant_iter_free (iter);
	if (args != NULL)
		g_variant_unref (args);
 	if (response != NULL)
 		g_variant_unref (response);
	return ret;
}

/**
 * mcm_inspect_show_profiles_for_type:
 **/
static gboolean
mcm_inspect_show_profiles_for_type (const gchar *type)
{
	gboolean ret = FALSE;
	GDBusConnection *connection;
	GError *error = NULL;
	const gchar *filename = NULL;
	const gchar *description = NULL;
	guint i = 0;
	GVariant *args = NULL;
	GVariant *response = NULL;
	GVariantIter *iter = NULL;

	/* get a session bus connection */
	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (connection == NULL) {
		/* TRANSLATORS: no DBus session bus */
		g_print ("%s %s\n", _("Failed to connect to session bus:"), error->message);
		g_error_free (error);
		goto out;
	}

	/* execute sync method */
	args = g_variant_new ("(ss)", type, ""),
	response = g_dbus_connection_call_sync (connection,
						MCM_DBUS_SERVICE,
						MCM_DBUS_PATH,
						MCM_DBUS_INTERFACE,
						"GetProfilesForType",
						args,
						G_VARIANT_TYPE ("(a(ss))"),
						G_DBUS_CALL_FLAGS_NONE,
						-1, NULL, &error);
	if (response == NULL) {
		/* TRANSLATORS: the DBus method failed */
		g_print ("%s %s\n", _("The request failed:"), error->message);
		g_error_free (error);
		goto out;
	}

	/* unpack the array */
	g_variant_get (response, "(a(ss))", &iter);
	if (g_variant_iter_n_children (iter) == 0) {
		/* TRANSLATORS: no profile has been asigned to this device */
 		g_print ("%s\n", _("There are no ICC profiles for this device type"));
		goto out;
	}

	/* TRANSLATORS: this is a list of profiles suitable for the device */
	g_print ("%s %s\n", _("Suitable profiles for:"), type);

	/* for each entry in the array */
	while (g_variant_iter_loop (iter, "(ss)", &filename, &description))
		g_print ("%i.\t%s\n\t%s\n", ++i, description, filename);

	/* success */
	ret = TRUE;
out:
	if (iter != NULL)
		g_variant_iter_free (iter);
	if (args != NULL)
		g_variant_unref (args);
	if (response != NULL)
		g_variant_unref (response);
	return ret;
}

/**
 * mcm_inspect_get_properties:
 **/
static gboolean
mcm_inspect_get_properties (void)
{
	GDBusProxy *proxy;
	GError *error = NULL;
	GVariant *result;

	/* connect to the named instance */
	proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
					       G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
					       NULL,
					       MCM_DBUS_SERVICE,
					       MCM_DBUS_PATH,
					       MCM_DBUS_INTERFACE,
					       NULL,
					       &error);
	if (proxy == NULL) {
		/* TRANSLATORS: the DBus method failed */
		g_print ("%s: %s\n", _("The request failed"), error->message);
		g_error_free (error);
		goto out;
	}

	/* get rendering intents */
	result = g_dbus_proxy_get_cached_property (proxy, "RenderingIntentDisplay");
	if (result != NULL) {
		/* TRANSLATORS: this is the rendering intent of the output */
		g_print ("%s:\t%s\n", _("Rendering intent (display)"), g_variant_get_string (result, NULL));
		g_variant_unref (result);
	}
	result = g_dbus_proxy_get_cached_property (proxy, "RenderingIntentSoftproof");
	if (result != NULL) {
		/* TRANSLATORS: this is the rendering intent of the printer */
		g_print ("%s\t%s\n", _("Rendering intent (display):"), g_variant_get_string (result, NULL));
		g_variant_unref (result);
	}

	/* get colorspaces */
	result = g_dbus_proxy_get_cached_property (proxy, "ColorspaceRgb");
	if (result != NULL) {
		/* TRANSLATORS: this is the rendering intent of the output */
		g_print ("%s\t%s\n", _("RGB Colorspace:"), g_variant_get_string (result, NULL));
		g_variant_unref (result);
	}
	result = g_dbus_proxy_get_cached_property (proxy, "ColorspaceCmyk");
	if (result != NULL) {
		/* TRANSLATORS: this is the rendering intent of the printer */
		g_print ("%s\t%s\n", _("CMYK Colorspace:"), g_variant_get_string (result, NULL));
		g_variant_unref (result);
	}
out:
	if (proxy != NULL)
		g_object_unref (proxy);
	return TRUE;
}

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	gboolean x11 = FALSE;
	gboolean dump = FALSE;
	guint xid = 0;
	gchar *device_id = NULL;
	gchar *type = NULL;
	gchar *filename = NULL;
	McmDeviceKind kind_enum;
	guint retval = 0;
	GOptionContext *context;

	const GOptionEntry options[] = {
		{ "x11", 'x', 0, G_OPTION_ARG_NONE, &x11,
			/* TRANSLATORS: command line option */
			_("Show X11 properties"), NULL },
		{ "device", '\0', 0, G_OPTION_ARG_STRING, &device_id,
			/* TRANSLATORS: command line option */
			_("Get the profiles for a specific device"), NULL },
		{ "file", '\0', 0, G_OPTION_ARG_STRING, &filename,
			/* TRANSLATORS: command line option */
			_("Get the profiles for a specific file"), NULL },
		{ "xid", '\0', 0, G_OPTION_ARG_INT, &xid,
			/* TRANSLATORS: command line option */
			_("Get the profile for a specific window"), NULL },
		{ "type", '\0', 0, G_OPTION_ARG_STRING, &type,
			/* TRANSLATORS: command line option */
			_("Get the profiles for a specific device type"), NULL },
		{ "dump", 'd', 0, G_OPTION_ARG_NONE, &dump,
			/* TRANSLATORS: command line option */
			_("Dump all details about this system"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	/* TRANSLATORS: just dumps the EDID to disk */
	context = g_option_context_new (_("EDID inspect program"));
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, egg_debug_get_option_group ());
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	if (x11 || dump)
		mcm_inspect_show_x11_atoms ();
	if (device_id != NULL)
		mcm_inspect_show_profiles_for_device (device_id);
	if (filename != NULL)
		mcm_inspect_show_profiles_for_file (filename);
	if (xid != 0)
		mcm_inspect_show_profile_for_window (xid);
	if (type != NULL) {
		kind_enum = mcm_device_kind_from_string (type);
		if (kind_enum == MCM_DEVICE_KIND_UNKNOWN) {
			/* TRANSLATORS: this is when the user does --type=mickeymouse */
			g_print ("%s\n", _("Device type not recognized"));
		} else {
			/* show device profiles */
			mcm_inspect_show_profiles_for_type (type);
		}
	}
	if (dump) {
		mcm_inspect_get_properties ();
		mcm_inspect_show_profiles_for_devices ();
	}

	g_free (device_id);
	g_free (filename);
	g_free (type);
	return retval;
}

