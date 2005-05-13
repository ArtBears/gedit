/*
 * gedit-plugins-engine.c
 * This file is part of gedit
 *
 * Copyright (C) 2002-2005 Paolo Maggi 
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
 * Modified by the gedit Team, 2002-2005. See the AUTHORS file for a 
 * list of people on the gedit Team.  
 * See the ChangeLog files for a list of changes. 
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib/gi18n.h>
#include <glib/gkeyfile.h>
#include <libgnome/gnome-util.h>
#include <gconf/gconf-client.h>

#include "gedit-plugins-engine.h"
#include "gedit-plugin.h"
#include "gedit-debug.h"
#include "gedit-module.h"
#include "gedit-app.h"

#define USER_GEDIT_PLUGINS_LOCATION "gedit/plugins/"

#define GEDIT_PLUGINS_ENGINE_BASE_KEY "/apps/gedit-2/plugins"
#define GEDIT_PLUGINS_ENGINE_KEY GEDIT_PLUGINS_ENGINE_BASE_KEY "/active-plugins"

#define PLUGIN_EXT	".gedit-plugin"

struct _GeditPluginInfo
{
	gchar       *file;
	
	gchar       *location;
	GTypeModule *module;

	gchar       *name;
	gchar       *desc;
	gchar       *author;
	gchar       *copyright;
	
	GeditPlugin *plugin;
	
	gboolean     active;
};

static void 		 gedit_plugins_engine_active_plugins_changed (GConfClient *client,
								      guint cnxn_id, 
								      GConfEntry *entry, 
								      gpointer user_data);

static GList *gedit_plugins_list = NULL;

static GConfClient *gedit_plugins_engine_gconf_client = NULL;

GSList *active_plugins = NULL;

static GeditPluginInfo *
gedit_plugins_engine_load (const gchar *file)
{
	GeditPluginInfo *info;
	GKeyFile *plugin_file = NULL;
	gchar *str;

	g_return_val_if_fail (file != NULL, NULL);

	gedit_debug_message (DEBUG_PLUGINS, "Loading plugin: %s", file);

	info = g_new0 (GeditPluginInfo, 1);
	info->file = g_strdup (file);

	plugin_file = g_key_file_new ();
	if (!g_key_file_load_from_file (plugin_file, file, G_KEY_FILE_NONE, NULL))
	{
		g_warning ("Bad plugin file: %s", file);
		goto error;
	}

	/* Get Location */
	str = g_key_file_get_string (plugin_file,
				     "Gedit Plugin",
				     "Module",
				     NULL);
	if (str)
		info->location = str;
	else
	{
		g_warning ("Could not find 'Module' in %s", file);
		goto error;
	}

	/* Get Name */
	str = g_key_file_get_locale_string (plugin_file,
					    "Gedit Plugin",
					    "Name",
					    NULL, NULL);
	if (str)
		info->name = str;
	else
	{
		g_warning ("Could not find 'Name' in %s", file);
		goto error;
	}

	/* Get Description */
	str = g_key_file_get_locale_string (plugin_file,
					    "Gedit Plugin",
					    "Description",
					    NULL, NULL);
	if (str)
		info->desc = str;
	else
	{
		g_warning ("Could not find 'Description' in %s", file);
		goto error;
	}

	/* Get Author */
	str = g_key_file_get_string (plugin_file,
				     "Gedit Plugin",
				     "Author",
				     NULL);
	if (str)
		info->author = str;
	else
	{
		g_warning ("Could not find 'Author' in %s", file);
		goto error;
	}

	/* Get Copyright */
	str = g_key_file_get_string (plugin_file,
				     "Gedit Plugin",
				     "Copyright",
				     NULL);
	if (str)
		info->copyright = str;
	else
	{
		g_warning ("Could not find 'Copyright' in %s", file);
		goto error;
	}

	g_key_file_free (plugin_file);
	
	return info;

error:
	g_free (info->file);
	g_free (info->location);
	g_free (info->name);
	g_free (info->desc);
	g_free (info->author);
	g_free (info->copyright);
	g_free (info);
	g_key_file_free (plugin_file);

	return NULL;
}

