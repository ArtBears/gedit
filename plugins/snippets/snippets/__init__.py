#    Gedit snippets plugin
#    Copyright (C) 2005-2006  Jesse van den Kieboom <jesse@icecrew.nl>
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
    
import gedit
import gtk
from gtk import gdk
import sys
import os
import shutil
from SnippetPluginInstance import SnippetsPluginInstance
from SnippetsLibrary import SnippetsLibrary
from SnippetsDialog import SnippetsDialog
from Snippet import Snippet

class SnippetsPlugin(gedit.Plugin):
	def __init__(self):
		gedit.Plugin.__init__(self)

		self.dlg = None
		
		library = SnippetsLibrary()
		library.set_accelerator_callback(self.accelerator_activated)
		
		userdir = os.path.join(os.environ['HOME'], '.gnome2', 'gedit', 'snippets')
		library.set_dirs(userdir, self.system_dirs())
	
	def system_dirs(self):
		if 'XDG_DATA_DIRS' in os.environ:
			datadirs = os.environ['XDG_DATA_DIRS']
		else:
			datadirs = '/usr/local/share:/usr/share'
		
		dirs = []
		
		for d in datadirs.split(':'):
			d = os.path.join(d, 'gedit-2', 'plugins', 'snippets')
			
			if os.path.isdir(d):
				dirs.append(d)
		
		return dirs
	
	def activate(self, window):
		data = SnippetsPluginInstance(self)
		window._snippets_plugin_data = data
		data.run(window)

	def deactivate(self, window):
		window._snippets_plugin_data.stop()
		window._snippets_plugin_data = None
		
	def update_ui(self, window):
		window._snippets_plugin_data.update()
	
	def create_configure_dialog(self):
		if not self.dlg:
			self.dlg = SnippetsDialog()
		else:
			self.dlg.run()
		
		window = gedit.app_get_default().get_active_window()
		
		if window:
			self.dlg.dlg.set_transient_for(window)
		
		return self.dlg.dlg
	
	def accelerator_activated(self, group, obj, keyval, mod):
		if hasattr(obj, '_snippets_plugin_data'):
			obj._snippets_plugin_data.accelerator_activated(keyval, mod)
