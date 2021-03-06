/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
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
 * SECTION:mcm-print
 * @short_description: Print device abstraction
 *
 * This object allows the programmer to detect a color sensor device.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "mcm-print.h"

#include "egg-debug.h"

static void     mcm_print_finalize	(GObject     *object);

#define MCM_PRINT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MCM_TYPE_PRINT, McmPrintPrivate))

/**
 * McmPrintPrivate:
 *
 * Private #McmPrint data
 **/
struct _McmPrintPrivate
{
	GtkPrintSettings		*settings;
};

enum {
	SIGNAL_STATUS_CHANGED,
	SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (McmPrint, mcm_print, G_TYPE_OBJECT)

/* temporary object so we can pass state */
typedef struct {
	McmPrint		*print;
	GPtrArray		*filenames;
	McmPrintRenderCb	 render_callback;
	gpointer		 user_data;
	GMainLoop		*loop;
	gboolean		 aborted;
	GError			*error;
} McmPrintTask;

/**
 * mcm_print_class_init:
 **/
static void
mcm_print_class_init (McmPrintClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = mcm_print_finalize;

	/**
	 * McmPrint::status-changed:
	 **/
	signals[SIGNAL_STATUS_CHANGED] =
		g_signal_new ("status-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (McmPrintClass, status_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);

	g_type_class_add_private (klass, sizeof (McmPrintPrivate));
}

/**
 * mcm_print_begin_print_cb:
 *
 * Emitted after the user has finished changing print settings in the dialog,
 * before the actual rendering starts.
 **/
static void
mcm_print_begin_print_cb (GtkPrintOperation *operation, GtkPrintContext *context, McmPrintTask *task)
{
	GtkPageSetup *page_setup;

	/* get the page details */
	page_setup = gtk_print_context_get_page_setup (context);

	/* get the list of files */
	task->filenames = task->render_callback (task->print, page_setup, task->user_data, &task->error);
	if (task->filenames == NULL) {
		gtk_print_operation_cancel (operation);
		goto out;
	}

	/* setting the page count */
	egg_debug ("setting %i pages", task->filenames->len);
	gtk_print_operation_set_n_pages (operation, task->filenames->len);
out:
	return;
}

/**
 * mcm_print_draw_page_cb:
 *
 * Emitted for every page that is printed. The signal handler must render the page onto the cairo context
 **/
static void
mcm_print_draw_page_cb (GtkPrintOperation *operation, GtkPrintContext *context, gint page_nr, McmPrintTask *task)
{
	cairo_t *cr;
	gdouble width = 0.0f;
	gdouble height = 0.0f;
	const gchar *filename;
	GdkPixbuf *pixbuf = NULL;
	cairo_surface_t *surface = NULL;
	gdouble scale;

	/* get the size of the page in _pixels_ */
	width = gtk_print_context_get_width (context);
	height = gtk_print_context_get_height (context);
	cr = gtk_print_context_get_cairo_context (context);

	/* load pixbuf, which we've already prepared */
	filename = g_ptr_array_index (task->filenames, page_nr);
	pixbuf = gdk_pixbuf_new_from_file (filename, &task->error);
	if (pixbuf == NULL) {
		gtk_print_operation_cancel (operation);
		goto out;
	}

	/* create a surface of the pixmap */
	surface = cairo_image_surface_create_for_data (gdk_pixbuf_get_pixels (pixbuf),
						       CAIRO_FORMAT_RGB24,
						       gdk_pixbuf_get_width (pixbuf),
						       gdk_pixbuf_get_height (pixbuf),
						       gdk_pixbuf_get_rowstride (pixbuf));

	/* scale image to fill the page, but preserve aspect */
	scale = MIN (width / gdk_pixbuf_get_width (pixbuf), height / gdk_pixbuf_get_height (pixbuf));
	egg_debug ("surface=%.0fx%.0f, pixbuf=%ix%i (scale=%f)", width, height, gdk_pixbuf_get_width (pixbuf), gdk_pixbuf_get_height (pixbuf), scale);
	cairo_scale (cr, scale, scale);

	/* blit to the context */
	cairo_set_source_surface (cr, surface, 0, 0);
	cairo_paint (cr);
out:
	if (surface != NULL)
		cairo_surface_destroy (surface);
	if (pixbuf != NULL)
		g_object_unref (pixbuf);
}

/**
 * mcm_print_loop_quit_idle_cb:
 **/
static gboolean
mcm_print_loop_quit_idle_cb (McmPrintTask *task)
{
	g_main_loop_quit (task->loop);
	return FALSE;
}

/**
 * mcm_print_status_changed_cb:
 **/
static void
mcm_print_status_changed_cb (GtkPrintOperation *operation, McmPrintTask *task)
{
	GtkPrintStatus status;

	/* signal the status change */
	status = gtk_print_operation_get_status (operation);
	egg_debug ("emit: status-changed: %i", status);
	g_signal_emit (task->print, signals[SIGNAL_STATUS_CHANGED], 0, status);

	/* done? */
	if (status == GTK_PRINT_STATUS_FINISHED) {
		egg_debug ("printing finished");
		g_idle_add ((GSourceFunc) mcm_print_loop_quit_idle_cb, task);
	} else if (status == GTK_PRINT_STATUS_FINISHED_ABORTED) {
		task->aborted = TRUE;

		/* we failed, and didn't set an error */
		if (task->error == NULL)
			g_set_error (&task->error, 1, 0, "printing was aborted, and no error was set");

		egg_debug ("printing aborted");
		g_idle_add ((GSourceFunc) mcm_print_loop_quit_idle_cb, task);
	}
}

/**
 * mcm_print_done_cb:
 **/
static void
mcm_print_done_cb (GtkPrintOperation *operation, GtkPrintOperationResult result, McmPrintTask *task)
{
	egg_debug ("we're done rendering...");
}

/**
 * mcm_print_with_render_callback:
 **/
gboolean
mcm_print_with_render_callback (McmPrint *print, GtkWindow *window, McmPrintRenderCb render_callback, gpointer user_data, GError **error)
{
	McmPrintPrivate *priv = print->priv;
	gboolean ret = TRUE;
	McmPrintTask *task;
	GtkPrintOperation *operation;
	GtkPrintOperationResult res;

	/* create temp object */
	task = g_new0 (McmPrintTask, 1);
	task->print = g_object_ref (print);
	task->render_callback = render_callback;
	task->user_data = user_data;
	task->loop = g_main_loop_new (NULL, FALSE);

	/* create new instance */
	operation = gtk_print_operation_new ();
	gtk_print_operation_set_print_settings (operation, priv->settings);
	g_signal_connect (operation, "begin-print", G_CALLBACK (mcm_print_begin_print_cb), task);
	g_signal_connect (operation, "draw-page", G_CALLBACK (mcm_print_draw_page_cb), task);
	g_signal_connect (operation, "status-changed", G_CALLBACK (mcm_print_status_changed_cb), task);
	g_signal_connect (operation, "done", G_CALLBACK (mcm_print_done_cb), task);

	/* we want this to be as big as possible, modulo page margins */
	gtk_print_operation_set_use_full_page (operation, FALSE);

	/* don't show status, we've got it covered */
	gtk_print_operation_set_show_progress (operation, FALSE);

	/* don't support selection */
	gtk_print_operation_set_support_selection (operation, FALSE);

	/* track status even when spooled */
	gtk_print_operation_set_track_print_status (operation, TRUE);

	/* we want to be able to use custom page sizes */
	gtk_print_operation_set_embed_page_setup (operation, TRUE);

	/* do the print UI */
	res = gtk_print_operation_run (operation,
				       GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
				       window, error);

	/* all okay, so save future settings */
	if (res == GTK_PRINT_OPERATION_RESULT_APPLY) {
		g_object_unref (priv->settings);
		priv->settings = g_object_ref (gtk_print_operation_get_print_settings (operation));
	}

	/* rely on error being set */
	if (res == GTK_PRINT_OPERATION_RESULT_ERROR) {
		ret = FALSE;
		goto out;
	}

	/* wait for finished or abort */
	g_main_loop_run (task->loop);

	/* pass on error */
	if (task->error != NULL) {
		g_set_error_literal (error, 1, 0, task->error->message);
		ret = FALSE;
	}
out:
	if (task->filenames != NULL)
		g_ptr_array_unref (task->filenames);
	if (task->print != NULL)
		g_object_unref (task->print);
	if (task->loop != NULL)
		g_main_loop_unref (task->loop);
	if (task->error != NULL)
		g_error_free (task->error);
	g_free (task);
	g_object_unref (operation);
	return ret;
}

/**
 * mcm_print_init:
 **/
static void
mcm_print_init (McmPrint *print)
{
	print->priv = MCM_PRINT_GET_PRIVATE (print);
	print->priv->settings = gtk_print_settings_new ();
}

/**
 * mcm_print_finalize:
 **/
static void
mcm_print_finalize (GObject *object)
{
	McmPrint *print = MCM_PRINT (object);
	McmPrintPrivate *priv = print->priv;

	g_object_unref (priv->settings);

	G_OBJECT_CLASS (mcm_print_parent_class)->finalize (object);
}

/**
 * mcm_print_new:
 *
 * Return value: a new McmPrint object.
 **/
McmPrint *
mcm_print_new (void)
{
	McmPrint *print;
	print = g_object_new (MCM_TYPE_PRINT, NULL);
	return MCM_PRINT (print);
}

