/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gedit-file.c
 * This file is part of gedit
 *
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2002 Chema Celorio, Paolo Maggi 
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
 * Foundation, Inc., 59 Temple Place, Suite 330, 
 * Boston, MA 02111-1307, USA. 
 */
 
/*
 * Modified by the gedit Team, 1998-2001. See the AUTHORS file for a 
 * list of people on the gedit Team.  
 * See the ChangeLog files for a list of changes. 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>
#include <libgnomevfs/gnome-vfs.h>

#include <eel/eel-vfs-extensions.h>

#include "gedit2.h"
#include "gedit-file.h"
#include "gedit-debug.h"
#include "gedit-utils.h"
#include "gedit-mdi.h"
#include "gedit-recent.h" 
#include "gedit-file-selector-util.h"
#include "gedit-plugins-engine.h"
#include "gnome-recent-model.h"
#include "gedit-prefs-manager.h"

static gchar 	*get_dirname_from_uri 		(const char *uri);
static gboolean  gedit_file_open_real 		(const gchar* file_name, 
						 GeditMDIChild* child);
static gboolean  gedit_file_save_as_real 	(const gchar* file_name, 
						 GeditMDIChild *child);

static gchar* gedit_default_path = NULL;

static gchar *
get_dirname_from_uri (const char *uri)
{
	GnomeVFSURI *vfs_uri;
	char *name;

	/* Make VFS version of URI. */
	vfs_uri = gnome_vfs_uri_new (uri);
	if (vfs_uri == NULL) {
		return NULL;
	}

	/* Extract name part. */
	name = gnome_vfs_uri_extract_dirname (vfs_uri);
	gnome_vfs_uri_unref (vfs_uri);

	return name;
}


void 
gedit_file_new (void)
{
	gint ret;
	GeditMDIChild* new_child = NULL;

	gedit_debug (DEBUG_FILE, "");

	new_child = gedit_mdi_child_new ();

	g_return_if_fail (new_child != NULL);
	g_return_if_fail (gedit_mdi != NULL);

	ret = bonobo_mdi_add_child (BONOBO_MDI (gedit_mdi), BONOBO_MDI_CHILD (new_child));
	g_return_if_fail (ret != FALSE);
	gedit_debug (DEBUG_COMMANDS, "Child added.");

	ret = bonobo_mdi_add_view (BONOBO_MDI (gedit_mdi), BONOBO_MDI_CHILD (new_child));
	g_return_if_fail (ret != FALSE);
	gedit_debug (DEBUG_COMMANDS, "View added.");
	
	gtk_widget_grab_focus (GTK_WIDGET (gedit_get_active_view ()));
}

void 
gedit_file_close (GtkWidget *view)
{
	gint ret;
	BonoboMDIChild* child;

	gedit_debug (DEBUG_FILE, "");

	g_return_if_fail (view != NULL);

	child = bonobo_mdi_get_child_from_view (view);
	g_return_if_fail (child != NULL);

	if (g_list_length (bonobo_mdi_child_get_views (child)) > 1)
	{		
		ret = bonobo_mdi_remove_view (BONOBO_MDI (gedit_mdi), view, FALSE);
		gedit_debug (DEBUG_COMMANDS, "View removed.");
	}
	else
	{
		ret = bonobo_mdi_remove_child (BONOBO_MDI (gedit_mdi), child, FALSE);
		gedit_debug (DEBUG_COMMANDS, "Child removed.");
	}

	if (ret)
		gedit_mdi_set_active_window_title (BONOBO_MDI (gedit_mdi));

	if (bonobo_mdi_get_active_child (BONOBO_MDI (gedit_mdi)) == NULL)
	{
		gedit_mdi_set_active_window_verbs_sensitivity (BONOBO_MDI (gedit_mdi));
		gedit_mdi_clear_active_window_statusbar (gedit_mdi);
	}

	gedit_debug (DEBUG_FILE, "END");
}

