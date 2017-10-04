/*
 *  Entangle: Tethered Camera Control & Capture
 *
 *  Copyright (C) 2009-2017 Daniel P. Berrange
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <unistd.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <gdk/gdkkeysyms.h>

#include "entangle-debug.h"
#include "entangle-media-popup.h"
#include "entangle-image-display.h"

gboolean do_popup_delete(GtkWidget *src,
                         GdkEvent *ev,
                         gpointer data);

typedef struct _EntangleMediaPopupPrivate EntangleMediaPopupPrivate;
struct _EntangleMediaPopupPrivate {
    EntangleMedia *media;
    EntangleImageDisplay *imageDisplay;
};

G_DEFINE_TYPE_WITH_PRIVATE(EntangleMediaPopup, entangle_media_popup, GTK_TYPE_WINDOW);

enum {
    PROP_0,
    PROP_MEDIA,
};

static void entangle_media_popup_get_property(GObject *object,
                                              guint prop_id,
                                              GValue *value,
                                              GParamSpec *pspec)
{
    EntangleMediaPopup *popup = ENTANGLE_MEDIA_POPUP(object);
    EntangleMediaPopupPrivate *priv = entangle_media_popup_get_instance_private(popup);

    switch (prop_id)
        {
        case PROP_MEDIA:
            g_value_set_object(value, priv->media);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        }
}

static void entangle_media_popup_set_property(GObject *object,
                                              guint prop_id,
                                              const GValue *value,
                                              GParamSpec *pspec)
{
    EntangleMediaPopup *popup = ENTANGLE_MEDIA_POPUP(object);
    EntangleMediaPopupPrivate *priv = entangle_media_popup_get_instance_private(popup);

    ENTANGLE_DEBUG("Set prop on media popup %d", prop_id);

    switch (prop_id)
        {
        case PROP_MEDIA: {
            if (priv->media)
                g_object_unref(priv->media);
            priv->media = g_value_dup_object(value);

            if (priv->media && ENTANGLE_IS_IMAGE(priv->media))
                entangle_image_display_set_image(priv->imageDisplay, ENTANGLE_IMAGE(priv->media));
            else
                entangle_image_display_set_image(priv->imageDisplay, NULL);
        } break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        }
}


static void entangle_media_popup_finalize(GObject *object)
{
    EntangleMediaPopup *popup = ENTANGLE_MEDIA_POPUP(object);
    EntangleMediaPopupPrivate *priv = entangle_media_popup_get_instance_private(popup);

    ENTANGLE_DEBUG("Remove popup");

    if (priv->media)
        g_object_unref(priv->media);

    G_OBJECT_CLASS(entangle_media_popup_parent_class)->finalize(object);
}


static gboolean entangle_media_popup_button_press(GtkWidget *widget,
                                                  GdkEventButton *ev,
                                                  gpointer data G_GNUC_UNUSED)
{
    g_return_val_if_fail(ENTANGLE_IS_MEDIA_POPUP(widget), FALSE);

    EntangleMediaPopup *popup = ENTANGLE_MEDIA_POPUP(widget);
    int w, h;

    gtk_window_get_size(GTK_WINDOW(popup), &w, &h);

    if (ev->button == 1) {
        gtk_window_begin_move_drag(GTK_WINDOW(popup), ev->button, ev->x_root, ev->y_root, ev->time);
    } else if (ev->button == 2 || ev->button == 3) {
        GdkWindowEdge edge;
        if (ev->x > (w/2)) {
            if (ev->y > (h/2))
                edge = GDK_WINDOW_EDGE_SOUTH_EAST;
            else
                edge = GDK_WINDOW_EDGE_NORTH_EAST;
        } else {
            if (ev->y > (h/2))
                edge = GDK_WINDOW_EDGE_SOUTH_WEST;
            else
                edge = GDK_WINDOW_EDGE_NORTH_WEST;
        }
        gtk_window_begin_resize_drag(GTK_WINDOW(popup), edge, ev->button, ev->x_root, ev->y_root, ev->time);
    } else {
        return FALSE;
    }

    return TRUE;
}

static gboolean entangle_media_popup_key_release(GtkWidget *widget,
                                                 GdkEventKey *ev,
                                                 gpointer data G_GNUC_UNUSED)
{
    g_return_val_if_fail(ENTANGLE_IS_MEDIA_POPUP(widget), FALSE);

    if (ev->keyval == GDK_KEY_Escape ||
        ev->keyval == GDK_KEY_KP_Enter ||
        ev->keyval == GDK_KEY_Return) {
        g_signal_emit_by_name(widget, "popup-close", NULL);
        gtk_widget_hide(widget);
        return TRUE;
    }

    return FALSE;
}

static void entangle_media_popup_class_init(EntangleMediaPopupClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = entangle_media_popup_finalize;
    object_class->get_property = entangle_media_popup_get_property;
    object_class->set_property = entangle_media_popup_set_property;

    g_object_class_install_property(object_class,
                                    PROP_MEDIA,
                                    g_param_spec_object("media",
                                                        "Media",
                                                        "Media to be displayed",
                                                        ENTANGLE_TYPE_MEDIA,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

    g_signal_new("popup-close",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE,
                 0);

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass),
                                                "/org/entangle_photo/Manager/frontend/entangle-media-popup.ui");
}


EntangleMediaPopup *entangle_media_popup_new(void)
{
    return ENTANGLE_MEDIA_POPUP(g_object_new(ENTANGLE_TYPE_MEDIA_POPUP, NULL));
}

gboolean do_popup_delete(GtkWidget *src,
                         GdkEvent *ev G_GNUC_UNUSED,
                         gpointer data G_GNUC_UNUSED)
{
    g_return_val_if_fail(ENTANGLE_IS_MEDIA_POPUP(src), FALSE);

    ENTANGLE_DEBUG("popup delete");

    g_signal_emit_by_name(src, "popup-close", NULL);
    gtk_widget_hide(src);
    return TRUE;
}


static void entangle_media_popup_init(EntangleMediaPopup *popup)
{
    EntangleMediaPopupPrivate *priv = entangle_media_popup_get_instance_private(popup);

    gtk_widget_init_template(GTK_WIDGET(popup));

    g_signal_connect(popup, "button-press-event",
                     G_CALLBACK(entangle_media_popup_button_press), NULL);
    g_signal_connect(popup, "key-release-event",
                     G_CALLBACK(entangle_media_popup_key_release), NULL);

    priv->imageDisplay = entangle_image_display_new();
    gtk_container_add(GTK_CONTAINER(popup), GTK_WIDGET(priv->imageDisplay));

    g_signal_connect(popup, "delete-event", G_CALLBACK(do_popup_delete), NULL);
}


void entangle_media_popup_show(EntangleMediaPopup *popup,
                               GtkWindow *parent,
                               int x, int y)
{
    g_return_if_fail(ENTANGLE_IS_MEDIA_POPUP(popup));
    g_return_if_fail(GTK_IS_WINDOW(parent));

    EntangleMediaPopupPrivate *priv = entangle_media_popup_get_instance_private(popup);

    gtk_widget_realize(GTK_WIDGET(popup));

    gtk_window_set_transient_for(GTK_WINDOW(popup), parent);

    gtk_widget_show(GTK_WIDGET(popup));
    gtk_window_move(GTK_WINDOW(popup), x, y);
    gtk_widget_show(GTK_WIDGET(priv->imageDisplay));
    gtk_window_present(GTK_WINDOW(popup));
}


void entangle_media_popup_move_to_monitor(EntangleMediaPopup *popup, gint monitorIdx)
{
    g_return_if_fail(ENTANGLE_IS_MEDIA_POPUP(popup));

#if GTK_CHECK_VERSION(3, 22, 0)
    GdkDisplay *display = gtk_widget_get_display(GTK_WIDGET(popup));
    GdkMonitor *monitor = gdk_display_get_monitor(display, monitorIdx);
#else
    GdkScreen *screen = gtk_window_get_screen(GTK_WINDOW(popup));
#endif
    GdkRectangle r;

#if GTK_CHECK_VERSION(3, 22, 0)
    gdk_monitor_get_geometry(monitor, &r);
#else
    gdk_screen_get_monitor_geometry(screen, monitorIdx, &r);
#endif

    ENTANGLE_DEBUG("At %d,%d Size %d,%d", r.x, r.y, r.width, r.height);

    gtk_window_move(GTK_WINDOW(popup), r.x, r.y);
    gtk_window_resize(GTK_WINDOW(popup), r.width, r.height);
    gtk_window_fullscreen(GTK_WINDOW(popup));
}


void entangle_media_popup_show_on_monitor(EntangleMediaPopup *popup, gint monitor)
{
    g_return_if_fail(ENTANGLE_IS_MEDIA_POPUP(popup));

    EntangleMediaPopupPrivate *priv = entangle_media_popup_get_instance_private(popup);
    GdkCursor *null_cursor = gdk_cursor_new_for_display(gdk_display_get_default(),
                                                        GDK_BLANK_CURSOR);

    gtk_widget_realize(GTK_WIDGET(popup));

    gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(popup)),
                          null_cursor);
    g_object_unref(null_cursor);

    entangle_media_popup_move_to_monitor(popup, monitor);

    gtk_widget_show(GTK_WIDGET(popup));
    gtk_widget_show(GTK_WIDGET(priv->imageDisplay));
    gtk_window_present(GTK_WINDOW(popup));
}


/**
 * entangle_media_popup_set_media:
 * @popup: (transfer none): the popup widget
 * @media: (transfer none)(allow-none): the media to display, or NULL
 *
 * Set the media to be displayed by the popup
 */
