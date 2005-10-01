/*
 * gedit-time-plugin.h
 * 
 * Copyright (C) 2002-2005 - Paolo Maggi
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * $Id$
 */

/*
 * Modified by the gedit Team, 2002. See the AUTHORS file for a 
 * list of people on the gedit Team.  
 * See the ChangeLog files for a list of changes. 
 */
 
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <time.h>

#include <gconf/gconf-client.h>
#include <libgnome/gnome-config.h>

#include "gedit-time-plugin.h"
#include <gedit/gedit-help.h>

#include <glib/gi18n-lib.h>
#include <gmodule.h>

#include <gedit/gedit-debug.h>
#include <gedit/gedit-utils.h>

#define GEDIT_TIME_PLUGIN_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), GEDIT_TYPE_TIME_PLUGIN, GeditTimePluginPrivate))

#define WINDOW_DATA_KEY "GeditTimePluginWindowData"
#define MENU_PATH "/MenuBar/EditMenu/EditOps_4"

#define TIME_BASE_KEY 	"/apps/gedit-2/plugins/time"
#define SELECTED_FORMAT_KEY "/selected_format"
#define PROMPT_TYPE_KEY "/prompt_type"
#define CUSTOM_FORMAT_KEY "/custom_format"

#define DIALOG_PROMPT_TYPE_KEY "gedit-2/time_plugin/prompt_type"

#define DEFAULT_CUSTOM_FORMAT "%d/%m/%Y %H:%M:%S"

enum
{
	COLUMN_FORMATS = 0,
	COLUMN_INDEX,
	NUM_COLUMNS
};

typedef struct _TimeConfigureDialog TimeConfigureDialog;

struct _TimeConfigureDialog
{
	GtkWidget *dialog;

	GtkWidget *list;
	
        /* Radio buttons to indicate what should be done */
        GtkWidget *prompt;
        GtkWidget *use_list;
        GtkWidget *custom;
        
        GtkWidget *custom_entry;
	GtkWidget *custom_format_example;

	/* Info needed for the response handler */
	GeditTimePlugin *plugin;	
};

typedef struct _ChooseFormatDialog ChooseFormatDialog;

struct _ChooseFormatDialog
{
	GtkWidget *dialog;

	GtkWidget *list;
	
        /* Radio buttons to indicate what should be done */
        GtkWidget *use_list;
        GtkWidget *custom;
        
        GtkWidget *custom_entry;
	GtkWidget *custom_format_example;
	
	/* Info needed for the response handler */
	GtkTextBuffer   *buffer;
	GeditTimePlugin *plugin;
};

static gchar *formats[] =
{
	"%c",
	"%x",
	"%X",
	"%x %X",
	"%Y-%m-%d %H:%M:%S",
	"%a %b %d %H:%M:%S %Z %Y",
	"%a %b %d %H:%M:%S %Y",
	"%a %d %b %Y %H:%M:%S %Z",
	"%a %d %b %Y %H:%M:%S",
	"%d/%m/%Y",
	"%d/%m/%y",
	"%D",
	"%A %d %B %Y",
	"%A %B %d %Y",
	"%Y-%m-%d",
	"%d %B %Y",
	"%B %d, %Y",
	"%A %b %d",
	"%H:%M:%S",
	"%H:%M",
	"%I:%M:%S %p",
	"%I:%M %p",
	"%H.%M.%S",
	"%H.%M",
	"%I.%M.%S %p",
	"%I.%M %p",
	"%d/%m/%Y %H:%M:%S",
	"%d/%m/%y %H:%M:%S",
#if __GLIBC__ >= 2
	"%a, %d %b %Y %H:%M:%S %z",
#endif
	NULL
};

typedef enum 
{
	PROMPT_FOR_FORMAT = 0,
	USE_SELECTED_FORMAT,
	USE_CUSTOM_FORMAT
} GeditTimePluginPromptType;

struct _GeditTimePluginPrivate
{
	GConfClient *gconf_client;
	
	gchar *last_custom_format;
	gchar *last_selected_format;
	
	GeditTimePluginPromptType last_prompt_type;	
};

GEDIT_PLUGIN_REGISTER_TYPE(GeditTimePlugin, gedit_time_plugin)

typedef struct
{
	GtkActionGroup *action_group;
	guint           ui_id;
} WindowData;

typedef struct
{
	GeditWindow     *window;
	GeditTimePlugin *plugin;
} ActionData;

static void time_cb (GtkAction *action, ActionData *data);