void
gedit_file_open (GeditMDIChild *active_child)
{
	gchar** files;
	
	gedit_debug (DEBUG_FILE, "");

	files = gedit_file_selector_open_multi (
			GTK_WINDOW (bonobo_mdi_get_active_window (BONOBO_MDI (gedit_mdi))),
			TRUE,
		        _("Open File ..."), 
			NULL, 
			gedit_default_path);
	
	if (files) 
	{
		gint i;
		for (i = 0; files[i]; ++i)
		{
			gedit_debug (DEBUG_FILE, "[%d]: %s", i, files[i]);

			if (gedit_file_open_real (files[i], active_child))
			{
				gchar *uri_utf8;
				
				if (gedit_utils_uri_has_file_scheme (files[i]))
				{				
					if (gedit_default_path != NULL)
						g_free (gedit_default_path);

					gedit_default_path = get_dirname_from_uri (files[i]);
				}
				
				uri_utf8 = eel_format_uri_for_display (files[i]);	
				if (uri_utf8 != NULL)
				{
					gedit_utils_flash_va (_("Loaded file '%s'"), uri_utf8);

					g_free (uri_utf8);
				}
							
			}
			
			gedit_debug (DEBUG_FILE, "File: %s", files[i]);
		}

		g_strfreev (files);
	}
}

static gboolean
gedit_file_open_real (const gchar* file_name, GeditMDIChild* active_child)
{
	GError *error = NULL;
	gchar *uri;
	
	GnomeRecentModel *recent;

	gedit_debug (DEBUG_FILE, "File name: %s", file_name);

	uri = eel_make_uri_canonical (file_name);
	g_return_val_if_fail (uri != NULL, FALSE);
	gedit_debug (DEBUG_FILE, "Canonical uri: %s", uri);
	
	if (active_child == NULL ||
	    !gedit_document_is_untouched (active_child->document))	     
	{
		gint ret;
		GeditMDIChild* new_child = NULL;

		new_child = gedit_mdi_child_new_with_uri (uri, &error);

		if (error)
		{
			gedit_utils_error_reporting_loading_file (uri, error,
					GTK_WINDOW (gedit_get_active_window ()));
			
			g_error_free (error);
			g_free (uri);
			return FALSE;
		}	
			
		g_return_val_if_fail (new_child != NULL, FALSE);
		g_return_val_if_fail (gedit_mdi != NULL, FALSE);

		ret = bonobo_mdi_add_child (BONOBO_MDI (gedit_mdi), BONOBO_MDI_CHILD (new_child));
		g_return_val_if_fail (ret != FALSE, FALSE);
		gedit_debug (DEBUG_FILE, "Child added.");

		ret = bonobo_mdi_add_view (BONOBO_MDI (gedit_mdi), BONOBO_MDI_CHILD (new_child));
		g_return_val_if_fail (ret != FALSE, FALSE);
		gedit_debug (DEBUG_FILE, "View added.");
	}
	else
	{		
		gedit_document_load (active_child->document, uri, &error);

		if (error)
		{
			gedit_utils_error_reporting_loading_file (uri, error,
					GTK_WINDOW (gedit_get_active_window ()));
			
			g_error_free (error);
			g_free (uri);
			return FALSE;
		}	
	}
	
	recent = gedit_recent_get_model ();
	gnome_recent_model_add (recent, uri);

	g_free (uri);

	gedit_debug (DEBUG_FILE, "END: %s\n", file_name);

	return TRUE;
}

