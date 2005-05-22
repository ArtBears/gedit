/*
 * gedit-window.c
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

#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

#include <glib/gi18n.h>
#include <gtksourceview/gtksourcelanguage.h>
#include <gtksourceview/gtksourcelanguagesmanager.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include "gedit-ui.h"
#include "gedit-window.h"
#include "gedit-window-private.h"
#include "gedit-app.h"
#include "gedit-notebook.h"
#include "gedit-statusbar.h"
#include "gedit-utils.h"
#include "gedit-commands.h"
#include "gedit-debug.h"
#include "gedit-languages-manager.h"
#include "gedit-prefs-manager-app.h"
#include "gedit-panel.h"
#include "gedit-recent.h"
#include "gedit-documents-panel.h"
#include "gedit-search-panel.h"
#include "gedit-plugins-engine.h"

#include "recent-files/egg-recent-model.h"
#include "recent-files/egg-recent-view.h"
#include "recent-files/egg-recent-view-gtk.h"
#include "recent-files/egg-recent-view-uimanager.h"

#define GEDIT_WINDOW_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), GEDIT_TYPE_WINDOW, GeditWindowPrivate))

/* Signals */
enum
{
	TAB_ADDED,
	TAB_REMOVED,
	TABS_REORDERED,
	ACTIVE_TAB_CHANGED,
	ACTIVE_TAB_STATE_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

enum
{
	TARGET_URI_LIST = 100
};

static const GtkTargetEntry drag_types[] =
{
	{ "text/uri-list", 0, TARGET_URI_LIST },
};

G_DEFINE_TYPE(GeditWindow, gedit_window, GTK_TYPE_WINDOW)

static void
gedit_window_finalize (GObject *object)
{
	GeditWindow *window = GEDIT_WINDOW (object); 
	
	g_object_unref (window->priv->window_group);
		
	G_OBJECT_CLASS (gedit_window_parent_class)->finalize (object);
}

static void
gedit_window_destroy (GtkObject *object)
{
	GeditWindow *window;
	
	window = GEDIT_WINDOW (object);

	if (gedit_prefs_manager_window_height_can_set ())
		gedit_prefs_manager_set_window_height (window->priv->height);

	if (gedit_prefs_manager_window_width_can_set ())
		gedit_prefs_manager_set_window_width (window->priv->width);

	if (gedit_prefs_manager_window_state_can_set ())
		gedit_prefs_manager_set_window_state (window->priv->state);
		
	if ((window->priv->side_panel_size > 0) &&
		gedit_prefs_manager_side_panel_size_can_set ())
			gedit_prefs_manager_set_side_panel_size	(
					window->priv->side_panel_size);
	
	if ((window->priv->bottom_panel_size > 0) && 
		gedit_prefs_manager_bottom_panel_size_can_set ())
			gedit_prefs_manager_set_bottom_panel_size (
					window->priv->bottom_panel_size);
										
	GTK_OBJECT_CLASS (gedit_window_parent_class)->destroy (object);
}

static gboolean
window_state_event (GtkWidget           *widget,
		    GdkEventWindowState *event)
{
	GeditWindow *window = GEDIT_WINDOW (widget);

	window->priv->state = event->new_window_state;

	if (event->changed_mask &
	    (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN))
	{
		gboolean show;

		show = !(event->new_window_state &
		         (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN));

		_gedit_statusbar_set_has_resize_grip (GEDIT_STATUSBAR (window->priv->statusbar),
						      show);
	}

	return FALSE;
}

static gboolean 
configure_event (GtkWidget         *widget,
		 GdkEventConfigure *event)
{
	GeditWindow *window = GEDIT_WINDOW (widget);

	window->priv->width = event->width;
	window->priv->height = event->height;

	return GTK_WIDGET_CLASS (gedit_window_parent_class)->configure_event (widget, event);
}

static void
gedit_window_class_init (GeditWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkObjectClass *gobject_class = GTK_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize = gedit_window_finalize;
	gobject_class->destroy = gedit_window_destroy;

	widget_class->window_state_event = window_state_event;
	widget_class->configure_event = configure_event;

	signals[TAB_ADDED] =
		g_signal_new ("tab_added",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GeditWindowClass, tab_added),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      GEDIT_TYPE_TAB);
	signals[TAB_REMOVED] =
		g_signal_new ("tab_removed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GeditWindowClass, tab_removed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      GEDIT_TYPE_TAB);
	signals[TABS_REORDERED] =
		g_signal_new ("tabs_reordered",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GeditWindowClass, tabs_reordered),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
	signals[ACTIVE_TAB_CHANGED] =
		g_signal_new ("active_tab_changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GeditWindowClass, active_tab_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      GEDIT_TYPE_TAB);
	signals[ACTIVE_TAB_STATE_CHANGED] =
		g_signal_new ("active_tab_state_changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GeditWindowClass, active_tab_state_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);			      			      

	g_type_class_add_private (object_class, sizeof(GeditWindowPrivate));
}

static void
menu_item_select_cb (GtkMenuItem *proxy,
                     GeditWindow *window)
{
	GtkAction *action;
	char *message;

	action = g_object_get_data (G_OBJECT (proxy),  "gtk-action");
	g_return_if_fail (action != NULL);

	g_object_get (G_OBJECT (action), "tooltip", &message, NULL);
	if (message)
	{
		gtk_statusbar_push (GTK_STATUSBAR (window->priv->statusbar),
				    window->priv->tip_message_cid, message);
		g_free (message);
	}
}

static void
menu_item_deselect_cb (GtkMenuItem *proxy,
                       GeditWindow *window)
{
	gtk_statusbar_pop (GTK_STATUSBAR (window->priv->statusbar),
			   window->priv->tip_message_cid);
}

static void
connect_proxy_cb (GtkUIManager *manager,
                  GtkAction *action,
                  GtkWidget *proxy,
                  GeditWindow *window)
{
	if (GTK_IS_MENU_ITEM (proxy))
	{
		g_signal_connect (proxy, "select",
				  G_CALLBACK (menu_item_select_cb), window);
		g_signal_connect (proxy, "deselect",
				  G_CALLBACK (menu_item_deselect_cb), window);
	}
}

static void
disconnect_proxy_cb (GtkUIManager *manager,
                     GtkAction *action,
                     GtkWidget *proxy,
                     GeditWindow *window)
{
	if (GTK_IS_MENU_ITEM (proxy))
	{
		g_signal_handlers_disconnect_by_func
			(proxy, G_CALLBACK (menu_item_select_cb), window);
		g_signal_handlers_disconnect_by_func
			(proxy, G_CALLBACK (menu_item_deselect_cb), window);
	}
}

/* Returns TRUE if toolbar is visible */
static gboolean
set_toolbar_style (GeditWindow *window,
		   GeditWindow *origin)
{
	gboolean visible;
	GeditToolbarSetting style;
	GtkAction *action;
	
	if (origin == NULL)
		visible = gedit_prefs_manager_get_toolbar_visible ();
	else
		visible = GTK_WIDGET_VISIBLE (origin->priv->toolbar);
	
	/* Set visibility */
	if (visible)
		gtk_widget_show (window->priv->toolbar);
	else
	{
		gtk_widget_hide (window->priv->toolbar);
	}
	
	action = gtk_action_group_get_action (window->priv->action_group,
					      "ViewToolbar");
					      
	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)) != visible)
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), visible);
	
	/* Set style */
	if (origin == NULL)
		style = gedit_prefs_manager_get_toolbar_buttons_style ();
	else
		style = origin->priv->toolbar_style;
		
	window->priv->toolbar_style = style;
		
	switch (style)
	{
		case GEDIT_TOOLBAR_SYSTEM:
			gedit_debug_message (DEBUG_MDI, "GEDIT: SYSTEM");
			gtk_toolbar_unset_style (
					GTK_TOOLBAR (window->priv->toolbar));
			break;
			
		case GEDIT_TOOLBAR_ICONS:
			gedit_debug_message (DEBUG_MDI, "GEDIT: ICONS");
			gtk_toolbar_set_style (
					GTK_TOOLBAR (window->priv->toolbar),
					GTK_TOOLBAR_ICONS);
			break;
			
		case GEDIT_TOOLBAR_ICONS_AND_TEXT:
			gedit_debug_message (DEBUG_MDI, "GEDIT: ICONS_AND_TEXT");
			gtk_toolbar_set_style (
					GTK_TOOLBAR (window->priv->toolbar),
					GTK_TOOLBAR_BOTH);			
			break;
			
		case GEDIT_TOOLBAR_ICONS_BOTH_HORIZ:
			gedit_debug_message (DEBUG_MDI, "GEDIT: ICONS_BOTH_HORIZ");
			gtk_toolbar_set_style (
					GTK_TOOLBAR (window->priv->toolbar),
					GTK_TOOLBAR_BOTH_HORIZ);	
			break;       
	}
	
	return visible;
}