static const GtkActionEntry action_entries[] =
{
	{ 
		"InsertDateAndTime",
		NULL,
		N_("In_sert Date and Time..."),
		NULL,
		N_("Insert current date and time at the cursor position"),
		G_CALLBACK (time_cb) 
	},
};

static void
gedit_time_plugin_init (GeditTimePlugin *plugin)
{
	gedit_debug_message (DEBUG_PLUGINS, "GeditTimePlugin initializing");
	
	plugin->priv = GEDIT_TIME_PLUGIN_GET_PRIVATE (plugin);

	plugin->priv->gconf_client = gconf_client_get_default ();

	gconf_client_add_dir (plugin->priv->gconf_client,
			      TIME_BASE_KEY,
			      GCONF_CLIENT_PRELOAD_ONELEVEL,
			      NULL);
			      
	plugin->priv->last_prompt_type = gnome_config_get_int (DIALOG_PROMPT_TYPE_KEY "=1");			      
}

static void
gedit_time_plugin_finalize (GObject *object)
{
	GeditTimePlugin *plugin = GEDIT_TIME_PLUGIN (object);

	gedit_debug_message (DEBUG_PLUGINS, "GeditTimePlugin finalizing");

	gconf_client_suggest_sync (plugin->priv->gconf_client, NULL);
	
	g_object_unref (G_OBJECT (plugin->priv->gconf_client));
	
	g_free (plugin->priv->last_custom_format);
	g_free (plugin->priv->last_selected_format);
	
	gnome_config_set_int (DIALOG_PROMPT_TYPE_KEY, plugin->priv->last_prompt_type);
	gnome_config_sync ();
	
	G_OBJECT_CLASS (gedit_time_plugin_parent_class)->finalize (object);
}

static void
free_window_data (WindowData *data)
{
	g_return_if_fail (data != NULL);

	g_object_unref (data->action_group);
	g_free (data);
}

static void
update_ui_real (GeditWindow  *window,
		WindowData   *data)
{
	GeditView *view;
	GtkAction *action;

	gedit_debug (DEBUG_PLUGINS);

	view = gedit_window_get_active_view (window);

	gedit_debug_message (DEBUG_PLUGINS, "View: %p", view);
	
	action = gtk_action_group_get_action (data->action_group,
					      "InsertDateAndTime");
	gtk_action_set_sensitive (action,
				  (view != NULL) &&
				  gtk_text_view_get_editable (GTK_TEXT_VIEW (view)));
}

static void
impl_activate (GeditPlugin *plugin,
	       GeditWindow *window)
{
	GtkUIManager *manager;
	WindowData *data;
	ActionData *action_data;
	
	gedit_debug (DEBUG_PLUGINS);

	data = g_new (WindowData, 1);
	action_data = g_new (ActionData, 1);
	
	action_data->plugin = GEDIT_TIME_PLUGIN (plugin);
	action_data->window = window;
	
	manager = gedit_window_get_ui_manager (window);

	data->action_group = gtk_action_group_new ("GeditTimePluginActions");
	gtk_action_group_set_translation_domain (data->action_group, 
						 GETTEXT_PACKAGE);
	gtk_action_group_add_actions_full (data->action_group, 
				      	   action_entries,
				      	   G_N_ELEMENTS (action_entries),
				      	   action_data,
				      	   (GDestroyNotify) g_free);

	gtk_ui_manager_insert_action_group (manager, data->action_group, -1);

	data->ui_id = gtk_ui_manager_new_merge_id (manager);

	g_object_set_data_full (G_OBJECT (window), 
				WINDOW_DATA_KEY, 
				data,
				(GDestroyNotify) free_window_data);

	gtk_ui_manager_add_ui (manager, 
			       data->ui_id, 
			       MENU_PATH,
			       "InsertDateAndTime", 
			       "InsertDateAndTime",
			       GTK_UI_MANAGER_MENUITEM, 
			       FALSE);

	update_ui_real (window, data);
}

static void
impl_deactivate	(GeditPlugin *plugin,
		 GeditWindow *window)
{
	GtkUIManager *manager;
	WindowData *data;

	gedit_debug (DEBUG_PLUGINS);

	manager = gedit_window_get_ui_manager (window);

	data = (WindowData *) g_object_get_data (G_OBJECT (window), WINDOW_DATA_KEY);
	g_return_if_fail (data != NULL);

	gtk_ui_manager_remove_ui (manager, data->ui_id);
	gtk_ui_manager_remove_action_group (manager, data->action_group);

	g_object_set_data (G_OBJECT (window), WINDOW_DATA_KEY, NULL);
}