static void
gedit_plugins_engine_load_dir (const gchar *dir)
{
	GError *error = NULL;
	GDir *d;
	const gchar *dirent;

	g_return_if_fail (gedit_plugins_engine_gconf_client != NULL);

	gedit_debug_message (DEBUG_PLUGINS, "DIR: %s", dir);

	d = g_dir_open (dir, 0, &error);
	if (!d)
	{
		g_warning (error->message);
		g_error_free (error);
		return;
	}

	while ((dirent = g_dir_read_name (d)))
	{
		if (g_str_has_suffix (dirent, PLUGIN_EXT))
		{
			gchar *plugin_file;
			GeditPluginInfo *info;
			
			plugin_file = g_build_filename (dir, dirent, NULL);
			info = gedit_plugins_engine_load (plugin_file);
			g_free (plugin_file);

			if (info == NULL)
				continue;

			/* Actually, the plugin will be activated when reactivate_all
			 * will be called for the first time. */
			info->active = (g_slist_find_custom (active_plugins,
							     info->location,
							     (GCompareFunc)strcmp) != NULL);

			gedit_plugins_list = g_list_prepend (gedit_plugins_list, info);

			gedit_debug_message (DEBUG_PLUGINS, "Plugin %s loaded", info->name);
		}
	}

	gedit_plugins_list = g_list_reverse (gedit_plugins_list);

	g_dir_close (d);
}

static void
gedit_plugins_engine_load_all (void)
{
	gchar *pdir;

	pdir = gnome_util_home_file (USER_GEDIT_PLUGINS_LOCATION);

	/* load user's plugins */
	if (g_file_test (pdir, G_FILE_TEST_IS_DIR))
		gedit_plugins_engine_load_dir (pdir);
	
	g_free (pdir);

	/* load system plugins */
	gedit_plugins_engine_load_dir (GEDIT_PLUGINDIR "/");
}

gboolean
gedit_plugins_engine_init (void)
{
	gedit_debug (DEBUG_PLUGINS);
	
	g_return_val_if_fail (gedit_plugins_list == NULL, FALSE);
	
	if (!g_module_supported ())
	{
		g_warning ("gedit is not able to initialize the plugins engine.");
		return FALSE;
	}

	gedit_plugins_engine_gconf_client = gconf_client_get_default ();
	g_return_val_if_fail (gedit_plugins_engine_gconf_client != NULL, FALSE);

	gconf_client_add_dir (gedit_plugins_engine_gconf_client,
			      GEDIT_PLUGINS_ENGINE_BASE_KEY,
			      GCONF_CLIENT_PRELOAD_ONELEVEL,
			      NULL);

	gconf_client_notify_add (gedit_plugins_engine_gconf_client,
				 GEDIT_PLUGINS_ENGINE_KEY,
				 gedit_plugins_engine_active_plugins_changed,
				 NULL, NULL, NULL);


	active_plugins = gconf_client_get_list (gedit_plugins_engine_gconf_client,
						GEDIT_PLUGINS_ENGINE_KEY,
						GCONF_VALUE_STRING,
						NULL);

	gedit_plugins_engine_load_all ();

	return TRUE;
}

void
gedit_plugins_engine_shutdown (void)
{
	GList *pl;

	gedit_debug (DEBUG_PLUGINS);

	g_return_if_fail (gedit_plugins_engine_gconf_client != NULL);

	for (pl = gedit_plugins_list; pl; pl = pl->next)
	{
		GeditPluginInfo *info = (GeditPluginInfo*)pl->data;

		if (info->plugin != NULL)
		{
		       	gedit_debug_message (DEBUG_PLUGINS, "Unref plugin %s", info->name);

			g_object_unref (info->plugin);
			
			/* CHECK: it seems it is not possible to finalize the type module */
			/* g_return_if_fail (info->module != NULL);
			   g_object_unref (info->module); */
		}

		g_free (info->file);
		g_free (info->location);
		g_free (info->name);
		g_free (info->desc);
		g_free (info->author);
		g_free (info->copyright);
		
		g_free (info);
	}

	g_slist_foreach (active_plugins, (GFunc)g_free, NULL);
	g_slist_free (active_plugins);

	active_plugins = NULL;

	g_list_free (gedit_plugins_list);
	gedit_plugins_list = NULL;

	g_object_unref (gedit_plugins_engine_gconf_client);
	gedit_plugins_engine_gconf_client = NULL;
}

const GList *
gedit_plugins_engine_get_plugins_list (void)
{
	gedit_debug (DEBUG_PLUGINS);

	return gedit_plugins_list;
}