static void
set_sensitivity_according_to_tab (GeditWindow *window,
				  GeditTab    *tab)
{
	GeditDocument *doc;
	GeditView     *view;
	GtkAction     *action;
	gboolean       b;
	gboolean       state_normal;
	GeditTabState  state;
	
	g_return_if_fail (GEDIT_TAB (tab));
		
	gedit_debug (DEBUG_MDI);
		
	state = gedit_tab_get_state (tab);
		
	state_normal = (state == GEDIT_TAB_STATE_NORMAL);
							
	view = gedit_tab_get_view (tab);
	doc = GEDIT_DOCUMENT (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
	
	action = gtk_action_group_get_action (window->priv->action_group,
					      "FileSave");
	gtk_action_set_sensitive (action,
				  state_normal ||
				  (state == GEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW));					      

	action = gtk_action_group_get_action (window->priv->action_group,
					      "FileSaveAs");
	gtk_action_set_sensitive (action,
				  state_normal ||
				  (state == GEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW));
				  	
	action = gtk_action_group_get_action (window->priv->action_group,
					      "FileRevert");
	gtk_action_set_sensitive (action, 
				  !gedit_document_is_untitled (doc) &&
				  state_normal);

	action = gtk_action_group_get_action (window->priv->action_group,
					      "FilePrintPreview");
	gtk_action_set_sensitive (action,
				  state_normal);

	action = gtk_action_group_get_action (window->priv->action_group,
					      "FilePrint");
	gtk_action_set_sensitive (action,
				  state_normal ||
				  (state == GEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW));
				  
	action = gtk_action_group_get_action (window->priv->action_group,
					      "FileClose");

	gtk_action_set_sensitive (action,
				  (state != GEDIT_TAB_STATE_PRINTING) &&
				  (state != GEDIT_TAB_STATE_LOADING)  &&
				  (state != GEDIT_TAB_STATE_PRINTING) &&
				  (state != GEDIT_TAB_STATE_PRINT_PREVIEWING));

	action = gtk_action_group_get_action (window->priv->action_group,
					      "EditUndo");
	gtk_action_set_sensitive (action, 
				  state_normal &&
				  gtk_source_buffer_can_undo (GTK_SOURCE_BUFFER (doc)));

	action = gtk_action_group_get_action (window->priv->action_group,
					      "EditRedo");
	gtk_action_set_sensitive (action, 
				  state_normal &&
				  gtk_source_buffer_can_redo (GTK_SOURCE_BUFFER (doc)));

	action = gtk_action_group_get_action (window->priv->action_group,
					      "EditCut");
	gtk_action_set_sensitive (action,
				  state_normal);
				  
	action = gtk_action_group_get_action (window->priv->action_group,
					      "EditCopy");
	gtk_action_set_sensitive (action,
				  state_normal);
				  
	action = gtk_action_group_get_action (window->priv->action_group,
					      "EditPaste");
	gtk_action_set_sensitive (action,
				  state_normal);

	action = gtk_action_group_get_action (window->priv->action_group,
					      "EditDelete");
	gtk_action_set_sensitive (action,
				  state_normal);

	action = gtk_action_group_get_action (window->priv->action_group,
					      "SearchFind");
	gtk_action_set_sensitive (action,
				  state_normal);

	action = gtk_action_group_get_action (window->priv->action_group,
					      "SearchReplace");
	gtk_action_set_sensitive (action,
				  state_normal);

	b = _gedit_document_can_find_again (doc);
	action = gtk_action_group_get_action (window->priv->action_group,
					      "SearchFindNext");
	gtk_action_set_sensitive (action, state_normal && b);

	action = gtk_action_group_get_action (window->priv->action_group,
					      "SearchFindPrevious");
	gtk_action_set_sensitive (action, state_normal && b);

	action = gtk_action_group_get_action (window->priv->action_group,
					      "SearchGoToLine");
	gtk_action_set_sensitive (action, state_normal);
	
	action = gtk_action_group_get_action (window->priv->action_group,
					      "ViewHighlightMode");
	gtk_action_set_sensitive (action, 
				  gedit_prefs_manager_get_enable_syntax_highlighting ());

	gedit_plugins_engine_update_plugins_ui (window, FALSE);
}

static void
language_toggled (GtkToggleAction *action,
		  GeditWindow     *window)
{
	GeditDocument *doc;
	const GSList *languages;
	const GtkSourceLanguage *lang;
	gint n;

	if (gtk_toggle_action_get_active (action) == FALSE)
		return;

	doc = gedit_window_get_active_document (window);
	if (doc == NULL)
		return;

	n = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (action));

	if (n < 0)
	{
		/* Normal (no highlighting) */
		lang = NULL;
	}
	else
	{
		languages = gtk_source_languages_manager_get_available_languages (
						gedit_get_languages_manager ());

		lang = GTK_SOURCE_LANGUAGE (g_slist_nth_data ((GSList *) languages, n));
	}

	gedit_document_set_language (doc, (GtkSourceLanguage *) lang);
}

static void
create_language_menu_item (GtkSourceLanguage *lang,
			   gint               index,
			   guint              ui_id,
			   GeditWindow       *window)
{
	GtkAction *section_action;
	GtkRadioAction *action;
	GtkAction *normal_action;
	GSList *group;
	gchar *section;
	gchar *lang_name;
	gchar *tip;
	gchar *path;

	section = gtk_source_language_get_section (lang);

	/* check if the section submenu exists or create it */
	section_action = gtk_action_group_get_action (window->priv->languages_action_group,
						      section);

	if (section_action == NULL)
	{
		// CHECK: escaping strings
		section_action = gtk_action_new (section,
						 section,
						 NULL,
						 NULL);

		gtk_action_group_add_action (window->priv->languages_action_group,
					     section_action);
		g_object_unref (section_action);

		gtk_ui_manager_add_ui (window->priv->manager,
				       ui_id,
				       "/MenuBar/ViewMenu/ViewHighlightModeMenu/LanguagesMenuPlaceholder",
				       section, section,
				       GTK_UI_MANAGER_MENU,
				       FALSE);
	}

	/* now add the language item to the section */
	lang_name = gtk_source_language_get_name (lang);
	tip = g_strdup_printf (_("Use %s highlight mode"), lang_name);
	path = g_strdup_printf ("/MenuBar/ViewMenu/ViewHighlightModeMenu/LanguagesMenuPlaceholder/%s",
				section);

	// CHECK: escaping strings
	action = gtk_radio_action_new (lang_name,
				       lang_name,
				       tip,
				       NULL,
				       index);

	gtk_action_group_add_action (window->priv->languages_action_group,
				     GTK_ACTION (action));
	g_object_unref (action);

	/* add the action to the same radio group of the "Normal" action */
	normal_action = gtk_action_group_get_action (window->priv->languages_action_group,
						     "LangNormal");
	group = gtk_radio_action_get_group (GTK_RADIO_ACTION (normal_action));
	gtk_radio_action_set_group (action, group);

	g_signal_connect (action,
			  "activate",
			  G_CALLBACK (language_toggled),
			  window);

	gtk_ui_manager_add_ui (window->priv->manager,
			       ui_id,
			       path,
			       lang_name, lang_name,
			       GTK_UI_MANAGER_MENUITEM,
			       FALSE);

	g_free (path);
	g_free (tip);
	g_free (lang_name);
	g_free (section);
}

static void
create_languages_menu (GeditWindow *window)
{
	GtkRadioAction *action_normal;
	const GSList *languages;
	const GSList *l;
	guint id;
	gint i;

	/* add the "Normal" item before all the others */
	action_normal = gtk_radio_action_new ("LangNormal",
					      _("Normal"),
					      _("Use Normal highlight mode"),
					      NULL,
					      -1);

	gtk_action_group_add_action (window->priv->languages_action_group,
				     GTK_ACTION (action_normal));
	g_object_unref (action_normal);

	g_signal_connect (action_normal,
			  "activate",
			  G_CALLBACK (language_toggled),
			  window);

	id = gtk_ui_manager_new_merge_id (window->priv->manager);

	gtk_ui_manager_add_ui (window->priv->manager,
			       id,
			       "/MenuBar/ViewMenu/ViewHighlightModeMenu/LanguagesMenuPlaceholder",
			       "LangNormal", "LangNormal",
			       GTK_UI_MANAGER_MENUITEM,
			       TRUE);

	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action_normal), TRUE);

	/* now add all the known languages */
	languages = gtk_source_languages_manager_get_available_languages (
						gedit_get_languages_manager ());

	i = 0;
	for (l = languages; l != NULL; l = l->next)
	{
		create_language_menu_item (GTK_SOURCE_LANGUAGE (l->data),
					    i,
					    id,
					    window);

		i++;
	}
}

static void
open_recent_gtk (EggRecentViewGtk *view, 
		 EggRecentItem    *item,
		 GeditWindow      *window)
{
	gedit_cmd_file_open_recent (item, window);
}

