/*
 * gedit-overlay.c
 * This file is part of gedit
 *
 * Copyright (C) 2011 - Ignacio Casal Quinteiro
 *
 * Based on Mike Krüger <mkrueger@novell.com> work.
 *
 * gedit is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * gedit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "gedit-overlay.h"
#include "gedit-overlay-child.h"

#define GEDIT_OVERLAY_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GEDIT_TYPE_OVERLAY, GeditOverlayPrivate))

struct _GeditOverlayPrivate
{
	GtkWidget *main_widget;
	GtkWidget *relative_widget;
	GSList    *children;
};

enum
{
	PROP_0,
	PROP_MAIN_WIDGET,
	PROP_RELATIVE_WIDGET
};

G_DEFINE_TYPE (GeditOverlay, gedit_overlay, GTK_TYPE_CONTAINER)

static void
add_toplevel_widget (GeditOverlay *overlay,
                     GtkWidget    *child)
{
	gtk_widget_set_parent (child, GTK_WIDGET (overlay));

	overlay->priv->children = g_slist_append (overlay->priv->children,
	                                          child);
}

static void
gedit_overlay_dispose (GObject *object)
{
	G_OBJECT_CLASS (gedit_overlay_parent_class)->dispose (object);
}

static void
gedit_overlay_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
	GeditOverlay *overlay = GEDIT_OVERLAY (object);
	GeditOverlayPrivate *priv = overlay->priv;

	switch (prop_id)
	{
		case PROP_MAIN_WIDGET:
			g_value_set_object (value, priv->main_widget);
			break;

		case PROP_RELATIVE_WIDGET:
			g_value_set_object (value, priv->relative_widget);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_overlay_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
	GeditOverlay *overlay = GEDIT_OVERLAY (object);
	GeditOverlayPrivate *priv = overlay->priv;

	switch (prop_id)
	{
		case PROP_MAIN_WIDGET:
			priv->main_widget = g_value_get_object (value);
			add_toplevel_widget (overlay, priv->main_widget);
			break;

		case PROP_RELATIVE_WIDGET:
			priv->relative_widget = g_value_get_object (value);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_overlay_realize (GtkWidget *widget)
{
	GtkAllocation allocation;
	GdkWindow *window;
	GdkWindowAttr attributes;
	gint attributes_mask;
	GtkStyleContext *context;

	gtk_widget_set_realized (widget, TRUE);

	gtk_widget_get_allocation (widget, &allocation);

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = allocation.x;
	attributes.y = allocation.y;
	attributes.width = allocation.width;
	attributes.height = allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.event_mask = gtk_widget_get_events (widget);
	attributes.event_mask |= GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK;

	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

	window = gdk_window_new (gtk_widget_get_parent_window (widget),
	                         &attributes, attributes_mask);
	gtk_widget_set_window (widget, window);
	gdk_window_set_user_data (window, widget);

	context = gtk_widget_get_style_context (widget);
	gtk_style_context_set_state (context, GTK_STATE_FLAG_NORMAL);
	gtk_style_context_set_background (context, window);
}

static void
gedit_overlay_get_preferred_width (GtkWidget *widget,
                                   gint      *minimum,
                                   gint      *natural)
{
	GeditOverlayPrivate *priv = GEDIT_OVERLAY (widget)->priv;
	GtkWidget *child;
	GSList *children;
	gint child_min, child_nat;

	*minimum = 0;
	*natural = 0;

	for (children = priv->children; children; children = children->next)
	{
		child = children->data;

		if (!gtk_widget_get_visible (child))
			continue;

		gtk_widget_get_preferred_width (child, &child_min, &child_nat);

		*minimum = MAX (*minimum, child_min);
		*natural = MAX (*natural, child_nat);
	}
}

static void
gedit_overlay_get_preferred_height (GtkWidget *widget,
                                    gint      *minimum,
                                    gint      *natural)
{
	GeditOverlayPrivate *priv = GEDIT_OVERLAY (widget)->priv;
	GtkWidget *child;
	GSList *children;
	gint child_min, child_nat;

	*minimum = 0;
	*natural = 0;

	for (children = priv->children; children; children = children->next)
	{
		child = children->data;

		if (!gtk_widget_get_visible (child))
			continue;

		gtk_widget_get_preferred_height (child, &child_min, &child_nat);

		*minimum = MAX (*minimum, child_min);
		*natural = MAX (*natural, child_nat);
	}
}

static void
gedit_overlay_size_allocate (GtkWidget     *widget,
                             GtkAllocation *allocation)
{
	GeditOverlay *overlay = GEDIT_OVERLAY (widget);
	GeditOverlayPrivate *priv = overlay->priv;
	GtkAllocation main_alloc;
	GSList *l;

	GTK_WIDGET_CLASS (gedit_overlay_parent_class)->size_allocate (widget, allocation);

	/* main widget allocation */
	main_alloc.x = 0;
	main_alloc.y = 0;
	main_alloc.width = allocation->width;
	main_alloc.height = allocation->height;

	gtk_widget_size_allocate (overlay->priv->main_widget, &main_alloc);

	/* if a relative widget exists place the floating widgets in relation to it */
	if (priv->relative_widget)
	{
		gtk_widget_get_allocation (priv->relative_widget, &main_alloc);
	}

	for (l = priv->children; l != NULL; l = g_slist_next (l))
	{
		GtkWidget *child = GTK_WIDGET (l->data);
		GtkRequisition req;
		GtkAllocation alloc;
		guint offset;

		if (child == priv->main_widget)
			continue;

		gtk_widget_get_preferred_size (child, &req, NULL);
		offset = gedit_overlay_child_get_offset (GEDIT_OVERLAY_CHILD (child));

		/* FIXME: Add all the positions here */
		switch (gedit_overlay_child_get_position (GEDIT_OVERLAY_CHILD (child)))
		{
			/* The gravity is treated as position and not as a gravity */
			case GEDIT_OVERLAY_CHILD_POSITION_NORTH_EAST:
				alloc.x = main_alloc.width - req.width - offset;
				alloc.y = 0;
				break;
			case GEDIT_OVERLAY_CHILD_POSITION_NORTH_WEST:
				alloc.x = offset;
				alloc.y = 0;
				break;
			case GEDIT_OVERLAY_CHILD_POSITION_SOUTH_WEST:
				alloc.x = offset;
				alloc.y = main_alloc.height - req.height;
				break;
			default:
				alloc.x = 0;
				alloc.y = 0;
		}

		alloc.width = req.width;
		alloc.height = req.height;

		gtk_widget_size_allocate (child, &alloc);
	}
}