static void
impl_update_ui	(GeditPlugin *plugin,
		 GeditWindow *window)
{
	WindowData   *data;

	gedit_debug (DEBUG_PLUGINS);

	data = (WindowData *) g_object_get_data (G_OBJECT (window), WINDOW_DATA_KEY);
	g_return_if_fail (data != NULL);

	update_ui_real (window, data);
}

static GeditTimePluginPromptType
get_prompt_type (GeditTimePlugin *plugin)
{
	gchar *prompt_type;
	GeditTimePluginPromptType res;
	       	
	prompt_type = gconf_client_get_string (plugin->priv->gconf_client,
			        TIME_BASE_KEY PROMPT_TYPE_KEY,
				NULL);

	if (prompt_type == NULL)
		return PROMPT_FOR_FORMAT;

	if (strcmp (prompt_type, "USE_SELECTED_FORMAT") == 0)
		res = USE_SELECTED_FORMAT;
	else 
	{
		if  (strcmp (prompt_type, "USE_CUSTOM_FORMAT") == 0)
			res = USE_CUSTOM_FORMAT;
		else
			res = PROMPT_FOR_FORMAT;
	}

	g_free (prompt_type);

	return res;
}

static void
set_prompt_type (GeditTimePlugin           *plugin,
		 GeditTimePluginPromptType  prompt_type)
{
	const gchar * str;

	if (!gconf_client_key_is_writable (plugin->priv->gconf_client,
					   TIME_BASE_KEY PROMPT_TYPE_KEY, 
					   NULL))
		return;
					 

	switch (prompt_type)
	{
		case USE_SELECTED_FORMAT:
			str = "USE_SELECTED_FORMAT";
			break;
		case USE_CUSTOM_FORMAT:
			str = "USE_CUSTOM_FORMAT";
			break;
		default:
			str = "PROMPT_FOR_FORMAT";
	}
	
	gconf_client_set_string (plugin->priv->gconf_client, 
				 TIME_BASE_KEY PROMPT_TYPE_KEY,
		       		 str, 
		       		 NULL);
}
	
static gchar *
get_selected_format (GeditTimePlugin *plugin)
{
	gchar *sel_format;
		       	
	sel_format = gconf_client_get_string (plugin->priv->gconf_client,
			        	      TIME_BASE_KEY SELECTED_FORMAT_KEY,
			        	      NULL);
			        	      
	if (sel_format == NULL)
		return g_strdup (formats [0]);

	return sel_format;
}

static void
set_selected_format (GeditTimePlugin *plugin,
		     const gchar     *format)
{
	g_return_if_fail (format != NULL);
	
	if (!gconf_client_key_is_writable (plugin->priv->gconf_client, 
					   TIME_BASE_KEY SELECTED_FORMAT_KEY, 
					   NULL))
		return;
		
	gconf_client_set_string (plugin->priv->gconf_client, 
				 TIME_BASE_KEY SELECTED_FORMAT_KEY,
		       		 format, 
		       		 NULL);
}

static gchar *
get_custom_format (GeditTimePlugin *plugin)
{
	gchar *format;
	       	
	format = gconf_client_get_string (plugin->priv->gconf_client,
			        	  TIME_BASE_KEY CUSTOM_FORMAT_KEY,
			        	  NULL);

	if (format == NULL)
		return g_strdup (DEFAULT_CUSTOM_FORMAT);

	return format;
}

static void
set_custom_format (GeditTimePlugin *plugin,
		   const gchar     *format)
{
	g_return_if_fail (format != NULL);

	if (!gconf_client_key_is_writable (plugin->priv->gconf_client,
					   TIME_BASE_KEY CUSTOM_FORMAT_KEY, 
					   NULL))
		return;

	gconf_client_set_string (plugin->priv->gconf_client, 
				 TIME_BASE_KEY CUSTOM_FORMAT_KEY,
		       		 format, 
		       		 NULL);
}