void entangle_media_popup_set_media(EntangleMediaPopup *popup, EntangleMedia *media)
{
    g_return_if_fail(ENTANGLE_IS_MEDIA_POPUP(popup));
    g_return_if_fail(!media || ENTANGLE_IS_MEDIA(media));

    g_object_set(popup, "media", media, NULL);
}


/**
 * entangle_media_popup_get_media:
 * @popup: (transfer none): the popup widget
 *
 * Retrieve the media that the popup is currently displaying
 *
 * Returns: (transfer none): the media displayed
 */
EntangleMedia *entangle_media_popup_get_media(EntangleMediaPopup *popup)
{
    g_return_val_if_fail(ENTANGLE_IS_MEDIA_POPUP(popup), NULL);

    EntangleMediaPopupPrivate *priv = entangle_media_popup_get_instance_private(popup);
    return priv->media;
}


void entangle_media_popup_set_background(EntangleMediaPopup *popup,
                                         const gchar *background)
{
    g_return_if_fail(ENTANGLE_IS_MEDIA_POPUP(popup));

    EntangleMediaPopupPrivate *priv = entangle_media_popup_get_instance_private(popup);

    entangle_image_display_set_background(priv->imageDisplay, background);
}

gchar *entangle_media_popup_get_background(EntangleMediaPopup *popup)
{
    g_return_val_if_fail(ENTANGLE_IS_MEDIA_POPUP(popup), NULL);

    EntangleMediaPopupPrivate *priv = entangle_media_popup_get_instance_private(popup);

    return entangle_image_display_get_background(priv->imageDisplay);
}



/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