static void
open_recent_uim (GtkAction   *action, 
		 GeditWindow *window)
{
	EggRecentItem *item;

	item = egg_recent_view_uimanager_get_item (window->priv->recent_view_uim,
						   action);
	g_return_if_fail (item != NULL);

	gedit_cmd_file_open_recent (item, window);
}

static void
recent_tooltip_func_gtk (GtkTooltips   *tooltips,
			 GtkWidget     *menu,
			 EggRecentItem *item,
			 gpointer       user_data)
{
	gchar *tip;
	gchar *uri_for_display;

	uri_for_display = egg_recent_item_get_uri_for_display (item);
	g_return_if_fail (uri_for_display != NULL);

	/* Translators: %s is a URI */
	tip = g_strdup_printf (_("Open '%s'"), uri_for_display);

	g_free (uri_for_display);

	gtk_tooltips_set_tip (tooltips, GTK_WIDGET (menu), tip, NULL);

	g_free (tip);
}

static char *
recent_tooltip_func_uim (EggRecentItem *item,
			 gpointer       user_data)
{
	gchar *tip;
	gchar *uri_for_display;

	uri_for_display = egg_recent_item_get_uri_for_display (item);
	g_return_val_if_fail (uri_for_display != NULL, NULL);

	/* Translators: %s is a URI */
	tip = g_strdup_printf (_("Open '%s'"), uri_for_display);

	g_free (uri_for_display);

	return tip;
}

static void
build_recent_tool_menu (GtkMenuToolButton *button,
			GeditWindow       *window)
{
	EggRecentViewGtk *view;
	EggRecentModel *model;

	model = gedit_recent_get_model ();
	view = egg_recent_view_gtk_new (window->priv->toolbar_recent_menu,
					NULL);

	egg_recent_view_gtk_show_icons (view, TRUE);
	egg_recent_view_gtk_show_numbers (view, FALSE);
	egg_recent_view_gtk_set_tooltip_func (view, recent_tooltip_func_gtk, NULL);

	egg_recent_view_set_model (EGG_RECENT_VIEW (view), model);

	g_signal_connect (view,
			  "activate",
			  G_CALLBACK (open_recent_gtk),
			  window);

	gtk_widget_show (window->priv->toolbar_recent_menu);

	/* this callback must run just once for lazy initialization:
	 * we can now disconnect it
	 */
	g_signal_handlers_disconnect_by_func (button,
					      G_CALLBACK (build_recent_tool_menu),
					      window);
}

static void
set_non_homogeneus (GtkWidget *widget, gpointer data)
{
	gtk_tool_item_set_homogeneous (GTK_TOOL_ITEM (widget), FALSE);
}
                                             
static void
create_menu_bar_and_toolbar (GeditWindow *window, 
			     GtkWidget   *main_box)
{
	GtkActionGroup *action_group;
	GtkAction *action;
	GtkUIManager *manager;
	GtkWidget *menubar;
	EggRecentModel *recent_model;
	EggRecentViewUIManager *recent_view;
	GtkToolItem *open_button;
	GError *error = NULL;

	manager = gtk_ui_manager_new ();
	window->priv->manager = manager;

	gtk_window_add_accel_group (GTK_WINDOW (window),
				    gtk_ui_manager_get_accel_group (manager));

	/* now load the UI definition */
	gtk_ui_manager_add_ui_from_file (manager, /* GEDIT_UI_DIR */ "gedit-ui.xml", &error);
	if (error != NULL)
	{
		g_warning ("Could not merge gedit-ui.xml: %s", error->message);
		g_error_free (error);
	}

	/* show tooltips in the statusbar */
	g_signal_connect (manager, "connect_proxy",
			  G_CALLBACK (connect_proxy_cb), window);
	g_signal_connect (manager, "disconnect_proxy",
			 G_CALLBACK (disconnect_proxy_cb), window);

	action_group = gtk_action_group_new ("GeditWindowAlwaysSensitiveActions");
	gtk_action_group_set_translation_domain (action_group, NULL);
	gtk_action_group_add_actions (action_group,
				      gedit_always_sensitive_menu_entries,
				      G_N_ELEMENTS (gedit_always_sensitive_menu_entries),
				      window);

	gtk_ui_manager_insert_action_group (manager, action_group, 0);
	window->priv->always_sensitive_action_group = action_group;

	action_group = gtk_action_group_new ("GeditWindowActions");
	gtk_action_group_set_translation_domain (action_group, NULL);
	gtk_action_group_add_actions (action_group,
				      gedit_menu_entries,
				      G_N_ELEMENTS (gedit_menu_entries),
				      window);
	gtk_action_group_add_toggle_actions (action_group,
					     gedit_toggle_menu_entries,
					     G_N_ELEMENTS (gedit_toggle_menu_entries),
					     window);

	gtk_ui_manager_insert_action_group (manager, action_group, 0);
	window->priv->action_group = action_group;

	/* set short labels to use in the toolbar */
	action = gtk_action_group_get_action (action_group, "FileSave");
	g_object_set (action, "short_label", _("Save"), NULL);
	action = gtk_action_group_get_action (action_group, "SearchFind");
	g_object_set (action, "short_label", _("Find"), NULL);
	action = gtk_action_group_get_action (action_group, "SearchReplace");
	g_object_set (action, "short_label", _("Replace"), NULL);

	/* set which actions should have priority on the toolbar */
	action = gtk_action_group_get_action (action_group, "FileSave");
	g_object_set (action, "is_important", TRUE, NULL);
	action = gtk_action_group_get_action (action_group, "EditUndo");
	g_object_set (action, "is_important", TRUE, NULL);

	/* recent files menu */
	recent_model = gedit_recent_get_model ();
	recent_view = egg_recent_view_uimanager_new (manager,
						     "/MenuBar/FileMenu/FileRecentsPlaceholder",
						     G_CALLBACK (open_recent_uim),
						     window);
	window->priv->recent_view_uim = recent_view;
	egg_recent_view_uimanager_show_icons (recent_view, FALSE);
	egg_recent_view_uimanager_set_tooltip_func (recent_view,
						    recent_tooltip_func_uim,
						    window);
	egg_recent_view_set_model (EGG_RECENT_VIEW (recent_view), recent_model);

	/* languages menu */
	action_group = gtk_action_group_new ("LanguagesActions");
	gtk_action_group_set_translation_domain (action_group, NULL);
	window->priv->languages_action_group = action_group;
	gtk_ui_manager_insert_action_group (manager, action_group, 0);
	create_languages_menu (window);

	/* list of open documents menu */
	action_group = gtk_action_group_new ("DocumentsListActions");
	gtk_action_group_set_translation_domain (action_group, NULL);
	window->priv->documents_list_action_group = action_group;
	gtk_ui_manager_insert_action_group (manager, action_group, 0);

	menubar = gtk_ui_manager_get_widget (manager, "/MenuBar");
	gtk_box_pack_start (GTK_BOX (main_box), menubar, FALSE, FALSE, 0);

	window->priv->toolbar = gtk_ui_manager_get_widget (manager, "/ToolBar");
	gtk_box_pack_start (GTK_BOX (main_box),
			    window->priv->toolbar,
			    FALSE,
			    FALSE,
			    0);

	/* add the custom Open button to the toolbar */
	open_button = gtk_menu_tool_button_new_from_stock (GTK_STOCK_OPEN);
//	gtk_tool_item_set_homogeneous (open_button, TRUE);

	/* the popup menu is actually built the first time it's showed */
	window->priv->toolbar_recent_menu = gtk_menu_new ();
	gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (open_button),
				       window->priv->toolbar_recent_menu);
	g_signal_connect (open_button, "show-menu",
			  G_CALLBACK (build_recent_tool_menu), window);

	// CHECK: not very nice the way we access the tooltops object
	// but I can't see a better way and I don't want a differen GtkTooltip
	// just for this tool button.
	gtk_tool_item_set_tooltip (open_button,
				   GTK_TOOLBAR (window->priv->toolbar)->tooltips,
				   _("Open a file"),
				   NULL);
	gtk_menu_tool_button_set_arrow_tooltip (GTK_MENU_TOOL_BUTTON (open_button),
						GTK_TOOLBAR (window->priv->toolbar)->tooltips,
						_("Open a recently used file"),
						NULL);

	action = gtk_action_group_get_action (window->priv->always_sensitive_action_group,
					      "FileOpen");
	g_object_set (action,
		      "is_important", TRUE,
		      "short_label", _("Open"),
		      NULL);
	gtk_action_connect_proxy (action, GTK_WIDGET (open_button));

	gtk_toolbar_insert (GTK_TOOLBAR (window->priv->toolbar), open_button, 1);
	
	set_toolbar_style (window, NULL);

	gtk_container_foreach (GTK_CONTAINER (window->priv->toolbar),
			       (GtkCallback)set_non_homogeneus,
			       NULL);
}