static gchar *
get_time (const gchar* format)
{
  	gchar *out = NULL;
	gchar *out_utf8 = NULL;
  	time_t clock;
  	struct tm *now;
  	size_t out_length = 0;
	gchar *locale_format;

	gedit_debug (DEBUG_PLUGINS);

	g_return_val_if_fail (format != NULL, NULL);

	if (strlen (format) == 0)
		return g_strdup (" ");

	locale_format = g_locale_from_utf8 (format, -1, NULL, NULL, NULL);
	
	if (locale_format == NULL)
		return g_strdup (" ");
		
  	clock = time (NULL);
  	now = localtime (&clock);
	  	
	do
	{
		out_length += 255;
		out = g_realloc (out, out_length);
	}
  	while (strftime (out, out_length, locale_format, now) == 0);
  	
	g_free (locale_format);

	if (g_utf8_validate (out, -1, NULL))
		out_utf8 = out;
	else
	{
		out_utf8 = g_locale_to_utf8 (out, -1, NULL, NULL, NULL);
		g_free (out);

		if (out_utf8 == NULL)
			out_utf8 = g_strdup (" ");
	}
	
  	return out_utf8;
}

static void
dialog_destroyed (GtkObject *obj,  gpointer dialog_pointer)
{
	gedit_debug (DEBUG_PLUGINS);
	
	g_free (dialog_pointer);

	gedit_debug_message (DEBUG_PLUGINS, "END");
	
}

static GtkTreeModel *
create_model (GtkWidget       *listview, 
	      const gchar     *sel_format,
	      GeditTimePlugin *plugin)
{
	gint i = 0;
	GtkListStore *store;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	gchar *sf;

	gedit_debug (DEBUG_PLUGINS);

	/* create list store */
	store = gtk_list_store_new (NUM_COLUMNS, G_TYPE_STRING, G_TYPE_INT);

	/* Set tree view model*/
	gtk_tree_view_set_model (GTK_TREE_VIEW (listview), 
				 GTK_TREE_MODEL (store));

	g_object_unref (G_OBJECT (store));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (listview));
	g_return_val_if_fail (selection != NULL, GTK_TREE_MODEL (store));

	/* there should always be one line selected */
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

	if (sel_format == NULL)
		sf = get_selected_format (plugin);
	else
		sf = g_strdup (sel_format);
	
	/* add data to the list store */
	while (formats[i] != NULL)
	{
		gchar *str;

		str = get_time (formats[i]);

		gedit_debug_message (DEBUG_PLUGINS, "%d : %s", i, str);
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COLUMN_FORMATS, str,
				    COLUMN_INDEX, i,
				    -1);
		g_free (str);	

		if (strncmp (formats[i], sf, strlen (sf)) == 0)
			gtk_tree_selection_select_iter (selection, &iter);

		++i;
	}

	/* fall back to select the first iter */
	if (!gtk_tree_selection_get_selected (selection, NULL, NULL))
	{
		gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter);
		gtk_tree_selection_select_iter (selection, &iter);
	}

	g_free (sf);

	return GTK_TREE_MODEL (store);
}

static void 
scroll_to_selected (GtkTreeView *tree_view)
{
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;

	gedit_debug (DEBUG_PLUGINS);

	model = gtk_tree_view_get_model (tree_view);
	g_return_if_fail (model != NULL);

	/* Scroll to selected */
	selection = gtk_tree_view_get_selection (tree_view);
	g_return_if_fail (selection != NULL);

	if (gtk_tree_selection_get_selected (selection, NULL, &iter))
	{
		GtkTreePath* path;

		path = gtk_tree_model_get_path (model, &iter);
		g_return_if_fail (path != NULL);

		gtk_tree_view_scroll_to_cell (tree_view,
					      path, NULL, TRUE, 1.0, 0.0);
		gtk_tree_path_free (path);
	}
}

static void
create_formats_list (GtkWidget       *listview, 
		     const gchar     *sel_format,
		     GeditTimePlugin *plugin)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell;
	
	gedit_debug (DEBUG_PLUGINS);

	g_return_if_fail (listview != NULL);
	
	/* the Available formats column */
	cell = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (
			_("Available formats"), 
			cell, 
			"text", COLUMN_FORMATS, 
			NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (listview), column);

	/* Create model, it also add model to the tree view */
	create_model (listview, sel_format, plugin);

	g_signal_connect (G_OBJECT (listview), 
			  "realize", 
			  G_CALLBACK (scroll_to_selected), 
			  NULL);

	gtk_widget_show (listview);
}

