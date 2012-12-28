/*
 * gedit.c
 * This file is part of gedit
 *
 * Copyright (C) 2005 - Paolo Maggi 
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
 * Modified by the gedit Team, 2005. See the AUTHORS file for a 
 * list of people on the gedit Team.  
 * See the ChangeLog files for a list of changes. 
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>

#include "gedit-app.h"

#include "gedit-debug.h"
#include "gedit-dirs.h"
#include "gedit-plugins-engine.h"

#ifndef ENABLE_GVFS_METADATA
#include "gedit-metadata-manager.h"
#define METADATA_FILE "gedit-metadata.xml"
#endif

int
main (int argc, char *argv[])
{
	GeditApp *app;
	GeditPluginsEngine *engine;
	const gchar *dir;
	gint status;

#ifndef ENABLE_GVFS_METADATA
	const gchar *cache_dir;
	gchar *metadata_filename;
#endif

	/* Setup debugging */
	gedit_debug_init ();
	gedit_debug_message (DEBUG_APP, "Startup");

	/* Setup locale/gettext */
	setlocale (LC_ALL, "");

	gedit_dirs_init ();

	dir = gedit_dirs_get_gedit_locale_dir ();
	bindtextdomain (GETTEXT_PACKAGE, dir);

	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

#ifndef ENABLE_GVFS_METADATA
	/* Setup metadata-manager */
	cache_dir = gedit_dirs_get_user_cache_dir ();

	metadata_filename = g_build_filename (cache_dir, METADATA_FILE, NULL);

	gedit_metadata_manager_init (metadata_filename);

	g_free (metadata_filename);
#endif

	/* Init plugins en thegine */
	gedit_debug_message (DEBUG_APP, "Init plugins");
	engine = gedit_plugins_engine_get_default ();

	gedit_debug_message (DEBUG_APP, "Run application");
	app = gedit_app_get_default ();
	status = g_application_run (G_APPLICATION (app), argc, argv);

	/* Cleanup */
	g_object_unref (app);
	g_object_unref (engine);

	gedit_dirs_shutdown ();

#ifndef ENABLE_GVFS_METADATA
	gedit_metadata_manager_shutdown ();
#endif

	return status;
}

/* ex:set ts=8 noet: */