static GeditOverlayChild *
get_overlay_child (GeditOverlay *overlay,
                   GtkWidget    *widget)
{
	GeditOverlayChild *overlay_child = NULL;
	GSList *l;

	for (l = overlay->priv->children; l != NULL; l = g_slist_next (l))
	{
		GtkWidget *child = GTK_WIDGET (l->data);

		/* skip the main widget as it is not a OverlayChild */
		if (child == overlay->priv->main_widget)
			continue;

		if (child == widget)
		{
			overlay_child = GEDIT_OVERLAY_CHILD (child);
			break;
		}
		else
		{
			GtkWidget *in_widget;

			/* let's try also with the internal widget */
			g_object_get (child, "widget", &in_widget, NULL);
			g_assert (in_widget != NULL);

			if (in_widget == widget)
			{
				overlay_child = GEDIT_OVERLAY_CHILD (child);
				g_object_unref (in_widget);

				break;
			}

			g_object_unref (in_widget);
		}
	}

	return overlay_child;
}

static void
overlay_add (GtkContainer *overlay,
             GtkWidget    *widget)
{
	GeditOverlayChild *child;

	/* check that the widget is not added yet */
	child = get_overlay_child (GEDIT_OVERLAY (overlay), widget);

	if (child == NULL)
	{
		if (GEDIT_IS_OVERLAY_CHILD (widget))
		{
			child = GEDIT_OVERLAY_CHILD (widget);
		}
		else
		{
			child = gedit_overlay_child_new (widget);
			gtk_widget_show (GTK_WIDGET (child));
		}

		add_toplevel_widget (GEDIT_OVERLAY (overlay), GTK_WIDGET (child));
	}
}

static void
gedit_overlay_remove (GtkContainer *overlay,
                      GtkWidget    *widget)
{
	GeditOverlayPrivate *priv = GEDIT_OVERLAY (overlay)->priv;
	GSList *l;

	for (l = priv->children; l != NULL; l = g_slist_next (l))
	{
		GtkWidget *child = l->data;

		if (child == widget)
		{
			gtk_widget_unparent (widget);
			priv->children = g_slist_remove_link (priv->children,
			                                      l);

			g_slist_free (l);
			break;
		}
	}
}