static void 
updated_custom_format_example (GtkEntry *format_entry, 
			       GtkLabel *format_example)
{
	const gchar *format;
	gchar *time;
	gchar *str;
	gchar *escaped_time;
	
	gedit_debug (DEBUG_PLUGINS);

	g_return_if_fail (GTK_IS_ENTRY (format_entry));
	g_return_if_fail (GTK_IS_LABEL (format_example));

	format = gtk_entry_get_text (format_entry);

	time = get_time (format);
	escaped_time = g_markup_escape_text (time, -1);
	
	str = g_strdup_printf ("<span size=\"small\">%s</span>", escaped_time);
	
	gtk_label_set_markup (format_example, str);

	g_free (escaped_time);
	g_free (time);
	g_free (str);
}

static void
choose_format_dialog_button_toggled (GtkToggleButton *button, 
				     ChooseFormatDialog *dialog)
{
	gedit_debug (DEBUG_PLUGINS);

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->custom)))
	{
		gtk_widget_set_sensitive (dialog->list, FALSE);
		gtk_widget_set_sensitive (dialog->custom_entry, TRUE);
		gtk_widget_set_sensitive (dialog->custom_format_example, TRUE);

		return;
	}

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->use_list)))
	{
		gtk_widget_set_sensitive (dialog->list, TRUE);
		gtk_widget_set_sensitive (dialog->custom_entry, FALSE);
		gtk_widget_set_sensitive (dialog->custom_format_example, FALSE);

		return;
	}
}

static void
configure_dialog_button_toggled (GtkToggleButton *button, TimeConfigureDialog *dialog)
{
	gedit_debug (DEBUG_PLUGINS);

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->custom)))
	{
		gtk_widget_set_sensitive (dialog->list, FALSE);
		gtk_widget_set_sensitive (dialog->custom_entry, TRUE);
		gtk_widget_set_sensitive (dialog->custom_format_example, TRUE);

		return;
	}

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->use_list)))
	{
		gtk_widget_set_sensitive (dialog->list, TRUE);
		gtk_widget_set_sensitive (dialog->custom_entry, FALSE);
		gtk_widget_set_sensitive (dialog->custom_format_example, FALSE);

		return;
	}

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->prompt)))
	{
		gtk_widget_set_sensitive (dialog->list, FALSE);
		gtk_widget_set_sensitive (dialog->custom_entry, FALSE);
		gtk_widget_set_sensitive (dialog->custom_format_example, FALSE);

		return;
	}
}

static gint
get_format_from_list (GtkWidget *listview)
{
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
        gint selected_value;

	gedit_debug (DEBUG_PLUGINS);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (listview));
	g_return_val_if_fail (model != NULL, 0);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (listview));
	g_return_val_if_fail (selection != NULL, 0);

	if (gtk_tree_selection_get_selected (selection, NULL, &iter))
	{
		gtk_tree_model_get (model, &iter, COLUMN_INDEX, &selected_value, -1);
	}
	
	gedit_debug_message (DEBUG_PLUGINS, "Sel value: %d", selected_value);

        return selected_value;
}

static void
ok_button_pressed (TimeConfigureDialog *dialog)
{
	gint sel_format;
	const gchar *custom_format;

	gedit_debug (DEBUG_PLUGINS);
   
	sel_format = get_format_from_list (dialog->list);
   
	custom_format = gtk_entry_get_text (GTK_ENTRY (dialog->custom_entry));
	
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->custom)))
	{
		set_prompt_type (dialog->plugin, USE_CUSTOM_FORMAT);
		set_custom_format (dialog->plugin, custom_format);
	}
	else 
	{
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->use_list)))
		{
			set_prompt_type (dialog->plugin, USE_SELECTED_FORMAT);

			set_selected_format (dialog->plugin, formats [sel_format]);
		}
		else
     			/* Default to always prompt the user */
			set_prompt_type (dialog->plugin, PROMPT_FOR_FORMAT);
	}

	gedit_debug_message (DEBUG_PLUGINS, "Sel: %d", sel_format);
}