gboolean 
gedit_file_save (GeditMDIChild* child)
{
	gint ret;
	GeditDocument* doc = NULL;
	GError *error = NULL;
	gchar *uri = NULL;
	
	gedit_debug (DEBUG_FILE, "");

	g_return_val_if_fail (child != NULL, FALSE);
	
	doc = child->document;
	g_return_val_if_fail (doc != NULL, FALSE);
	
	if (gedit_document_is_untitled (doc))
	{
		gedit_debug (DEBUG_FILE, "Untitled");

		return gedit_file_save_as (child);
	}

	if (!gedit_document_get_modified (doc))	
	{
		gchar *raw_uri;
		gboolean deleted = FALSE;
		
		gedit_debug (DEBUG_FILE, "Not modified");

		raw_uri = gedit_document_get_raw_uri (doc);
		if (raw_uri != NULL)
		{
			if (gedit_document_is_readonly (doc))
				deleted = FALSE;
			else
				deleted = !gedit_utils_uri_exists (raw_uri);
		}
		g_free (raw_uri);
			
		if (!deleted)
			return TRUE;
	}
	
	uri = gedit_document_get_uri (doc);
	g_return_val_if_fail (uri != NULL, FALSE);
	
	gedit_utils_flash_va (_("Saving file '%s' ..."), uri);	
	
	ret = gedit_document_save (doc, &error);

	if (!ret)
	{
		GtkWidget *view;
		
		g_return_val_if_fail (error != NULL, FALSE);
		gedit_debug (DEBUG_FILE, "FAILED");

		view = GTK_WIDGET (g_list_nth_data (
					bonobo_mdi_child_get_views (BONOBO_MDI_CHILD (child)), 0));
		if (view != NULL)
		{
			GtkWindow *window;

			window = GTK_WINDOW (bonobo_mdi_get_window_from_view (view));
			gtk_window_present (window);

			bonobo_mdi_set_active_view (BONOBO_MDI (gedit_mdi), view);
		}
		
		gedit_utils_error_reporting_saving_file (uri, error,
					GTK_WINDOW (gedit_get_active_window ()));

		g_error_free (error);

		gedit_utils_flash_va (_("The document has not been saved."));

		g_free (uri);

		return FALSE;
	}	
	else
	{
		GnomeRecentModel *recent;
		gchar *raw_uri;
		
		gedit_debug (DEBUG_FILE, "OK");

		gedit_utils_flash_va (_("File '%s' saved."), uri);

		g_free (uri);

		raw_uri = gedit_document_get_raw_uri (doc);
		g_return_val_if_fail (raw_uri != NULL, TRUE);
		
		recent = gedit_recent_get_model ();
		gnome_recent_model_add (recent, raw_uri);

		g_free (raw_uri);
		
		return TRUE;
	}
}

gboolean
gedit_file_save_as (GeditMDIChild *child)
{
	gchar *file;
	gboolean ret = FALSE;
	GeditDocument *doc;
	gchar *fname = NULL;
	gchar *path = NULL;
	gchar *raw_uri = NULL;
	
	gedit_debug (DEBUG_FILE, "");

	g_return_val_if_fail (child != NULL, FALSE);

	doc = child->document;
	g_return_val_if_fail (doc != NULL, FALSE);

	raw_uri = gedit_document_get_raw_uri (doc);

	if (gedit_document_is_untitled (doc))
	{
		char *tmpstr;
		
		path = (gedit_default_path != NULL) ? 
			g_strdup (gedit_default_path) : NULL;

		tmpstr = gedit_document_get_uri (doc);
		fname = g_filename_from_utf8 (tmpstr, -1, NULL, NULL, NULL);
		if (!fname)
			/* FIXME: should fname remain NULL in this case? - Paolo */		
			fname = g_strdup ("Untitled"); /* Use ASCII */
		g_free (tmpstr);
	}
	else
	{
		g_return_val_if_fail (raw_uri != NULL, FALSE);

		fname = eel_uri_get_basename (raw_uri);

		if (gedit_utils_uri_has_file_scheme (raw_uri))
			path = get_dirname_from_uri (raw_uri);
		else
			path = (gedit_default_path != NULL) ? 
				g_strdup (gedit_default_path) : NULL;
	}
				
	g_return_val_if_fail (fname != NULL, FALSE);
	
	file = gedit_file_selector_save (
			GTK_WINDOW (bonobo_mdi_get_active_window (BONOBO_MDI (gedit_mdi))),
			FALSE,
		        _("Save as ..."), 
			NULL, 
			path,
			fname);
	
	g_free (raw_uri);
	g_free (fname);
	
	if (path != NULL)
		g_free (path);

	if (file != NULL) 
	{
		gchar *uri;
		gchar *file_utf8;

		uri = eel_make_uri_from_shell_arg (file);
		g_return_val_if_fail (uri != NULL, FALSE);

		file_utf8 = eel_format_uri_for_display (uri);
		if (file_utf8 != NULL)
			gedit_utils_flash_va (_("Saving file '%s' ..."), file_utf8);
		
		ret = gedit_file_save_as_real (uri, child);
		
		if (ret)
		{			
			if (gedit_default_path != NULL)
				g_free (gedit_default_path);

			gedit_default_path = get_dirname_from_uri (file);

			if (file_utf8 != NULL)
				gedit_utils_flash_va (_("File '%s' saved."), file_utf8);
		}
		else
			gedit_utils_flash_va (_("The document has not been saved."));

		gedit_debug (DEBUG_FILE, "File: %s", file);
		g_free (uri);
		g_free (file);

		if (file_utf8 != NULL)
			g_free (file_utf8);
		
	}

	return ret;
}