static void
documents_list_menu_activate (GtkToggleAction *action,
			      GeditWindow     *window)
{
	gint n;

	if (gtk_toggle_action_get_active (action) == FALSE)
		return;

	n = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (action));
	gtk_notebook_set_current_page (GTK_NOTEBOOK (window->priv->notebook), n);
}

static void
update_documents_list_menu (GeditWindow *window)
{
	GeditWindowPrivate *p = window->priv;
	GList *actions, *l;
	gint n, i;
	guint id;
	GSList *group = NULL;

	g_return_if_fail (p->documents_list_action_group != NULL);

	if (p->documents_list_menu_ui_id != 0)
		gtk_ui_manager_remove_ui (p->manager,
					  p->documents_list_menu_ui_id);

	actions = gtk_action_group_list_actions (p->documents_list_action_group);
	for (l = actions; l != NULL; l = l->next)
	{
		g_signal_handlers_disconnect_by_func (GTK_ACTION (l->data),
						      G_CALLBACK (documents_list_menu_activate),
						      window);
 		gtk_action_group_remove_action (p->documents_list_action_group,
						GTK_ACTION (l->data));
	}
	g_list_free (actions);

	n = gtk_notebook_get_n_pages (GTK_NOTEBOOK (p->notebook));

	id = (n > 0) ? gtk_ui_manager_new_merge_id (p->manager) : 0;

	for (i = 0; i < n; i++)
	{
		GtkWidget *tab;
		GtkRadioAction *action;
		gchar *action_name;
		gchar *tab_name; //CHECK: must escape underscores and gmarkup
		gchar *tip;	 // ditto as above
		gchar *accel;

		tab = gtk_notebook_get_nth_page (GTK_NOTEBOOK (p->notebook), i);

		/* NOTE: the action is associated to the position of the tab in
		 * the notebook not to the tab itself! This is needed to work
		 * around the gtk+ bug #170727: gtk leaves around the accels
		 * of the action. Since the accel depends on the tab position
		 * the problem is worked around, action with the same name always
		 * get the same accel.
		 */
		action_name = g_strdup_printf ("Tab_%d", i);
		tab_name = _gedit_tab_get_name (GEDIT_TAB (tab));
		tip =  g_strdup_printf (_("Activate %s"), tab_name);

		/* alt + 1, 2, 3... 0 to switch to the first ten tabs */
		accel = (i < 10) ? g_strdup_printf ("<alt>%d", (i + 1) % 10) : NULL;

		action = gtk_radio_action_new (action_name,
					       tab_name,
					       tip,
					       NULL,
					       i);

		if (group != NULL)
			gtk_radio_action_set_group (action, group);

		/* note that group changes each time we add an action, so it must be updated */
		group = gtk_radio_action_get_group (action);

		gtk_action_group_add_action_with_accel (p->documents_list_action_group,
							GTK_ACTION (action),
							accel);

		g_signal_connect (action,
				  "activate",
				  G_CALLBACK (documents_list_menu_activate),
				  window);

		gtk_ui_manager_add_ui (p->manager,
				       id,
				       "/MenuBar/DocumentsMenu/DocumentsListPlaceholder",
				       action_name, action_name,
				       GTK_UI_MANAGER_MENUITEM,
				       FALSE);

		if (GEDIT_TAB (tab) == p->active_tab)
			gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

		g_object_unref (action);

		g_free (action_name);
		g_free (tab_name);
		g_free (tip);
		g_free (accel);
	}

	p->documents_list_menu_ui_id = id;
}

/* Returns TRUE if status bar is visible */
static gboolean
set_statusbar_style (GeditWindow *window,
		     GeditWindow *origin)
{
	GtkAction *action;
	
	gboolean visible;

	if (origin == NULL)
		visible = gedit_prefs_manager_get_statusbar_visible ();
	else
		visible = GTK_WIDGET_VISIBLE (origin->priv->statusbar);

	if (visible)
		gtk_widget_show (window->priv->statusbar);
	else
		gtk_widget_hide (window->priv->statusbar);

	action = gtk_action_group_get_action (window->priv->action_group,
					      "ViewStatusbar");
					      
	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)) != visible)
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), visible);
		
	return visible;
}

static void
create_statusbar (GeditWindow *window, 
		  GtkWidget   *main_box)
{
	window->priv->statusbar = gedit_statusbar_new ();

	window->priv->generic_message_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR (window->priv->statusbar), "generic_message");
	window->priv->tip_message_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR (window->priv->statusbar), "tip_message");

	gtk_box_pack_end (GTK_BOX (main_box),
			  window->priv->statusbar,
			  FALSE, 
			  TRUE, 
			  0);	
			  
	set_statusbar_style (window, NULL);		
}

static GeditWindow *
clone_window (GeditWindow *origin)
{
	GtkWindow *window;
	GeditApp  *app;
	
	app = gedit_app_get_default ();
	
	window = GTK_WINDOW (gedit_app_create_window (app));
	
	gtk_window_set_default_size (window, 
				     origin->priv->width,
				     origin->priv->height);
				     
	if ((origin->priv->state & GDK_WINDOW_STATE_MAXIMIZED) != 0)
	{
		gtk_window_set_default_size (window, 
					     gedit_prefs_manager_get_default_window_width (),
					     gedit_prefs_manager_get_default_window_height ());
					     
		gtk_window_maximize (window);
	}
	else
	{
		gtk_window_set_default_size (window, 
				     origin->priv->width,
				     origin->priv->height);

		gtk_window_unmaximize (window);
	}		

	if ((origin->priv->state & GDK_WINDOW_STATE_STICKY ) != 0)
		gtk_window_stick (window);
	else
		gtk_window_unstick (window);

	gtk_paned_set_position (GTK_PANED (GEDIT_WINDOW (window)->priv->hpaned),
				gtk_paned_get_position (GTK_PANED (origin->priv->hpaned)));

	gtk_paned_set_position (GTK_PANED (GEDIT_WINDOW (window)->priv->vpaned),
				gtk_paned_get_position (GTK_PANED (origin->priv->vpaned)));
				
	if (GTK_WIDGET_VISIBLE (origin->priv->side_panel))
		gtk_widget_show (GEDIT_WINDOW (window)->priv->side_panel);
	else
		gtk_widget_hide (GEDIT_WINDOW (window)->priv->side_panel);

	if (GTK_WIDGET_VISIBLE (origin->priv->bottom_panel))
		gtk_widget_show (GEDIT_WINDOW (window)->priv->bottom_panel);
	else
		gtk_widget_hide (GEDIT_WINDOW (window)->priv->bottom_panel);
		
	set_statusbar_style (GEDIT_WINDOW (window), origin);
	set_toolbar_style (GEDIT_WINDOW (window), origin);

	return GEDIT_WINDOW (window);
}

static void
update_cursor_position_statusbar (GtkTextBuffer *buffer, 
				  GeditWindow   *window)
{
	gint row, col;
	GtkTextIter iter;
	GtkTextIter start;
	guint tab_size;
	GeditView *view;

	gedit_debug (DEBUG_MDI);
  
 	if (buffer != GTK_TEXT_BUFFER (gedit_window_get_active_document (window)))
 		return;
 		
 	view = gedit_window_get_active_view (window);
 	
	gtk_text_buffer_get_iter_at_mark (buffer,
					  &iter,
					  gtk_text_buffer_get_insert (buffer));
	
	row = gtk_text_iter_get_line (&iter);
	
	start = iter;
	gtk_text_iter_set_line_offset (&start, 0);
	col = 0;

	tab_size = gtk_source_view_get_tabs_width (GTK_SOURCE_VIEW (view));

	while (!gtk_text_iter_equal (&start, &iter))
	{
		/* FIXME: Are we Unicode compliant here? */
		if (gtk_text_iter_get_char (&start) == '\t')
					
			col += (tab_size - (col  % tab_size));
		else
			++col;

		gtk_text_iter_forward_char (&start);
	}
	
	gedit_statusbar_set_cursor_position (
				GEDIT_STATUSBAR (window->priv->statusbar),
				row + 1,
				col + 1);
}

static void
cursor_moved (GtkTextBuffer     *buffer,
	      const GtkTextIter *new_location,
	      GtkTextMark       *mark,
	      GeditWindow       *window)
{
	if (buffer != GTK_TEXT_BUFFER (gedit_window_get_active_document (window)))
		return;
			  
	if (mark == gtk_text_buffer_get_insert (buffer))
		update_cursor_position_statusbar (buffer, window);
}

static void
update_overwrite_mode_statusbar (GtkTextView *view, 
				 GeditWindow *window)
{
	if (view != GTK_TEXT_VIEW (gedit_window_get_active_view (window)))
		return;
		
	/* Note that we have to use !gtk_text_view_get_overwrite since we
	   are in the in the signal handler of "toggle overwrite" that is
	   G_SIGNAL_RUN_LAST
	*/
	gedit_statusbar_set_overwrite (
			GEDIT_STATUSBAR (window->priv->statusbar),
			!gtk_text_view_get_overwrite (view));
}