static TimeConfigureDialog * 
get_configure_dialog (GeditTimePlugin *plugin)
{
	TimeConfigureDialog *dialog = NULL;

	GtkWidget *content;
	GtkWidget *viewport;
	GeditTimePluginPromptType prompt_type;
	gchar *sf, *cf;
	GtkWidget *error_widget;
	gboolean ret;

	gedit_debug (DEBUG_PLUGINS);

	dialog = g_new0 (TimeConfigureDialog, 1);
	
	dialog->dialog = gtk_dialog_new_with_buttons (_("Configure insert date/time plugin..."),
						      NULL,
						      GTK_DIALOG_DESTROY_WITH_PARENT,
						      GTK_STOCK_CANCEL,
						      GTK_RESPONSE_CANCEL,
						      GTK_STOCK_OK,
						      GTK_RESPONSE_OK,
						      GTK_STOCK_HELP,
						      GTK_RESPONSE_HELP,
						      NULL);
						      
	g_return_val_if_fail (dialog->dialog != NULL, NULL);
						      
	ret = gedit_utils_get_glade_widgets (GEDIT_GLADEDIR "time.glade2",
					     "time_dialog_content",
					     &error_widget,
					     "time_dialog_content", &content,
					     "formats_viewport", &viewport,
					     "formats_tree", &dialog->list,
					     "always_prompt", &dialog->prompt,
					     "never_prompt", &dialog->use_list,
					     "use_custom", &dialog->custom,
					     "custom_entry", &dialog->custom_entry,
					     "custom_format_example", &dialog->custom_format_example,
					     NULL);

	if (!ret)
	{
		gtk_box_pack_start_defaults (GTK_BOX (GTK_DIALOG (dialog->dialog)->vbox),
					     error_widget);

		gtk_widget_show (error_widget);

		return dialog;
	}
	
	gtk_window_set_resizable (GTK_WINDOW (dialog->dialog), FALSE);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog->dialog), FALSE);

	sf = get_selected_format (plugin);
	create_formats_list (dialog->list, sf, plugin);
	g_free (sf);

	prompt_type = get_prompt_type (plugin);

	cf = get_custom_format (plugin);
     	gtk_entry_set_text (GTK_ENTRY(dialog->custom_entry), cf);
       	g_free (cf);

        if (prompt_type == USE_CUSTOM_FORMAT)
        {
	        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->custom), TRUE);

		gtk_widget_set_sensitive (dialog->list, FALSE);
		gtk_widget_set_sensitive (dialog->custom_entry, TRUE);
		gtk_widget_set_sensitive (dialog->custom_format_example, TRUE);
        }
        else if (prompt_type == USE_SELECTED_FORMAT)
        {
	        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->use_list), TRUE);

		gtk_widget_set_sensitive (dialog->list, TRUE);
		gtk_widget_set_sensitive (dialog->custom_entry, FALSE);
		gtk_widget_set_sensitive (dialog->custom_format_example, FALSE);
        }
        else
        {
	        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->prompt), TRUE);

		gtk_widget_set_sensitive (dialog->list, FALSE);
		gtk_widget_set_sensitive (dialog->custom_entry, FALSE);
		gtk_widget_set_sensitive (dialog->custom_format_example, FALSE);
        }

	updated_custom_format_example (GTK_ENTRY (dialog->custom_entry), 
			GTK_LABEL (dialog->custom_format_example));
	
	/* setup a window of a sane size. */
	gtk_widget_set_size_request (GTK_WIDGET (viewport), 10, 200);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog->dialog)->vbox),
			    content, FALSE, FALSE, 0);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog->dialog),
					 GTK_RESPONSE_OK);

	g_signal_connect (G_OBJECT (dialog->custom), "toggled",
			  G_CALLBACK (configure_dialog_button_toggled), 
			  dialog);

   	g_signal_connect (G_OBJECT (dialog->prompt), "toggled",
			  G_CALLBACK (configure_dialog_button_toggled), 
			  dialog);

	g_signal_connect (G_OBJECT (dialog->use_list), "toggled",
			  G_CALLBACK (configure_dialog_button_toggled), 
			  dialog);

	g_signal_connect (G_OBJECT (dialog->dialog), "destroy",
			  G_CALLBACK (dialog_destroyed), dialog);
	
	g_signal_connect (G_OBJECT (dialog->custom_entry), "changed",
			  G_CALLBACK (updated_custom_format_example), 
			  dialog->custom_format_example);

	return dialog;
}