static gboolean
load_plugin_module (GeditPluginInfo *info)
{
	gchar *path;
	gchar *dirname;

	g_return_val_if_fail (info != NULL, FALSE);
	g_return_val_if_fail (info->file != NULL, FALSE);
	g_return_val_if_fail (info->location != NULL, FALSE);
	g_return_val_if_fail (info->plugin == NULL, FALSE);
	
	dirname = g_path_get_dirname (info->file);	
	g_return_val_if_fail (dirname != NULL, FALSE);

	path = g_module_build_path (dirname, info->location);
	g_free (dirname);
	g_return_val_if_fail (path != NULL, FALSE);
	
	info->module = G_TYPE_MODULE (gedit_module_new (path));
	g_free (path);
	
	if (g_type_module_use (info->module) == FALSE)
	{
		g_warning ("Could not load plugin file at %s\n",
			   gedit_module_get_path (GEDIT_MODULE (info->module)));
			   
		g_object_unref (G_OBJECT (info->module));
		info->module = NULL;
		
		return FALSE;
	}

	info->plugin = GEDIT_PLUGIN (gedit_module_new_object (GEDIT_MODULE (info->module)));

	g_type_module_unuse (info->module);

	return TRUE;
}

static gboolean 	 
gedit_plugins_engine_activate_plugin_real (GeditPluginInfo *info)
{
	gboolean res = TRUE;

	if (info->plugin == NULL)
		res = load_plugin_module (info);

	if (res)
	{
		const GSList *wins = gedit_app_get_windows (gedit_app_get_default ());
		while (wins != NULL)
		{
			gedit_plugin_activate (info->plugin,
					       GEDIT_WINDOW (wins->data));
			
			wins = g_slist_next (wins);
		}
	}
	else
		g_warning ("Error, impossible to activate plugin '%s'",
			   info->name);

	return res;
}

gboolean 	 
gedit_plugins_engine_activate_plugin (GeditPluginInfo *info)
{
	gedit_debug (DEBUG_PLUGINS);

	g_return_val_if_fail (info != NULL, FALSE);

	if (info->active)
		return TRUE;

	if (gedit_plugins_engine_activate_plugin_real (info))
	{
		gboolean res;
		GSList *list;
		
		/* Update plugin state */
		info->active = TRUE;

		/* I want to be really sure :) */
		list = active_plugins;
		while (list != NULL)
		{
			if (strcmp (info->location, (gchar *)list->data) == 0)
			{
				g_warning ("Plugin %s is already active.", info->location);
				return TRUE;
			}

			list = g_slist_next (list);
		}
	
		active_plugins = g_slist_insert_sorted (active_plugins, 
						        g_strdup (info->location), 
						        (GCompareFunc)strcmp);
		
		res = gconf_client_set_list (gedit_plugins_engine_gconf_client,
		    			     GEDIT_PLUGINS_ENGINE_KEY,
					     GCONF_VALUE_STRING,
					     active_plugins,
					     NULL);
		
		if (!res)
			g_warning ("Error saving the list of active plugins.");

		return TRUE;
	}

	return FALSE;
}

static void
gedit_plugins_engine_deactivate_plugin_real (GeditPluginInfo *info)
{
	const GSList *wins = gedit_app_get_windows (gedit_app_get_default ());
	
	while (wins != NULL)
	{
		gedit_plugin_deactivate (info->plugin,
					 GEDIT_WINDOW (wins->data));
			
		wins = g_slist_next (wins);
	}
}

gboolean
gedit_plugins_engine_deactivate_plugin (GeditPluginInfo *info)
{
	gboolean res;
	GSList *list;
	
	gedit_debug (DEBUG_PLUGINS);

	g_return_val_if_fail (info != NULL, FALSE);

	if (!info->active)
		return TRUE;

	gedit_plugins_engine_deactivate_plugin_real (info);

	/* Update plugin state */
	info->active = FALSE;

	list = active_plugins;
	res = (list == NULL);

	while (list != NULL)
	{
		if (strcmp (info->location, (gchar *)list->data) == 0)
		{
			g_free (list->data);
			active_plugins = g_slist_delete_link (active_plugins, list);
			list = NULL;
			res = TRUE;
		}
		else
			list = g_slist_next (list);
	}

	if (!res)
	{
		g_warning ("Plugin %s is already deactivated.", info->location);
		return TRUE;
	}

	res = gconf_client_set_list (gedit_plugins_engine_gconf_client,
	    			     GEDIT_PLUGINS_ENGINE_KEY,
				     GCONF_VALUE_STRING,
				     active_plugins,
				     NULL);
		
	if (!res)
		g_warning ("Error saving the list of active plugins.");

	return TRUE;
}

gboolean
gedit_plugins_engine_plugin_is_active (GeditPluginInfo *info)
{
	g_return_val_if_fail (info != NULL, FALSE);
	
	return info->active;
}