#define MAX_TITLE_LENGTH 100

static void 
set_title (GeditWindow *window)
{
	GeditDocument *doc = NULL;
	const gchar *short_name;
	gchar *name;
	gchar *dirname = NULL;
	gchar *title = NULL;
	gint len;

	if (window->priv->active_tab == NULL)
	{
		gtk_window_set_title (GTK_WINDOW (window), "gedit");
		return;
	}

	doc = gedit_tab_get_document (window->priv->active_tab);
	g_return_if_fail (doc != NULL);

	short_name = gedit_document_get_short_name_for_display (doc);

	len = g_utf8_strlen (short_name, -1);

	/* if the name is awfully long, truncate it and be done with it,
	 * otherwise also show the directory (ellipsized if needed)
	 */
	if (len > MAX_TITLE_LENGTH)
	{
		name = gedit_utils_str_middle_truncate (short_name, 
							MAX_TITLE_LENGTH);
	}
	else
	{
		const gchar *uri;
		gchar *str;

		name = g_strdup (short_name);

		uri = gedit_document_get_uri_for_display (doc);
		str = gedit_utils_uri_get_dirname (uri);

		if (str != NULL)
		{
			/* use the remaining space for the dir, but use a min of 20 chars
			 * so that we do not end up with a dirname like "(a...b)".
			 * This means that in the worst case when the filename is long 99
			 * we have a title long 99 + 20, but I think it's a rare enough
			 * case to be acceptable. It's justa darn title afterall :)
			 */
			dirname = gedit_utils_str_middle_truncate (str, 
								   MAX (20, MAX_TITLE_LENGTH - len));
			g_free (str);
		}
	}

	if (gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (doc)))
	{
		if (dirname != NULL)
			title = g_strdup_printf ("*%s (%s) - gedit", 
						 name, 
						 dirname);
		else
			title = g_strdup_printf ("*%s - gedit", 
						 name);
	} 
	else 
	{
		if (gedit_document_is_readonly (doc)) 
		{
			if (dirname != NULL)
				title = g_strdup_printf ("%s [%s] (%s) - gedit", 
							 name, 
							 _("Read Only"), 
							 dirname);
			else
				title = g_strdup_printf ("%s [%s] - gedit", 
							 name, 
							 _("Read Only"));
		} 
		else 
		{
			if (dirname != NULL)
				title = g_strdup_printf ("%s (%s) - gedit", 
							 name, 
							 dirname);
			else
				title = g_strdup_printf ("%s - gedit", 
							 name);
		}
	}

	gtk_window_set_title (GTK_WINDOW (window), title);

	g_free (dirname);
	g_free (name);
	g_free (title);
}

#undef MAX_TITLE_LENGTH

static void 
notebook_switch_page (GtkNotebook     *book, 
		      GtkNotebookPage *pg,
		      gint             page_num, 
		      GeditWindow     *window)
{
	GeditView *view;
	GeditTab *tab;
	GtkAction *action;
	gchar *action_name;

	/* CHECK: I don't know why but it seems notebook_switch_page is called
	two times every time the user change the active tab */
	
	tab = GEDIT_TAB (gtk_notebook_get_nth_page (book, page_num));
	if (tab == window->priv->active_tab)
		return;

	/* set the active tab */		
	window->priv->active_tab = tab;

	set_title (window);
	set_sensitivity_according_to_tab (window, tab);

	/* activate the right item in the documents menu */
	action_name = g_strdup_printf ("Tab_%d", page_num);
	action = gtk_action_group_get_action (window->priv->documents_list_action_group,
					      action_name);

	/* sometimes the action doesn't exist yet, and the proper action
	 * is set active during the documents list menu creation
	 * CHECK: would it be nicer if active_tab was a property and we monitored the notify signal?
	 */
	if (action != NULL)
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

	g_free (action_name);

	view = gedit_tab_get_view (tab);

	/* sync the statusbar */
	update_cursor_position_statusbar (GTK_TEXT_BUFFER (gedit_tab_get_document (tab)),
					  window);
	gedit_statusbar_set_overwrite (GEDIT_STATUSBAR (window->priv->statusbar),
				       gtk_text_view_get_overwrite (GTK_TEXT_VIEW (view)));
				       
	g_signal_emit (G_OBJECT (window), 
		       signals[ACTIVE_TAB_CHANGED], 
		       0, 
		       window->priv->active_tab);				       
}

static void
sync_state (GeditTab *tab, GParamSpec *pspec, GeditWindow *window)
{	
	if (tab != window->priv->active_tab)
		return;

	set_sensitivity_according_to_tab (window, tab);
	
	g_signal_emit (G_OBJECT (window), signals[ACTIVE_TAB_STATE_CHANGED], 0);		
}

static void
sync_name (GeditTab *tab, GParamSpec *pspec, GeditWindow *window)
{
	GtkAction *action;
	gchar *action_name;
	gchar *tab_name; // CHECK escaping
	gint n;
	GeditDocument *doc;

	if (tab != window->priv->active_tab)
		return;

	set_title (window);

	/* sync the item in the documents list menu */
	n = gtk_notebook_page_num (GTK_NOTEBOOK (window->priv->notebook),
				   GTK_WIDGET (tab));
	action_name = g_strdup_printf ("Tab_%d", n);
	action = gtk_action_group_get_action (window->priv->documents_list_action_group,
					      action_name);
	g_return_if_fail (action != NULL);

	tab_name = _gedit_tab_get_name (tab);
	g_object_set (action, "label", tab_name, NULL);

	g_free (action_name);
	g_free (tab_name);

	doc = gedit_tab_get_document (tab);
	action = gtk_action_group_get_action (window->priv->action_group,
					      "FileRevert");
	gtk_action_set_sensitive (action,
				  !gedit_document_is_untitled (doc));

	gedit_plugins_engine_update_plugins_ui (window, FALSE);
}

static void
drag_data_received_cb (GtkWidget        *widget,
		       GdkDragContext   *context,
		       gint              x,
		       gint              y,
		       GtkSelectionData *selection_data,
		       guint             info,
		       guint             time,
		       gpointer          data)
{
	GtkWidget *target_window;
	gchar **uris;
	GSList *uri_list = NULL;
	gint i;

	if (info != TARGET_URI_LIST)
		return;

	g_return_if_fail (widget != NULL);

	target_window = gtk_widget_get_toplevel (widget);
	g_return_if_fail (GEDIT_IS_WINDOW (target_window));

	uris = g_uri_list_extract_uris (selection_data->data);

	for (i = 0; uris[i] != NULL; i++)
		uri_list = g_slist_prepend (uri_list, g_strdup (uris[i]));

	g_strfreev (uris);

	if (uri_list == NULL)
		return;

	uri_list = g_slist_reverse (uri_list);

	gedit_cmd_load_files (GEDIT_WINDOW (target_window),
			      uri_list,
			      NULL);

	g_slist_foreach (uri_list, (GFunc) g_free, NULL);	
	g_slist_free (uri_list);
}

/*
 * Override the gtk_text_view_drag_motion and drag_drop 
 * functions to get URIs
 *
 * If the mime type is text/uri-list, then we will accept
 * the potential drop, or request the data (depending on the
 * function).
 *
 * If the drag context has any other mime type, then pass the
 * information onto the GtkTextView's standard handlers.
 * (widget_class->function_name).
 *
 * See bug #89881 for details
 */

static gboolean
drag_motion_cb (GtkWidget      *widget,
		GdkDragContext *context,
		gint            x,
		gint            y,
		guint           time)
{
	GtkTargetList *tl;
	GtkWidgetClass *widget_class;
	gboolean result;

	tl = gtk_target_list_new (drag_types,
				  G_N_ELEMENTS (drag_types));

	/* If this is a URL, deal with it here, or pass to the text view */
	if (gtk_drag_dest_find_target (widget, context, tl) != GDK_NONE) 
	{
		gdk_drag_status (context, context->suggested_action, time);
		result = TRUE;
	}
	else
	{
		widget_class = GTK_WIDGET_GET_CLASS (widget);
		result = (*widget_class->drag_motion) (widget, context, x, y, time);
	}

	gtk_target_list_unref (tl);

	return result;
}

static gboolean
drag_drop_cb (GtkWidget      *widget,
	      GdkDragContext *context,
	      gint            x,
	      gint            y,
	      guint           time)
{
	GtkTargetList *tl;
	GtkWidgetClass *widget_class;
	gboolean result;
	GdkAtom target;

	tl = gtk_target_list_new (drag_types,
				  G_N_ELEMENTS (drag_types));

	/* If this is a URL, just get the drag data */
	target = gtk_drag_dest_find_target (widget, context, tl);
	if (target != GDK_NONE)
	{
		gtk_drag_get_data (widget, context, target, time);
		result = TRUE;
	}
	else
	{
		widget_class = GTK_WIDGET_GET_CLASS (widget);
		result = (*widget_class->drag_drop) (widget, context, x, y, time);
	}

	gtk_target_list_unref (tl);

	return result;
}