static ChooseFormatDialog *
get_choose_format_dialog (GtkWindow *parent, GeditTimePlugin *plugin)
{
	ChooseFormatDialog *dialog;
	GtkWidget *error_widget;
	gboolean ret;

	dialog = g_new0 (ChooseFormatDialog, 1);

	ret = gedit_utils_get_glade_widgets (GEDIT_GLADEDIR "time.glade2",
					     "choose_format_dialog",
					     &error_widget,
					     "choose_format_dialog", &dialog->dialog,
					     "choice_list", &dialog->list,
					     "use_sel_format_radiobutton", &dialog->use_list,
					     "use_custom_radiobutton", &dialog->custom,
					     "custom_entry", &dialog->custom_entry,
					     "custom_format_example", &dialog->custom_format_example,
					     NULL);

	if (!ret)
	{
		gedit_warning (parent,
			       gtk_label_get_label (GTK_LABEL (error_widget)));

		g_free (dialog);
		gtk_widget_destroy (error_widget);

		return NULL;
	}

	if (plugin->priv->last_selected_format == NULL)
		plugin->priv->last_selected_format = get_selected_format (plugin);

	create_formats_list (dialog->list, 
			     plugin->priv->last_selected_format,
			     plugin);

	if (plugin->priv->last_custom_format == NULL)
		plugin->priv->last_custom_format = get_custom_format (plugin);
	
     	gtk_entry_set_text (GTK_ENTRY(dialog->custom_entry), 
     			    plugin->priv->last_custom_format);
	updated_custom_format_example (GTK_ENTRY (dialog->custom_entry), 
				       GTK_LABEL (dialog->custom_format_example));

	if (plugin->priv->last_prompt_type == USE_CUSTOM_FORMAT)
	{
        	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->custom), TRUE);
		
		gtk_widget_set_sensitive (dialog->list, FALSE);
		gtk_widget_set_sensitive (dialog->custom_entry, TRUE);
		gtk_widget_set_sensitive (dialog->custom_format_example, TRUE);
	}
	else
	{
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->use_list), TRUE);

		gtk_widget_set_sensitive (dialog->list, TRUE);
		gtk_widget_set_sensitive (dialog->custom_entry, FALSE);
		gtk_widget_set_sensitive (dialog->custom_format_example, FALSE);
	}

	/* setup a window of a sane size. */
	gtk_widget_set_size_request (dialog->list, 10, 200);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog->dialog),
					 GTK_RESPONSE_OK);

	g_signal_connect (G_OBJECT (dialog->custom), "toggled",
			  G_CALLBACK (choose_format_dialog_button_toggled), 
			  dialog);

	g_signal_connect (G_OBJECT (dialog->use_list), "toggled",
			  G_CALLBACK (choose_format_dialog_button_toggled), 
			  dialog);

	g_signal_connect (G_OBJECT (dialog->dialog), "destroy",
			  G_CALLBACK (dialog_destroyed), dialog);
	
	g_signal_connect (G_OBJECT (dialog->custom_entry), "changed",
			  G_CALLBACK (updated_custom_format_example), 
			  dialog->custom_format_example);

	gtk_window_set_resizable (GTK_WINDOW (dialog->dialog), FALSE);

	return dialog;
}

static void
real_insert_time (GtkTextBuffer *buffer,
		  const gchar   *the_time)
{
	gedit_debug_message (DEBUG_PLUGINS, "Insert: %s", the_time);
	
	gtk_text_buffer_begin_user_action (buffer);

	gtk_text_buffer_insert_at_cursor (buffer, the_time, -1);
	gtk_text_buffer_insert_at_cursor (buffer, " ", -1);

	gtk_text_buffer_end_user_action (buffer);
}

static void
choose_format_dialog_response_cb (GtkWidget          *widget, 
				  gint                response, 
				  ChooseFormatDialog *dialog)
{
	switch (response)
	{
		case GTK_RESPONSE_HELP:
		{
			gedit_debug_message (DEBUG_PLUGINS, "GTK_RESPONSE_HELP");	
			gedit_help_display (GTK_WINDOW (widget),
					    "gedit.xml",
					    "gedit-insert-date-time-plugin");
			break;
		}
		case GTK_RESPONSE_OK:
		{
			gchar *the_time;

			gedit_debug_message (DEBUG_PLUGINS, "GTK_RESPONSE_OK");
						
			/* Get the user's chosen format */
			if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->use_list)))
			{
				gint sel_format;
						
				sel_format = get_format_from_list (dialog->list);
				the_time = get_time (formats[sel_format]);
						
				g_free (dialog->plugin->priv->last_selected_format);		
				dialog->plugin->priv->last_selected_format = g_strdup (formats[sel_format]);

				dialog->plugin->priv->last_prompt_type = USE_SELECTED_FORMAT;
				set_selected_format (dialog->plugin,
						     dialog->plugin->priv->last_selected_format);
			}
			else
			{
				const gchar *format;
						
				format = gtk_entry_get_text (GTK_ENTRY (dialog->custom_entry));
				the_time = get_time (format);

				g_free (dialog->plugin->priv->last_custom_format);					
				dialog->plugin->priv->last_custom_format = g_strdup (format);

				dialog->plugin->priv->last_prompt_type = USE_CUSTOM_FORMAT;
				set_custom_format (dialog->plugin,
						   dialog->plugin->priv->last_custom_format);
			}

			g_return_if_fail (the_time != NULL);

			real_insert_time (dialog->buffer, the_time);
			g_free (the_time);
		
			gtk_widget_destroy (dialog->dialog);
			break;
		}	
		case GTK_RESPONSE_CANCEL:
			gedit_debug_message (DEBUG_PLUGINS, "GTK_RESPONSE_CANCEL");
			gtk_widget_destroy (dialog->dialog);
	}	
}

