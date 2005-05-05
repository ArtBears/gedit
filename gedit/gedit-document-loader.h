/*
 * gedit-document-loader.h
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

#ifndef __GEDIT_DOCUMENT_LOADER_H__
#define __GEDIT_DOCUMENT_LOADER_H__

#include <gedit/gedit-document.h>
#include <libgnomevfs/gnome-vfs-file-size.h>

G_BEGIN_DECLS

/*
 * Type checking and casting macros
 */
#define GEDIT_TYPE_DOCUMENT_LOADER              (gedit_document_loader_get_type())
#define GEDIT_DOCUMENT_LOADER(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GEDIT_TYPE_DOCUMENT_LOADER, GeditDocumentLoader))
#define GEDIT_DOCUMENT_LOADER_CONST(obj)        (G_TYPE_CHECK_INSTANCE_CAST((obj), GEDIT_TYPE_DOCUMENT_LOADER, GeditDocumentLoader const))
#define GEDIT_DOCUMENT_LOADER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GEDIT_TYPE_DOCUMENT_LOADER, GeditDocumentLoaderClass))
#define GEDIT_IS_DOCUMENT_LOADER(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GEDIT_TYPE_DOCUMENT_LOADER))
#define GEDIT_IS_DOCUMENT_LOADER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_DOCUMENT_LOADER))
#define GEDIT_DOCUMENT_LOADER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GEDIT_TYPE_DOCUMENT_LOADER, GeditDocumentLoaderClass))

/* This specifies the current phase in the loading operation.  Phases whose
   comments are marked with `(*)' are always reported in "normal" (i.e. no
   error) condition; the other ones are only reported if an error happens in
   that specific phase.  */
typedef enum {
	/* Idle (not loading a file) */
	GEDIT_DOCUMENT_LOADER_IDLE,
	/* Initial phase */
	GEDIT_DOCUMENT_LOADER_PHASE_INITIAL,
	/* Ready to go (*) */
	GEDIT_DOCUMENT_LOADER_PHASE_READY_TO_GO,
	/* Open the file to read */
	GEDIT_DOCUMENT_LOADER_PHASE_OPEN,
	/* Getting info on the document to load */
	GEDIT_DOCUMENT_LOADER_PHASE_GETTING_INFO,
	/* Got info on the document to load (*) */
	GEDIT_DOCUMENT_LOADER_PHASE_GOT_INFO,	
	/* Loading the file (*) */
	GEDIT_DOCUMENT_LOADER_PHASE_LOADING,
	/* Reading the file */
	GEDIT_DOCUMENT_LOADER_PHASE_READING,	
	/* Converting file to UTF-8 */
	GEDIT_DOCUMENT_LOADER_PHASE_CONVERTING,	
	/* File loaded (*) */
	GEDIT_DOCUMENT_LOADER_PHASE_LOADED,
	/* File loading cancelled */
	GEDIT_DOCUMENT_LOADER_PHASE_CANCELLED,
	/* Operation finished (*) */
	GEDIT_DOCUMENT_LOADER_PHASE_COMPLETED,
	/* Loader has been used. Reset or destroy */
	GEDIT_DOCUMENT_LOADER_PHASE_END,

	GEDIT_DOCUMENT_LOADER_NUM_OF_PHASES
} GeditDocumentLoaderPhase;

/* Private structure type */
typedef struct _GeditDocumentLoaderPrivate GeditDocumentLoaderPrivate;

/*
 * Main object structure
 */
typedef struct _GeditDocumentLoader GeditDocumentLoader;

struct _GeditDocumentLoader 
{
	GObject object;

	/*< private > */
	GeditDocumentLoaderPrivate *priv;
};

/*
 * Class definition
 */
typedef struct _GeditDocumentLoaderClass GeditDocumentLoaderClass;

struct _GeditDocumentLoaderClass 
{
	GObjectClass parent_class;

	void (* loading) (GeditDocumentLoader *loader,
			  gboolean             completed,
			  const GError        *error);
};

/*
 * Public methods
 */
GType 		 	 gedit_document_loader_get_type		(void) G_GNUC_CONST;

GeditDocumentLoader 	*gedit_document_loader_new 		(GeditDocument        *doc);

/* If enconding == NULL, the encoding will be autodetected */
gboolean		 gedit_document_loader_load		(GeditDocumentLoader  *loader,
							 	 const gchar          *uri,
								 const GeditEncoding  *encoding);
#if 0
gboolean		 gedit_document_loader_load_from_stdin	(GeditDocumentLoader  *loader);
							 
void			 gedit_document_loader_cancel		(GeditDocumentLoader  *loader);
#endif

GeditDocumentLoaderPhase gedit_document_loader_get_phase	(GeditDocumentLoader  *loader);
#if 0
gchar			*gedit_document_loader_get_message	(GeditDocumentLoader  *loader);
#endif
/* Returns STDIN_URI if loading from stdin */
#define STDIN_URI "stdin:" 
const gchar		*gedit_document_loader_get_uri		(GeditDocumentLoader  *loader);

const gchar		*gedit_document_loader_get_mime_type	(GeditDocumentLoader  *loader);

time_t 			 gedit_document_loader_get_mtime 	(GeditDocumentLoader  *loader);

/* Returns 0 if file size is unknown */
GnomeVFSFileSize	 gedit_document_loader_get_file_size	(GeditDocumentLoader  *loader);									 

GnomeVFSFileSize	 gedit_document_loader_get_bytes_read	(GeditDocumentLoader  *loader);									 


G_END_DECLS

#endif  /* __GEDIT_DOCUMENT_LOADER_H__  */