static void
can_undo (GeditDocument *doc,
          gboolean       can,
          GeditWindow   *window)
{
	GtkAction *action;
	
	if (doc != gedit_window_get_active_document (window))
		return;
		
	action = gtk_action_group_get_action (window->priv->action_group,
					     "EditUndo");
	gtk_action_set_sensitive (action, can);
}

static void
can_redo (GeditDocument *doc,
          gboolean       can,
          GeditWindow   *window)
{
	GtkAction *action;

	if (doc != gedit_window_get_active_document (window))
		return;
	
	action = gtk_action_group_get_action (window->priv->action_group,
					     "EditRedo");
	gtk_action_set_sensitive (action, can);
}

static void
notebook_tab_added (GeditNotebook *notebook,
		    GeditTab      *tab,
		    GeditWindow   *window)
{
	GeditView *view;
	GeditDocument *doc;
	GtkTargetList *tl;
	GtkAction *action;

	gedit_debug (DEBUG_MDI);

	++window->priv->num_tabs;

	/* Set sensitivity */
	if (!gtk_action_group_get_sensitive (window->priv->action_group))
		gtk_action_group_set_sensitive (window->priv->action_group,
						TRUE);

	action = gtk_action_group_get_action (window->priv->action_group,
					     "DocumentsMoveToNewWindow");
	gtk_action_set_sensitive (action,
				  window->priv->num_tabs > 1);

	g_signal_connect (tab, 
			 "notify::name",
			  G_CALLBACK (sync_name), 
			  window);
			  
	g_signal_connect (tab, 
			 "notify::state",
			  G_CALLBACK (sync_state), 
			  window);
			  
	view = gedit_tab_get_view (tab);
	doc = gedit_tab_get_document (tab);
	
	/* CHECK: in the old gedit-view we also connected doc "changed" */

	g_signal_connect (doc, 
			  "changed",
			  G_CALLBACK (update_cursor_position_statusbar),
			  window);
	g_signal_connect (doc,
			  "mark_set",/* cursor moved */
			  G_CALLBACK (cursor_moved),
			  window);			  
	g_signal_connect (doc,
			  "can-undo",
			  G_CALLBACK (can_undo),
			  window);
	g_signal_connect (doc,
			  "can-redo",
			  G_CALLBACK (can_redo),
			  window);
	g_signal_connect (view,
			  "toggle_overwrite",
			  G_CALLBACK (update_overwrite_mode_statusbar),
			  window);

	update_documents_list_menu (window);

	/* CHECK: it seems to me this does not work when tab are moved between
	   windows */
	/* Drag and drop support */
	tl = gtk_drag_dest_get_target_list (GTK_WIDGET (view));
	g_return_if_fail (tl != NULL);

	gtk_target_list_add_table (tl, drag_types, G_N_ELEMENTS (drag_types));

	g_signal_connect (view,
			  "drag_data_received",
			  G_CALLBACK (drag_data_received_cb), 
			  NULL);

	/* Get signals before the standard text view functions to deal 
	 * with uris for text files.
	 */
	g_signal_connect (view,
			  "drag_motion",
			  G_CALLBACK (drag_motion_cb), 
			  NULL);
	g_signal_connect (view,
			  "drag_drop",
			  G_CALLBACK (drag_drop_cb), 
			  NULL);

	g_signal_emit (G_OBJECT (window), signals[TAB_ADDED], 0, tab);
}

static void
notebook_tab_removed (GeditNotebook *notebook,
		      GeditTab      *tab,
		      GeditWindow   *window)
{
	GeditView     *view;
	GeditDocument *doc;
	GtkAction     *action;
	
	gedit_debug (DEBUG_MDI);
	
	--window->priv->num_tabs;
	
	view = gedit_tab_get_view (tab);
	doc = gedit_tab_get_document (tab);
	
	g_signal_handlers_disconnect_by_func (tab,
					      G_CALLBACK (sync_name), 
					      window);
	g_signal_handlers_disconnect_by_func (tab,
					      G_CALLBACK (sync_state), 
					      window);
	g_signal_handlers_disconnect_by_func (doc,
					      G_CALLBACK (update_cursor_position_statusbar), 
					      window);
	g_signal_handlers_disconnect_by_func (doc,
					      G_CALLBACK (cursor_moved), 
					      window);					      
	g_signal_handlers_disconnect_by_func (doc, 
					      G_CALLBACK (can_undo),
					      window);
	g_signal_handlers_disconnect_by_func (doc, 
					      G_CALLBACK (can_redo),
					      window);
	g_signal_handlers_disconnect_by_func (view, 
					      G_CALLBACK (update_overwrite_mode_statusbar),
					      window);
	g_signal_handlers_disconnect_by_func (view, 
					      G_CALLBACK (drag_data_received_cb),
					      NULL);
	g_signal_handlers_disconnect_by_func (view, 
					      G_CALLBACK (drag_motion_cb),
					      NULL);
	g_signal_handlers_disconnect_by_func (view, 
					      G_CALLBACK (drag_drop_cb),
					      NULL);

	g_return_if_fail (window->priv->num_tabs >= 0);
	if (window->priv->num_tabs == 0)
	{
		GeditApp *app;
		const GSList *windows;
			
		app = gedit_app_get_default ();
		windows = gedit_app_get_windows (app);		
		g_return_if_fail (windows != NULL);
		
		window->priv->active_tab = NULL;
			       
		set_title (window);
			
		/* Remove line and col info */
		gedit_statusbar_set_cursor_position (
				GEDIT_STATUSBAR (window->priv->statusbar),
				-1,
				-1);
				
		gedit_statusbar_clear_overwrite (
				GEDIT_STATUSBAR (window->priv->statusbar));								
	}

	if (!window->priv->removing_all_tabs)
		update_documents_list_menu (window);
	else
	{
		if (window->priv->num_tabs == 0)
			update_documents_list_menu (window);
	}
	
	/* Set sensitivity */
	if (window->priv->num_tabs == 0)
	{
		gtk_action_group_set_sensitive (window->priv->action_group,
						FALSE);

		action = gtk_action_group_get_action (window->priv->action_group,
						      "ViewHighlightMode");

		gtk_action_set_sensitive (action, FALSE);
		
		gedit_plugins_engine_update_plugins_ui (window, FALSE);
	}

	if (window->priv->num_tabs <= 1)
	{
		action = gtk_action_group_get_action (window->priv->action_group,
						     "DocumentsMoveToNewWindow");
		gtk_action_set_sensitive (action,
					  FALSE);
	}
	
	g_signal_emit (G_OBJECT (window), signals[TAB_REMOVED], 0, tab);	
}

static void
notebook_tabs_reordered (GeditNotebook *notebook,
			 GeditWindow   *window)
{
	update_documents_list_menu (window);
	
	g_signal_emit (G_OBJECT (window), signals[TABS_REORDERED], 0);
}

static void
notebook_tab_detached (GeditNotebook *notebook,
		       GeditTab      *tab,
		       GeditWindow   *window)
{
	GeditWindow *new_window;
	
	new_window = clone_window (window);
		
	gedit_notebook_move_tab (notebook,
				 GEDIT_NOTEBOOK (_gedit_window_get_notebook (new_window)),
				 tab, 0);
				 
	gtk_window_set_position (GTK_WINDOW (new_window), 
				 GTK_WIN_POS_MOUSE);
					 
	gtk_widget_show (GTK_WIDGET (new_window));
}		      

static gboolean
show_notebook_popup_menu (GtkNotebook    *notebook,
			  GeditWindow    *window,
			  GdkEventButton *event)
{
	GtkWidget *menu;
//	GtkAction *action;

	menu = gtk_ui_manager_get_widget (window->priv->manager, "/NotebookPopup");
	g_return_val_if_fail (menu != NULL, FALSE);

// CHECK do we need this?
#if 0
	/* allow extensions to sync when showing the popup */
	action = gtk_action_group_get_action (window->priv->action_group,
					      "NotebookPopupAction");
	g_return_val_if_fail (action != NULL, FALSE);
	gtk_action_activate (action);
#endif
	if (event != NULL)
	{
		gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
				NULL, NULL,
				event->button, event->time);
	}
	else
	{
		GtkWidget *tab;
		GtkWidget *tab_label;

		tab = GTK_WIDGET (gedit_window_get_active_tab (window));
		g_return_val_if_fail (tab != NULL, FALSE);

		tab_label = gtk_notebook_get_tab_label (notebook, tab);

		gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
				gedit_utils_menu_position_under_widget, tab_label,
				0, gtk_get_current_event_time ());

		gtk_menu_shell_select_first (GTK_MENU_SHELL (menu), FALSE);
	}

	return TRUE;
}

