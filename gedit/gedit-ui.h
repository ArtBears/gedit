/*
 * gedit-ui.h
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

#ifndef __GEDIT_UI_H__
#define __GEDIT_UI_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include "gedit-commands.h"

G_BEGIN_DECLS

static const GtkActionEntry gedit_menu_entries[] =
{
	/* Toplevel */
	{ "File", NULL, N_("_File") },
	{ "Edit", NULL, N_("_Edit") },
	{ "View", NULL, N_("_View") },
	{ "Search", NULL, N_("_Search") },
	{ "Tools", NULL, N_("_Tools") },
	{ "Documents", NULL, N_("_Documents") },
	{ "Help", NULL, N_("_Help") },

	/* File menu */
	{ "FileNew", GTK_STOCK_NEW, N_("_New"), "<control>N",
	  N_("Create a new document"), G_CALLBACK (gedit_cmd_file_new) },
	{ "FileOpen", GTK_STOCK_OPEN, N_("_Open..."), "<control>O",
	  N_("Open a file"), G_CALLBACK (gedit_cmd_file_open) },
	{ "FileOpenURI", NULL, N_("Open _Location..."), "<control>L",
	  N_("Open a file from a specified location"), G_CALLBACK (gedit_cmd_file_open_uri) },
	{ "FileSave", GTK_STOCK_SAVE, N_("Save"), "<control>S",
	  N_("Save the current file"), G_CALLBACK (gedit_cmd_file_save) },
	{ "FileSaveAs", GTK_STOCK_SAVE_AS, N_("Save _As..."), "<shift><control>S",
	  N_("Save the current file with a different name"), G_CALLBACK (gedit_cmd_file_save_as) },
	{ "FileRevert", GTK_STOCK_REVERT_TO_SAVED, N_("_Revert"), NULL,
	  N_("Revert to a saved version of the file"), G_CALLBACK (gedit_cmd_file_revert) },
	{ "FilePageSetup", NULL, N_("Page Set_up..."), NULL,
	  N_("Setup the page settings"), G_CALLBACK (gedit_cmd_file_page_setup) },
	{ "FilePrintPreview", GTK_STOCK_PRINT_PREVIEW, N_("Print Previe_w"),"<control><shift>P",
	  N_("Print preview"), G_CALLBACK (gedit_cmd_file_print_preview) },
	 { "FilePrint", GTK_STOCK_PRINT, N_("_Print..."), "<control>P",
	  N_("Print the current page"), G_CALLBACK (gedit_cmd_file_print) },
	{ "FileClose", GTK_STOCK_CLOSE, N_("_Close"), "<control>W",
	  N_("Close the current file"), G_CALLBACK (gedit_cmd_file_close) },
	{ "FileQuit", GTK_STOCK_QUIT, N_("_Quit"), "<control>Q",
	  N_("Quit the program"), G_CALLBACK (gedit_cmd_file_quit) },

	/* Edit menu */
	{ "EditUndo", GTK_STOCK_UNDO, N_("_Undo"), "<control>Z",
	  N_("Undo the last action"), G_CALLBACK (gedit_cmd_edit_undo) },
	{ "EditRedo", GTK_STOCK_REDO, N_("_Redo"), "<shift><control>Z",
	  N_("Redo the last undone action"), G_CALLBACK (gedit_cmd_edit_redo) },
	{ "EditCut", GTK_STOCK_CUT, N_("Cu_t"), "<control>X",
	  N_("Cut the selection"), G_CALLBACK (gedit_cmd_edit_cut) },
	{ "EditCopy", GTK_STOCK_COPY, N_("_Copy"), "<control>C",
	  N_("Copy the selection"), G_CALLBACK (gedit_cmd_edit_copy) },
	{ "EditPaste", GTK_STOCK_PASTE, N_("_Paste"), "<control>V",
	  N_("Paste the clipboard"), G_CALLBACK (gedit_cmd_edit_paste) },
	{ "EditDelete", GTK_STOCK_DELETE, N_("_Delete"), NULL,
	  N_("Delete the selected text"), G_CALLBACK (gedit_cmd_edit_delete) },
	{ "EditSelectAll", NULL, N_("Select _All"), "<control>A",
	  N_("Select the entire document"), G_CALLBACK (gedit_cmd_edit_select_all) },
	{ "EditPreferences", GTK_STOCK_PREFERENCES, N_("Pr_eferences"), NULL,
	  N_("Configure the application"), G_CALLBACK (gedit_cmd_edit_preferences) },

	/* View menu */

	/* Search menu */
	{ "SearchFind", GTK_STOCK_FIND, N_("_Find..."), "<control>F",
	  N_("Search for text"), G_CALLBACK (gedit_cmd_search_find) },
	{ "SearchFindNext", NULL, N_("Find Ne_xt"), "<control>G",
	  N_("Search forwards for the same text"), G_CALLBACK (gedit_cmd_search_find_next) },
	{ "SearchFindPrevious", NULL, N_("Find Pre_vious"), "<shift><control>G",
	  N_("Search backwards for the same text"), G_CALLBACK (gedit_cmd_search_find_prev) },
	{ "SearchReplace", GTK_STOCK_FIND_AND_REPLACE, N_("_Replace..."), "<control>R",
	  N_("Search for and replace text"), G_CALLBACK (gedit_cmd_search_replace) },
	{ "SearchGoToLine", GTK_STOCK_JUMP_TO, N_("Go to _Line..."), "<control>I",
	  N_("Go to a specific line"), G_CALLBACK (gedit_cmd_search_goto_line) },

	/* Documents menu */
	{ "FileSaveAll", GTK_STOCK_SAVE, N_("_Save All"), "<shift><control>L",
	  N_("Save all open files"), G_CALLBACK (gedit_cmd_file_save_all) },
	{ "FileCloseAll", GTK_STOCK_CLOSE, N_("_Close All"), "<shift><control>W",
	  N_("Close all open files"), G_CALLBACK (gedit_cmd_file_close_all) },
	{ "DocumentsMoveToNewWindow", NULL, N_("_Move to New Window"), NULL,
	  N_("Move the current document to a new window"), G_CALLBACK (gedit_cmd_documents_move_to_new_window) },
	  
	/* Help menu */
	{"HelpContents", GTK_STOCK_HELP, N_("_Contents"), "F1",
	 N_("Open the gedit manual"), G_CALLBACK (gedit_cmd_help_contents) },
	{ "HelpAbout", GTK_STOCK_ABOUT, N_("_About"), NULL,
	 N_("About this application"), G_CALLBACK (gedit_cmd_help_about) }
};

static const guint gedit_n_menu_entries = G_N_ELEMENTS (gedit_menu_entries);


static const GtkToggleActionEntry gedit_toggle_menu_entries[] =
{
	{ "ViewToolbar", NULL, N_("_Toolbar"), NULL,
	  N_("Show or hide the toolbar in the current window"),
	  NULL, TRUE },
	{ "ViewStatusbar", NULL, N_("_Statusbar"), NULL,
	  N_("Show or hide the statusbar in the current window"),
	  G_CALLBACK (gedit_cmd_view_show_statusbar), TRUE },
	{ "ViewOutputWindow", NULL, N_("_Output Window"), "<control><alt>O",
	  N_("Show or hide the output window in the current window"),
	  NULL, FALSE }
};

static const guint gedit_n_toggle_menu_entries = G_N_ELEMENTS (gedit_toggle_menu_entries);

G_END_DECLS

#endif  /* __GEDIT_UI_H__  */