static void
gedit_overlay_forall (GtkContainer *overlay,
                      gboolean      include_internals,
                      GtkCallback   callback,
                      gpointer      callback_data)
{
	GeditOverlayPrivate *priv = GEDIT_OVERLAY (overlay)->priv;
	GSList *children;

	children = priv->children;
	while (children)
	{
		GtkWidget *child = GTK_WIDGET (children->data);
		children = children->next;

		(* callback) (child, callback_data);
	}
}

static GType
gedit_overlay_child_type (GtkContainer *overlay)
{
	return GTK_TYPE_WIDGET;
}


static void
gedit_overlay_class_init (GeditOverlayClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

	object_class->dispose = gedit_overlay_dispose;
	object_class->get_property = gedit_overlay_get_property;
	object_class->set_property = gedit_overlay_set_property;

	widget_class->realize = gedit_overlay_realize;
	widget_class->get_preferred_width = gedit_overlay_get_preferred_width;
	widget_class->get_preferred_height = gedit_overlay_get_preferred_height;
	widget_class->size_allocate = gedit_overlay_size_allocate;

	container_class->add = overlay_add;
	container_class->remove = gedit_overlay_remove;
	container_class->forall = gedit_overlay_forall;
	container_class->child_type = gedit_overlay_child_type;

	g_object_class_install_property (object_class, PROP_MAIN_WIDGET,
	                                 g_param_spec_object ("main-widget",
	                                                      "Main Widget",
	                                                      "The Main Widget",
	                                                      GTK_TYPE_WIDGET,
	                                                      G_PARAM_READWRITE |
	                                                      G_PARAM_CONSTRUCT_ONLY |
	                                                      G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class, PROP_RELATIVE_WIDGET,
	                                 g_param_spec_object ("relative-widget",
	                                                      "Relative Widget",
	                                                      "Widget on which the floating widgets are placed",
	                                                      GTK_TYPE_WIDGET,
	                                                      G_PARAM_READWRITE |
	                                                      G_PARAM_CONSTRUCT_ONLY |
	                                                      G_PARAM_STATIC_STRINGS));

	g_type_class_add_private (object_class, sizeof (GeditOverlayPrivate));
}

static void
gedit_overlay_init (GeditOverlay *overlay)
{
	overlay->priv = GEDIT_OVERLAY_GET_PRIVATE (overlay);
}

/**
 * gedit_overlay_new:
 * @main_widget: a #GtkWidget
 * @relative_widget: (allow-none): a #Gtkwidget
 *
 * Creates a new #GeditOverlay. If @relative_widget is not %NULL the floating
 * widgets will be placed in relation to it, if not @main_widget will be use
 * for this purpose.
 *
 * Returns: a new #GeditOverlay object.
 */
GtkWidget *
gedit_overlay_new (GtkWidget *main_widget,
                   GtkWidget *relative_widget)
{
	g_return_val_if_fail (GTK_IS_WIDGET (main_widget), NULL);

	return GTK_WIDGET (g_object_new (GEDIT_TYPE_OVERLAY,
	                                 "main-widget", main_widget,
	                                 "relative-widget", relative_widget,
	                                 NULL));
}

/**
 * gedit_overlay_add:
 * @overlay: a #GeditOverlay
 * @widget: a #GtkWidget to be added to the container
 * @position: a #GeditOverlayChildPosition
 * @offset: offset for @widget
 *
 * Adds @widget to @overlay in a specific position.
 */
void
gedit_overlay_add (GeditOverlay             *overlay,
                   GtkWidget                *widget,
                   GeditOverlayChildPosition position,
                   guint                     offset)
{
	GeditOverlayChild *child;

	g_return_if_fail (GEDIT_IS_OVERLAY (overlay));
	g_return_if_fail (GTK_IS_WIDGET (widget));

	gtk_container_add (GTK_CONTAINER (overlay), widget);

	/* NOTE: can we improve this without exposing overlay child? */
	child = get_overlay_child (overlay, widget);
	g_assert (child != NULL);

	gedit_overlay_child_set_position (child, position);
	gedit_overlay_child_set_offset (child, offset);
}