static gboolean
notebook_button_press_event (GtkNotebook    *notebook,
			     GdkEventButton *event,
			     GeditWindow    *window)
{
	if (GDK_BUTTON_PRESS == event->type && 3 == event->button)
	{
		return show_notebook_popup_menu (notebook, window, event);
	}

	return FALSE;
}

static gboolean
notebook_popup_menu (GtkNotebook *notebook,
		     GeditWindow *window)
{
	/* Only respond if the notebook is the actual focus */
	if (GEDIT_IS_NOTEBOOK (gtk_window_get_focus (GTK_WINDOW (window))))
	{
		return show_notebook_popup_menu (notebook, window, NULL);
	}

	return FALSE;
}

static void
side_panel_size_allocate (GtkWidget     *widget,
			  GtkAllocation *allocation,
			  GeditWindow   *window)
{
	window->priv->side_panel_size = allocation->width;
}

static void
side_panel_hide (GtkWidget   *panel,
		 GeditWindow *window)
{
	_gedit_window_set_side_panel_visible (window, FALSE);
}
		 
static void
create_side_panel (GeditWindow *window)
{
	GtkAction *action;
	gboolean visible;
	GtkWidget *documents_panel;
	
	window->priv->side_panel = gedit_panel_new ();

  	gtk_paned_pack1 (GTK_PANED (window->priv->hpaned), 
  			 window->priv->side_panel, 
  			 TRUE, 
  			 FALSE);
	gtk_widget_set_size_request (window->priv->side_panel, 100, -1);  			 
  			 
  	g_signal_connect (window->priv->side_panel,
  			  "size_allocate",
  			  G_CALLBACK (side_panel_size_allocate),
  			  window);

	g_signal_connect (window->priv->side_panel,
  			  "hide",
  			  G_CALLBACK (side_panel_hide),
  			  window);
  			  
	gtk_paned_set_position (GTK_PANED (window->priv->hpaned),
				MAX (100, gedit_prefs_manager_get_side_panel_size ()));
				

	documents_panel = gedit_documents_panel_new (window);
  	gedit_panel_add_item (GEDIT_PANEL (window->priv->side_panel), 
  			      documents_panel, 
  			      "Documents", 
  			      GTK_STOCK_FILE);

	window->priv->search_panel = gedit_search_panel_new (window);
	gedit_panel_add_item (GEDIT_PANEL (window->priv->side_panel), 
			      window->priv->search_panel, 
			      "Search", 
			      GTK_STOCK_FIND);
	
	visible = gedit_prefs_manager_get_side_pane_visible ();
	
	action = gtk_action_group_get_action (window->priv->action_group,
					      "ViewSidePane");		
		
	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)) != visible)
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), visible);

	if (visible)
		gtk_widget_show (window->priv->side_panel);
}

static void
create_bottom_panel (GeditWindow *window) 
{
	window->priv->bottom_panel = gedit_panel_new ();
  	gtk_paned_pack2 (GTK_PANED (window->priv->vpaned), 
  			 window->priv->bottom_panel, 
  			 FALSE, 
  			 TRUE);
  			 
	gtk_paned_set_position (GTK_PANED (window->priv->vpaned),
				gedit_prefs_manager_get_bottom_panel_size ());
				  			 
  	gtk_widget_hide_all (window->priv->bottom_panel);
}

/* Generates a unique string for a window role.
 *
 * Taken from EOG.
 */
static gchar *
gen_role (void)
{
        gchar *ret;
	static gchar *hostname;
	time_t t;
	static gint serial;

	t = time (NULL);

	if (!hostname)
	{
		static char buffer[512];

		if ((gethostname (buffer, sizeof (buffer) - 1) == 0) &&
		    (buffer[0] != 0))
			hostname = buffer;
		else
			hostname = "localhost";
	}

	ret = g_strdup_printf ("gedit-window-%d-%d-%d-%ld-%d@%s",
			       getpid (),
			       getgid (),
			       getppid (),
			       (long) t,
			       serial++,
			       hostname);

	return ret;
}

static void
gedit_window_init (GeditWindow *window)
{
	GtkWidget *main_box;

	window->priv = GEDIT_WINDOW_GET_PRIVATE (window);
	window->priv->active_tab = NULL;
	window->priv->num_tabs = 0;
	window->priv->removing_all_tabs = FALSE;
	
	window->priv->window_group = gtk_window_group_new ();
	gtk_window_group_add_window (window->priv->window_group, GTK_WINDOW (window));
	
	main_box = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), main_box);
	gtk_widget_show (main_box);

	/* Add menu bar and toolbar bar */
	create_menu_bar_and_toolbar (window, main_box);

	/* Add status bar */
	create_statusbar (window, main_box);

	/* Add the main area */
	window->priv->hpaned = gtk_hpaned_new ();
  	gtk_box_pack_start (GTK_BOX (main_box), 
  			    window->priv->hpaned, 
  			    TRUE, 
  			    TRUE, 
  			    0);
	gtk_widget_show (window->priv->hpaned);

  	create_side_panel (window);
  	  	
	window->priv->vpaned = gtk_vpaned_new ();
  	gtk_paned_pack2 (GTK_PANED (window->priv->hpaned), 
  			 window->priv->vpaned, 
  			 TRUE, 
  			 FALSE);
  	gtk_widget_show (window->priv->vpaned);
  	
	window->priv->notebook = gedit_notebook_new ();
  	gtk_paned_pack1 (GTK_PANED (window->priv->vpaned), 
  			 window->priv->notebook,
  			 TRUE, 
  			 TRUE);
  	gtk_widget_show (window->priv->notebook);  			 

	create_bottom_panel (window);
	
	/* Set visibility of panels */
	// TODO

	if (gtk_window_get_role (GTK_WINDOW (window)) == NULL)
	{
		gchar *role;

		role = gen_role ();
		gtk_window_set_role (GTK_WINDOW (window), role);
		g_free (role);
	}

	/* Drag and drop support */
	gtk_drag_dest_set (GTK_WIDGET (window),
			   GTK_DEST_DEFAULT_MOTION |
			   GTK_DEST_DEFAULT_HIGHLIGHT |
			   GTK_DEST_DEFAULT_DROP,
			   drag_types,
			   G_N_ELEMENTS (drag_types),
			   GDK_ACTION_COPY);

	/* Connect signals */
	g_signal_connect (G_OBJECT (window->priv->notebook),
			  "switch_page",
			  G_CALLBACK (notebook_switch_page),
			  window);
	g_signal_connect (G_OBJECT (window->priv->notebook),
			  "tab_added",
			  G_CALLBACK (notebook_tab_added),
			  window);
	g_signal_connect (G_OBJECT (window->priv->notebook),
			  "tab_removed",
			  G_CALLBACK (notebook_tab_removed),
			  window);
	g_signal_connect (G_OBJECT (window->priv->notebook),
			  "tabs_reordered",
			  G_CALLBACK (notebook_tabs_reordered),
			  window);			  
	g_signal_connect (G_OBJECT (window->priv->notebook),
			  "tab_detached",
			  G_CALLBACK (notebook_tab_detached),
			  window);
	g_signal_connect (G_OBJECT (window->priv->notebook),
			  "button-press-event",
			  G_CALLBACK (notebook_button_press_event),
			  window);
	g_signal_connect (G_OBJECT (window->priv->notebook),
			  "popup-menu",
			  G_CALLBACK (notebook_popup_menu),
			  window);

	/* connect instead pf override, so that we can
	 * share the cb code with the view */
	g_signal_connect (G_OBJECT (window), 
			  "drag_data_received",
	                  G_CALLBACK (drag_data_received_cb), 
	                  NULL);
	                  
        gedit_plugins_engine_update_plugins_ui (window, TRUE);
}

GeditView *
gedit_window_get_active_view (GeditWindow *window)
{
	GeditView *view;
	
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);
	
	if (window->priv->active_tab == NULL)
		return NULL;
		
	view = gedit_tab_get_view (GEDIT_TAB (window->priv->active_tab));
	
	return view;
}

GeditDocument *
gedit_window_get_active_document (GeditWindow *window)
{
	GeditView *view;
	
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);
	
	view = gedit_window_get_active_view (window);
	if (view == NULL)
		return NULL;
	
	return GEDIT_DOCUMENT (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
}

GtkWidget *
_gedit_window_get_notebook (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);

	return window->priv->notebook;
}

GeditTab *
gedit_window_create_tab (GeditWindow *window,
			 gboolean     jump_to)
{
	GeditTab *tab;
	
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);
	
	tab = GEDIT_TAB (_gedit_tab_new ());	
	gtk_widget_show (GTK_WIDGET (tab));	
	
	gedit_notebook_add_tab (GEDIT_NOTEBOOK (window->priv->notebook),
				tab,
				-1,
				jump_to);
				
	return tab;
}