static void
reactivate_all (GeditWindow *window)
{
	GList *pl;

	gedit_debug (DEBUG_PLUGINS);

	for (pl = gedit_plugins_list; pl; pl = pl->next)
	{
		gboolean res = TRUE;
		
		GeditPluginInfo *info = (GeditPluginInfo*)pl->data;

		if (info->plugin == NULL)
			res = load_plugin_module (info);
			
		if (info->active && res)
		{
			gedit_plugin_activate (info->plugin,
					       window);
		}
	}
}

void
gedit_plugins_engine_update_plugins_ui (GeditWindow *window, 
					gboolean     new_window)
{
	GList *pl;

	gedit_debug (DEBUG_PLUGINS);

	g_return_if_fail (GEDIT_IS_WINDOW (window));

	if (new_window)
		reactivate_all (window);

	/* updated ui of all the plugins that implement update_ui */
	for (pl = gedit_plugins_list; pl; pl = pl->next)
	{
		GeditPluginInfo *info = (GeditPluginInfo*)pl->data;

		if (!info->active)
			continue;
			
	       	gedit_debug_message (DEBUG_PLUGINS, "Updating UI of %s", info->name);
		
		gedit_plugin_update_ui (info->plugin, window);
	}
}

gboolean
gedit_plugins_engine_plugin_is_configurable (GeditPluginInfo *info)
{
	gedit_debug (DEBUG_PLUGINS);

	g_return_val_if_fail (info != NULL, FALSE);

	if ((info->plugin == NULL) || !info->active)
		return FALSE;
	
	return gedit_plugin_is_configurable (info->plugin);
}

void 	 
gedit_plugins_engine_configure_plugin (GeditPluginInfo *info, 
				       GtkWindow       *parent)
{
	GtkWidget *conf_dlg;
	
	GtkWindowGroup *wg;
	
	gedit_debug (DEBUG_PLUGINS);

	g_return_if_fail (info != NULL);

	conf_dlg = gedit_plugin_create_configure_dialog (info->plugin);
	g_return_if_fail (conf_dlg != NULL);
	gtk_window_set_transient_for (GTK_WINDOW (conf_dlg),
				      parent);

	wg = parent->group;		      
	if (wg == NULL)
	{
		wg = gtk_window_group_new ();
		gtk_window_group_add_window (wg, parent);
	}
			
	gtk_window_group_add_window (wg,
				     GTK_WINDOW (conf_dlg));
		
	gtk_window_set_modal (GTK_WINDOW (conf_dlg), TRUE);		     
	gtk_widget_show (conf_dlg);
}

static void 
gedit_plugins_engine_active_plugins_changed (GConfClient *client,
					     guint cnxn_id,
					     GConfEntry *entry,
					     gpointer user_data)
{
	GList *pl;
	gboolean to_activate;

	gedit_debug (DEBUG_PLUGINS);

	g_return_if_fail (entry->key != NULL);
	g_return_if_fail (entry->value != NULL);

	
	if (!((entry->value->type == GCONF_VALUE_LIST) && 
	      (gconf_value_get_list_type (entry->value) == GCONF_VALUE_STRING)))
	{
		g_warning ("The gconf key '%s' mat be corrupted", GEDIT_PLUGINS_ENGINE_KEY);
		return;
	}
	
	active_plugins = gconf_client_get_list (gedit_plugins_engine_gconf_client,
						GEDIT_PLUGINS_ENGINE_KEY,
						GCONF_VALUE_STRING,
						NULL);

	for (pl = gedit_plugins_list; pl; pl = pl->next)
	{
		GeditPluginInfo *info = (GeditPluginInfo*)pl->data;

		to_activate = (g_slist_find_custom (active_plugins,
						    info->location,
						    (GCompareFunc)strcmp) != NULL);

		if (!info->active && to_activate)
		{
			/* Activate plugin */
			if (gedit_plugins_engine_activate_plugin_real (info))
				/* Update plugin state */
				info->active = TRUE;
		}
		else
		{
			if (info->active && !to_activate)
			{
				gedit_plugins_engine_deactivate_plugin_real (info);	

				/* Update plugin state */
				info->active = FALSE;
			}
		}
	}
}

const gchar *
gedit_plugins_engine_get_plugin_name (GeditPluginInfo *info)
{
	g_return_val_if_fail (info != NULL, NULL);
	
	return info->name;
}

const gchar *
gedit_plugins_engine_get_plugin_description (GeditPluginInfo *info)
{
	g_return_val_if_fail (info != NULL, NULL);
	
	return info->desc;
}
/*
const gchar	*gedit_plugins_engine_get_plugin_authors
							(GeditPluginInfo *info);
*/

const gchar *
gedit_plugins_engine_get_plugin_copyright (GeditPluginInfo *info)
{
	g_return_val_if_fail (info != NULL, NULL);
	
	return info->copyright;
}