static gboolean
gedit_file_save_as_real (const gchar* file_name, GeditMDIChild *child)
{
	gchar *uri;
	gboolean ret;
	GeditDocument *doc = NULL;
	GError *error = NULL;

	gedit_debug (DEBUG_FILE, "");

	g_return_val_if_fail (child != NULL, FALSE);

	doc = child->document;
	g_return_val_if_fail (doc != NULL, FALSE);

	uri = eel_make_uri_canonical (file_name);
	g_return_val_if_fail (uri != NULL, FALSE);
	
	ret = gedit_document_save_as (doc, uri, &error);

	if (!ret)
	{
		g_return_val_if_fail (error != NULL, FALSE);

		gedit_utils_error_reporting_saving_file (file_name, error,
					GTK_WINDOW (gedit_get_active_window ()));

		g_error_free (error);
		
		g_free (uri);

		return FALSE;
		
	}	
	else
	{
#if 0
		gchar *temp;

		gedit_debug (DEBUG_FILE, "OK");

		/* uri is not valid utf8 */	
		temp = g_filename_to_utf8 (uri, -1, NULL, NULL, NULL);

		if (temp != NULL)
		{
			GnomeRecentModel *recent;

			recent = gedit_recent_get_model ();
			gnome_recent_model_add (recent, temp);

			g_free (temp);
		}

#endif
		GnomeRecentModel *recent;

		recent = gedit_recent_get_model ();
		gnome_recent_model_add (recent, uri);

		g_free (uri);

		return TRUE;
	}
}


gboolean
gedit_file_close_all (void)
{
	gboolean ret;
	gedit_debug (DEBUG_FILE, "");

	ret = bonobo_mdi_remove_all (BONOBO_MDI (gedit_mdi), FALSE);

	if (bonobo_mdi_get_active_child (BONOBO_MDI (gedit_mdi)) == NULL)
	{
		gedit_mdi_set_active_window_verbs_sensitivity (BONOBO_MDI (gedit_mdi));
		gedit_mdi_clear_active_window_statusbar (gedit_mdi); 
	}

	return ret;
}

void
gedit_file_exit (void)
{
	gedit_debug (DEBUG_FILE, "");
	
	if (!gedit_file_close_all ())
		return;

	gedit_debug (DEBUG_FILE, "All files closed.");
	
	gedit_plugins_engine_save_settings ();
	
	gedit_prefs_manager_shutdown ();
	
	gedit_debug (DEBUG_FILE, "Unref gedit_mdi.");

	g_object_unref (G_OBJECT (gedit_mdi));

	gedit_debug (DEBUG_FILE, "Unref gedit_mdi: DONE");

	gedit_debug (DEBUG_FILE, "Unref gedit_app_server.");

	bonobo_object_unref (gedit_app_server);

	gedit_debug (DEBUG_FILE, "Unref gedit_app_server: DONE");

	gtk_main_quit ();
}

void
gedit_file_save_all (void)
{
	guint i = 0;
	GeditMDIChild* child;
	GtkWidget* view;

	gedit_debug (DEBUG_FILE, "");

	view = bonobo_mdi_get_active_view (BONOBO_MDI (gedit_mdi));

	for (i = 0; i < g_list_length (bonobo_mdi_get_children (BONOBO_MDI (gedit_mdi))); i++)
	{
		child = GEDIT_MDI_CHILD (g_list_nth_data (
				bonobo_mdi_get_children (BONOBO_MDI (gedit_mdi)), i));

		if (gedit_document_get_modified (child->document))
		{
			gedit_file_save (child);	
		}
	}

	if (view !=  bonobo_mdi_get_active_view (BONOBO_MDI (gedit_mdi)))
	{
		GtkWindow *window;

		window = GTK_WINDOW (bonobo_mdi_get_window_from_view (view));
		gtk_window_present (window);

		bonobo_mdi_set_active_view (BONOBO_MDI (gedit_mdi), view);
	}
}