GeditTab *
gedit_window_create_tab_from_uri (GeditWindow         *window,
				  const gchar         *uri,
				  const GeditEncoding *encoding,
				  gint                 line_pos,
				  gboolean             create,
				  gboolean             jump_to)
{
	GtkWidget *tab;
	
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);
	g_return_val_if_fail (uri != NULL, NULL);
	
	tab = _gedit_tab_new_from_uri (uri,
				       encoding,
				       line_pos,
				       create);	
	if (tab == NULL)
		return NULL;
		
	gtk_widget_show (tab);	
	
	gedit_notebook_add_tab (GEDIT_NOTEBOOK (window->priv->notebook),
				GEDIT_TAB (tab),
				-1,
				jump_to);
				
	return GEDIT_TAB (tab);
}				  

GeditTab *
gedit_window_get_active_tab (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);
	
	return (window->priv->active_tab == NULL) ? 
				NULL : GEDIT_TAB (window->priv->active_tab);
}

static void
add_document (GeditTab *tab, GList **res)
{
	GeditDocument *doc;
	
	doc = gedit_tab_get_document (tab);
	
	*res = g_list_prepend (*res, doc);
}

/* Returns a newly allocated list with all the documents in the window */
GList *
gedit_window_get_documents (GeditWindow *window)
{
	GList *res = NULL;

	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);
	
	gtk_container_foreach (GTK_CONTAINER (window->priv->notebook),
			       (GtkCallback)add_document,
			       &res);
			       
	res = g_list_reverse (res);
	
	return res;
}

static void
add_view (GeditTab *tab, GList **res)
{
	GeditView *view;
	
	view = gedit_tab_get_view (tab);
	
	*res = g_list_prepend (*res, view);
}

/* Returns a newly allocated list with all the views in the window */
GList *
gedit_window_get_views (GeditWindow *window)
{
	GList *res = NULL;

	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);
	
	gtk_container_foreach (GTK_CONTAINER (window->priv->notebook),
			       (GtkCallback)add_view,
			       &res);
			       
	res = g_list_reverse (res);
	
	return res;
}

void 
gedit_window_close_tab (GeditWindow *window,
			GeditTab    *tab)
{
	g_return_if_fail (GEDIT_IS_WINDOW (window));
	g_return_if_fail (GEDIT_IS_TAB (tab));
	
	gedit_notebook_remove_tab (GEDIT_NOTEBOOK (window->priv->notebook),
				   tab);
}

void 
gedit_window_close_all_tabs (GeditWindow *window)
{
	g_return_if_fail (GEDIT_IS_WINDOW (window));

	window->priv->removing_all_tabs = TRUE;
	
	gedit_notebook_remove_all_tabs (GEDIT_NOTEBOOK (window->priv->notebook));
	
	window->priv->removing_all_tabs = FALSE;
}

void
_gedit_window_set_statusbar_visible (GeditWindow *window,
				     gboolean     visible)
{
	GtkAction *action;
	static gboolean recursione_guard = FALSE;
	
	g_return_if_fail (GEDIT_IS_WINDOW (window));

	if (recursione_guard)
		return;
		
	recursione_guard = TRUE;

	visible = (visible != FALSE);
		
	if (visible)
		gtk_widget_show (window->priv->statusbar);
	else
		gtk_widget_hide (window->priv->statusbar);

	if (gedit_prefs_manager_statusbar_visible_can_set ())
		gedit_prefs_manager_set_statusbar_visible (visible);
		
	action = gtk_action_group_get_action (window->priv->action_group,
					      "ViewStatusbar");		
		
	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)) != visible)
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), visible);

	recursione_guard = FALSE;		
}

void
_gedit_window_set_toolbar_visible (GeditWindow *window,
				   gboolean     visible)
{
	GtkAction *action;
	static gboolean recursione_guard = FALSE;
	
	g_return_if_fail (GEDIT_IS_WINDOW (window));

	if (recursione_guard)
		return;
		
	recursione_guard = TRUE;
	
	visible = (visible != FALSE);
		
	if (visible)
		gtk_widget_show (window->priv->toolbar);
	else
		gtk_widget_hide (window->priv->toolbar);

	if (gedit_prefs_manager_toolbar_visible_can_set ())
		gedit_prefs_manager_set_toolbar_visible (visible);

	action = gtk_action_group_get_action (window->priv->action_group,
					      "ViewToolbar");		
		
	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)) != visible)
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), visible);
	
	recursione_guard = FALSE;
}

void
_gedit_window_set_side_panel_visible (GeditWindow *window,
				      gboolean     visible)
{
	GtkAction *action;
	static gboolean recursion_guard = FALSE;
	gboolean show = FALSE;
	g_return_if_fail (GEDIT_IS_WINDOW (window));
	
	if (recursion_guard)
		return;
		
	recursion_guard = TRUE;

	visible = (visible != FALSE);
	
	if (visible &&
	    (GTK_WIDGET_VISIBLE (window->priv->side_panel) != visible))
	{
		gtk_widget_show (window->priv->side_panel);
		show = TRUE;
	}
	else
	{
		gtk_widget_hide (window->priv->side_panel);
		if (window->priv->active_tab)
			gtk_widget_grab_focus (GTK_WIDGET (
					gedit_tab_get_view (GEDIT_TAB (window->priv->active_tab))));
	}

	if (gedit_prefs_manager_side_pane_visible_can_set ())
		gedit_prefs_manager_set_side_pane_visible (visible);

	action = gtk_action_group_get_action (window->priv->action_group,
					      "ViewSidePane");		

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)) != visible)
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), visible);

	if (show)
	{
		g_print ("GRAB side panel\n");
		gtk_widget_grab_focus (window->priv->side_panel);
	}

	recursion_guard = FALSE;
}

void
_gedit_window_set_bottom_panel_visible (GeditWindow *window,
					gboolean     visible)
{
	GtkAction *action;
	static gboolean recursion_guard = FALSE;
	gboolean show = FALSE;
	g_return_if_fail (GEDIT_IS_WINDOW (window));
	
	if (recursion_guard)
		return;

	recursion_guard = TRUE;

	visible = (visible != FALSE);

	if (visible &&
	    (GTK_WIDGET_VISIBLE (window->priv->bottom_panel) != visible))
	{
		gtk_widget_show (window->priv->bottom_panel);
		show = TRUE;
	}
	else
	{
		gtk_widget_hide (window->priv->bottom_panel);
		if (window->priv->active_tab)
			gtk_widget_grab_focus (GTK_WIDGET (
					gedit_tab_get_view (GEDIT_TAB (window->priv->active_tab))));
	}

	if (gedit_prefs_manager_bottom_panel_visible_can_set ())
		gedit_prefs_manager_set_bottom_panel_visible (visible);

	action = gtk_action_group_get_action (window->priv->action_group,
					      "ViewBottomPanel");		

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)) != visible)
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), visible);

	if (show)
	{
		g_print ("GRAB bottom panel\n");
		gtk_widget_grab_focus (window->priv->bottom_panel);
	}

	recursion_guard = FALSE;
}

GeditWindow *
_gedit_window_move_tab_to_new_window (GeditWindow *window,
				      GeditTab    *tab)
{
	GeditWindow *new_window;

	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);
	g_return_val_if_fail (GEDIT_IS_TAB (tab), NULL);
	g_return_val_if_fail (gtk_notebook_get_n_pages (
				GTK_NOTEBOOK (window->priv->notebook)) > 1, 
			      NULL);
			      
	new_window = clone_window (window);

	gedit_notebook_move_tab (GEDIT_NOTEBOOK (window->priv->notebook),
				 GEDIT_NOTEBOOK (new_window->priv->notebook),
				 tab,
				 -1);
				 
	gtk_widget_show (GTK_WIDGET (new_window));
	
	return new_window;
}				      

void
gedit_window_set_active_tab (GeditWindow *window,
			     GeditTab    *tab)
{
	gint page_num;
	
	g_return_if_fail (GEDIT_IS_WINDOW (window));
	g_return_if_fail (GEDIT_IS_TAB (tab));
	
	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (window->priv->notebook),
					  GTK_WIDGET (tab));
	g_return_if_fail (page_num != -1);
	
	gtk_notebook_set_current_page (GTK_NOTEBOOK (window->priv->notebook),
				       page_num);
}

GtkWindowGroup *
gedit_window_get_group (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);
	
	return window->priv->window_group;
}

gboolean
_gedit_window_is_removing_all_tabs (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), FALSE);
	
	return window->priv->removing_all_tabs;
}

GtkUIManager *
gedit_window_get_ui_manager (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);
	
	return window->priv->manager;
}

GeditPanel *
gedit_window_get_side_panel (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);

	return GEDIT_PANEL (window->priv->side_panel);
}

GtkWidget *
_gedit_window_get_search_panel (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);

	return window->priv->search_panel;	
}

GtkWidget *
gedit_window_get_statusbar (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), 0);

	return window->priv->statusbar;
}