static void 
time_cb (GtkAction  *action,
	 ActionData *data)
{
	GtkTextBuffer *buffer;
	gchar *the_time = NULL;
	GeditTimePluginPromptType prompt_type;

	gedit_debug (DEBUG_PLUGINS);

	buffer = GTK_TEXT_BUFFER (gedit_window_get_active_document (data->window));
	g_return_if_fail (buffer != NULL);

	prompt_type = get_prompt_type (data->plugin);

        if (prompt_type == USE_CUSTOM_FORMAT)
        {
		gchar *cf = get_custom_format (data->plugin);
	        the_time = get_time (cf);
		g_free (cf);
	}
        else if (prompt_type == USE_SELECTED_FORMAT)
        {
		gchar *sf = get_selected_format (data->plugin);
	        the_time = get_time (sf);
		g_free (sf);
	}
        else
        {
        	GtkWindowGroup *wg;
		ChooseFormatDialog *dialog;
		
		dialog = get_choose_format_dialog (GTK_WINDOW (data->window),
						   data->plugin);
		g_return_if_fail (dialog != NULL);

		wg = gedit_window_get_group (data->window);
		
		gtk_window_group_add_window (wg,
				     	     GTK_WINDOW (dialog->dialog));

		gtk_window_set_transient_for (GTK_WINDOW (dialog->dialog),
					      GTK_WINDOW (data->window));
		gtk_window_set_modal (GTK_WINDOW (dialog->dialog), TRUE);

		dialog->buffer = buffer;
		dialog->plugin = data->plugin;
		
		g_signal_connect (dialog->dialog,
				  "response",
				  G_CALLBACK (choose_format_dialog_response_cb),
				  dialog);

		gtk_widget_show (GTK_WIDGET (dialog->dialog));

		return;
	}

	g_return_if_fail (the_time != NULL);

	real_insert_time (buffer, the_time);

	g_free (the_time);
}

static void
configure_dialog_response_cb (GtkWidget           *widget, 
			      gint                 response, 
			      TimeConfigureDialog *dialog)
{
	switch (response)
	{
		case GTK_RESPONSE_HELP:
		{
			gedit_debug_message (DEBUG_PLUGINS, "GTK_RESPONSE_HELP");

			gedit_help_display (GTK_WINDOW (dialog),
					    "gedit.xml",
					    "gedit-date-time-configure");
			break;
		}
		case GTK_RESPONSE_OK:
		{
			gedit_debug_message (DEBUG_PLUGINS, "GTK_RESPONSE_OK");
		
			ok_button_pressed (dialog);
			
			gtk_widget_destroy (dialog->dialog);
			break;
		}	
		case GTK_RESPONSE_CANCEL:
		{
			gedit_debug_message (DEBUG_PLUGINS, "GTK_RESPONSE_CANCEL");
			gtk_widget_destroy (dialog->dialog);
		}
	}
}

static GtkWidget *
impl_create_configure_dialog (GeditPlugin *plugin)
{
	TimeConfigureDialog *dialog;

	dialog = get_configure_dialog (GEDIT_TIME_PLUGIN (plugin));
	
	dialog->plugin = GEDIT_TIME_PLUGIN (plugin);
		
	g_signal_connect (dialog->dialog,
			  "response",
			  G_CALLBACK (configure_dialog_response_cb),
			  dialog);
			  
	return GTK_WIDGET (dialog->dialog);
}


static void
gedit_time_plugin_class_init (GeditTimePluginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GeditPluginClass *plugin_class = GEDIT_PLUGIN_CLASS (klass);

	object_class->finalize = gedit_time_plugin_finalize;

	plugin_class->activate = impl_activate;
	plugin_class->deactivate = impl_deactivate;
	plugin_class->update_ui = impl_update_ui;

	plugin_class->create_configure_dialog = impl_create_configure_dialog;

	g_type_class_add_private (object_class, sizeof (GeditTimePluginPrivate));
}