gboolean
gedit_file_revert (GeditMDIChild *child)
{
	gint ret;
	GeditDocument* doc = NULL;
	GError *error = NULL;
	gchar* uri = NULL;

	gedit_debug (DEBUG_FILE, "");

	g_return_val_if_fail (child != NULL, FALSE);
	
	doc = child->document;
	g_return_val_if_fail (doc != NULL, FALSE);

	uri = gedit_document_get_uri (doc);
	g_return_val_if_fail (uri != NULL, FALSE);

	gedit_utils_flash_va (_("Reverting file '%s' ..."), uri);	
	
	ret = gedit_document_revert (doc, &error);

	if (!ret)
	{
		g_return_val_if_fail (error != NULL, FALSE);

		gedit_utils_error_reporting_reverting_file (uri, error,
					GTK_WINDOW (gedit_get_active_window ()));

		g_error_free (error);

		gedit_utils_flash_va (_("The document has not been reverted."));

		g_free (uri);

		return FALSE;
	}	
	else
	{
		gedit_debug (DEBUG_FILE, "OK");

		gedit_utils_flash_va (_("File '%s' reverted."), uri);

		g_free (uri);

		return TRUE;
	}
}

gboolean 
gedit_file_open_uri_list (GList* uri_list, gint line, gboolean create)
{
	gchar *full_path;
	gboolean ret = FALSE;
	gint loaded_files = 0;
	BonoboMDIChild *active_child = NULL;
	GeditView* view = NULL;
	gint l;
	
	gedit_debug (DEBUG_FILE, "");
	
	if (uri_list == NULL) 
		return FALSE;

	l = g_list_length (uri_list);
	
	if (l > 1)
		gedit_utils_flash_va (_("Loading %d files..."), l);

	active_child = bonobo_mdi_get_active_child (BONOBO_MDI (gedit_mdi));

        /* create a file for each document in the parameter list */
	for ( ; uri_list; uri_list = g_list_next (uri_list))
	{
		full_path = eel_make_uri_from_shell_arg (uri_list->data);
	
		if (full_path != NULL) 
		{
			if (!gedit_utils_uri_has_file_scheme (full_path) || gedit_utils_uri_exists (full_path))
			{
				if (gedit_file_open_real (full_path, 
					(active_child != NULL) ? GEDIT_MDI_CHILD (active_child): NULL))
				{
					++loaded_files;
					ret |= TRUE;
				}
			}
			else
			{
				if (create)
				{
					GtkWidget *dialog;
					gboolean created = FALSE;
					gchar *formatted_path;

					formatted_path = eel_format_uri_for_display (full_path);
					g_return_val_if_fail (formatted_path != NULL, FALSE);
					
					dialog = gtk_message_dialog_new (
						GTK_WINDOW (bonobo_mdi_get_active_window (BONOBO_MDI (gedit_mdi))),
						GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					   	GTK_MESSAGE_QUESTION,
					   	GTK_BUTTONS_YES_NO,
						_("The file \"%s\" does not exist. Would you like to create it?"),
						formatted_path);

					g_free (formatted_path);

					gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_NO);

					gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

					if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES)
					{
						created = gedit_utils_create_empty_file (full_path);

						if (!created)	
							gedit_utils_error_reporting_creating_file (full_path, errno, GTK_WINDOW (dialog));
					}
									
					gtk_widget_destroy (dialog);

					if (created)
					{
						if (gedit_file_open_real (full_path, 
							(active_child != NULL) ? GEDIT_MDI_CHILD (active_child): NULL))
						{
							++loaded_files;
							ret |= TRUE;
						}
					}
				}
			}
			
			g_free (full_path);
		}

		if (view == NULL)
			view = gedit_get_active_view ();
	}

	if (view != NULL)
	{
		GeditDocument *doc;
		GtkWindow *window;

		doc = gedit_view_get_document (view);
		g_return_val_if_fail (doc, FALSE);
		
		gedit_document_goto_line (doc, MAX (0, line - 1));

		window = GTK_WINDOW (bonobo_mdi_get_window_from_view (GTK_WIDGET (view)));
		gtk_window_present (window);

		bonobo_mdi_set_active_view (BONOBO_MDI (gedit_mdi), GTK_WIDGET (view));		
	}

	if (loaded_files > 1)
		gedit_utils_flash_va (_("Loaded %i files"), loaded_files);

	return ret;
}

gboolean 
gedit_file_open_recent (GnomeRecentView *view, const gchar *uri, gpointer data)
{
	gboolean ret = FALSE;
	GeditView* active_view;

	gedit_debug (DEBUG_FILE, "Open : %s", uri);

	/* Note that gedit_file_open_single_uri takes a possibly mangled "uri", in UTF8 */

	ret = gedit_file_open_single_uri (uri);
	
	if (!ret) 
	{
		GnomeRecentModel *model;

		model = gedit_recent_get_model ();
		gnome_recent_model_delete (model, uri);
	}
		
	active_view = gedit_get_active_view ();
	if (active_view != NULL)
		gtk_widget_grab_focus (GTK_WIDGET (active_view));

	gedit_debug (DEBUG_FILE, "END");

	return ret;
}

/*
 *  uri: a possibly mangled "uri", in UTF8
 */
gboolean 
gedit_file_open_single_uri (const gchar* uri)
{
	gchar *full_path;
	gboolean ret = TRUE;

	gedit_debug (DEBUG_FILE, "");
	
	if (uri == NULL) return FALSE;
	
	full_path = eel_make_uri_from_input (uri);

	if (full_path != NULL) 
	{
		BonoboMDIChild *active_child = NULL;

		active_child = bonobo_mdi_get_active_child (BONOBO_MDI (gedit_mdi));
		
		ret = gedit_file_open_real (full_path, 
					    (active_child != NULL) ? GEDIT_MDI_CHILD (active_child): NULL);
		if (ret)
		{
			gchar *uri_utf8;

			uri_utf8 = eel_format_uri_for_display (full_path);
			
			if (uri_utf8 != NULL)
			{
				gedit_utils_flash_va (_("Loaded file '%s'"), uri_utf8);

				g_free (uri_utf8);
			}
		}

		g_free (full_path);
	}

	return ret;
}

/* FIXME: it is broken */
gboolean
gedit_file_open_from_stdin (GeditMDIChild *active_child)
{
	struct stat stats;
	gboolean ret = TRUE;
	GeditDocument *doc = NULL;
	GError *error = NULL;
	GeditMDIChild *child;
	GeditMDIChild* new_child = NULL;

	gedit_debug (DEBUG_FILE, "");
	
	fstat (STDIN_FILENO, &stats);
	
	if (stats.st_size  == 0)
		return FALSE;

	child = active_child;
	
	if (active_child == NULL ||
	    !gedit_document_is_untouched (active_child->document))	     
	{
		new_child = gedit_mdi_child_new ();

		g_return_val_if_fail (new_child != NULL, FALSE);
		g_return_val_if_fail (gedit_mdi != NULL, FALSE);

		ret = bonobo_mdi_add_child (BONOBO_MDI (gedit_mdi), BONOBO_MDI_CHILD (new_child));
		g_return_val_if_fail (ret != FALSE, FALSE);
		gedit_debug (DEBUG_FILE, "Child added.");

		child= new_child;
	}

	doc = child->document;
	g_return_val_if_fail (doc != NULL, FALSE);

	ret = gedit_document_load_from_stdin (doc, &error);

	if (error)
	{
		GtkWidget *dialog;
		/* FIXME: do a more user friendly error reporting */
		gchar *errstr;
	       	
		errstr = g_strdup_printf (_("Could not read data from stdin."));
		
		dialog = gtk_message_dialog_new (
				GTK_WINDOW (bonobo_mdi_get_active_window (BONOBO_MDI (gedit_mdi))),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			   	GTK_MESSAGE_ERROR,
			   	GTK_BUTTONS_OK,
				errstr);

		gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

		gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_free (errstr);
		g_error_free (error);

		ret = FALSE;
	}
	
	if (new_child != NULL)
	{
		ret = bonobo_mdi_add_view (BONOBO_MDI (gedit_mdi), BONOBO_MDI_CHILD (new_child));
		g_return_val_if_fail (ret != FALSE, FALSE);
		gedit_debug (DEBUG_FILE, "View added.");
	}
	
	return ret;
}

