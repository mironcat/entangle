/*
 *  Entangle: Tethered Camera Control & Capture
 *
 *  Copyright (C) 2009-2018 Daniel P. Berrangé
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

#include <string.h>

#include "entangle-session-browser.h"

#include "entangle-debug.h"
#include "entangle-video.h"

struct _EntangleSessionBrowser
{
    GtkDrawingArea parent;

    EntangleSession *session;
    EntangleThumbnailLoader *loader;

    GdkRGBA background;
    GdkRGBA highlight;

    gulong sigMediaAdded;
    gulong sigMediaRemoved;
    gulong sigThumbReady;

    EntangleMedia *selectedMedia;
    gint selectedIndex;

    GtkAdjustment *hadjustment;
    GtkAdjustment *vadjustment;

    GdkPixbuf *imageIcon;
    GdkPixbuf *videoIcon;

    guint hscroll_policy : 1;
    guint vscroll_policy : 1;

    guint itemsize;
    guint binwidth;

    GdkWindow *bin_window;

    gint margin;

    gint dnd_start_x;
    gint dnd_start_y;
};

static void
entangle_session_browser_adjustment_changed(GtkAdjustment *adjustment,
                                            EntangleSessionBrowser *browser);

static void
entangle_session_browser_set_hadjustment(EntangleSessionBrowser *browser,
                                         GtkAdjustment *adjustment);
static void
entangle_session_browser_set_vadjustment(EntangleSessionBrowser *browser,
                                         GtkAdjustment *adjustment);

static void
entangle_session_browser_set_hadjustment_values(
    EntangleSessionBrowser *browser);
static void
entangle_session_browser_set_vadjustment_values(
    EntangleSessionBrowser *browser);

static gboolean
entangle_session_browser_draw(GtkWidget *widget, cairo_t *cr);

static void
entangle_session_browser_scroll_to_selected(EntangleSessionBrowser *browser);

G_DEFINE_TYPE_WITH_CODE(EntangleSessionBrowser,
                        entangle_session_browser,
                        GTK_TYPE_DRAWING_AREA,
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_SCROLLABLE, NULL))

enum
{
    PROP_O,
    PROP_SESSION,
    PROP_LOADER,
    PROP_HADJUSTMENT,
    PROP_VADJUSTMENT,
    PROP_HSCROLL_POLICY,
    PROP_VSCROLL_POLICY,
};

enum
{
    SIGNAL_SELECTION_CHANGED,

    SIGNAL_LAST,
};

static guint browser_signals[SIGNAL_LAST] = { 0 };

static void
entangle_session_browser_scroll_to_selected(EntangleSessionBrowser *browser)
{
    g_return_if_fail(ENTANGLE_IS_SESSION_BROWSER(browser));

    int minx, maxx;
    int x, y;
    GtkAllocation allocation;

    if (browser->selectedIndex == -1)
        return;

    if (!browser->bin_window)
        return;

    minx = browser->selectedIndex * (browser->margin + browser->itemsize);
    maxx = minx + browser->itemsize + (browser->margin * 2);

    gdk_window_get_position(browser->bin_window, &x, &y);
    gtk_widget_get_allocation(GTK_WIDGET(browser), &allocation);

    if ((x + minx) < 0) {
        gtk_adjustment_set_value(
            browser->hadjustment,
            gtk_adjustment_get_value(browser->hadjustment) + x + minx);
    } else if ((x + maxx) > allocation.width) {
        gtk_adjustment_set_value(
            browser->hadjustment,
            gtk_adjustment_get_value(browser->hadjustment) + x + maxx -
                allocation.width);
    }
}

static gboolean
entangle_session_browser_select_media(EntangleSessionBrowser *browser, int idx)
{
    EntangleMedia *media;

    if (idx == -1) {
        media = NULL;
    } else {
        media = entangle_session_get_media(browser->session, idx);
        if (!media) {
            idx = -1;
        }
    }
    if (idx != browser->selectedIndex || media != browser->selectedMedia) {
        if (browser->selectedMedia) {
            g_object_unref(browser->selectedMedia);
        }
        browser->selectedMedia = media;
        browser->selectedIndex = idx;
        if (media) {
            g_object_ref(browser->selectedMedia);
        }
        entangle_session_browser_scroll_to_selected(browser);
        g_signal_emit(browser, browser_signals[SIGNAL_SELECTION_CHANGED], 0);
        gtk_widget_queue_draw(GTK_WIDGET(browser));
    }

    return browser->selectedIndex != -1;
}

static void
do_thumb_loaded(EntanglePixbufLoader *loader G_GNUC_UNUSED,
                EntangleImage *image G_GNUC_UNUSED,
                gpointer data)
{
    EntangleSessionBrowser *browser = data;

    gtk_widget_queue_draw(GTK_WIDGET(browser));
}

static void
do_media_added(EntangleSession *session G_GNUC_UNUSED,
               EntangleMedia *media,
               gpointer data)
{
    EntangleSessionBrowser *browser = data;

    ENTANGLE_DEBUG("Request image %s for new image",
                   entangle_media_get_filename(media));
    if (ENTANGLE_IS_IMAGE(media))
        entangle_pixbuf_loader_load(ENTANGLE_PIXBUF_LOADER(browser->loader),
                                    ENTANGLE_IMAGE(media));

    entangle_session_browser_select_media(
        browser, entangle_session_get_media_count(browser->session) - 1);
    gtk_widget_queue_resize(GTK_WIDGET(browser));
}

static void
do_media_removed(EntangleSession *session G_GNUC_UNUSED,
                 EntangleMedia *media,
                 gpointer data)
{
    EntangleSessionBrowser *browser = data;

    ENTANGLE_DEBUG("Unrequest image %s for new image",
                   entangle_media_get_filename(media));
    if (ENTANGLE_IS_IMAGE(media))
        entangle_pixbuf_loader_unload(ENTANGLE_PIXBUF_LOADER(browser->loader),
                                      ENTANGLE_IMAGE(media));

    if (browser->selectedIndex >= 0) {
        media = entangle_session_get_media(browser->session,
                                           browser->selectedIndex);
        if (media != browser->selectedMedia) {
            entangle_session_browser_select_media(browser,
                                                  browser->selectedIndex - 1);
        }
    }

    gtk_widget_queue_resize(GTK_WIDGET(browser));
}

static void
do_model_unload(EntangleSessionBrowser *browser)
{
    g_return_if_fail(ENTANGLE_IS_SESSION_BROWSER(browser));

    int count;

    ENTANGLE_DEBUG("Unload model");

    g_signal_handler_disconnect(browser->session, browser->sigMediaAdded);
    g_signal_handler_disconnect(browser->session, browser->sigMediaRemoved);
    g_signal_handler_disconnect(browser->loader, browser->sigThumbReady);

    count = entangle_session_get_media_count(browser->session);
    for (int i = 0; i < count; i++) {
        EntangleMedia *media = entangle_session_get_media(browser->session, i);
        if (ENTANGLE_IS_IMAGE(media))
            entangle_pixbuf_loader_unload(
                ENTANGLE_PIXBUF_LOADER(browser->loader), ENTANGLE_IMAGE(media));
    }
    browser->selectedIndex = -1;
    if (browser->selectedMedia)
        g_object_unref(browser->selectedMedia);
    browser->selectedMedia = NULL;
}

static void
do_model_load(EntangleSessionBrowser *browser)
{
    g_return_if_fail(ENTANGLE_IS_SESSION_BROWSER(browser));

    int count;

    ENTANGLE_DEBUG("Load model");

    browser->sigMediaAdded =
        g_signal_connect(browser->session, "session-media-added",
                         G_CALLBACK(do_media_added), browser);
    browser->sigMediaRemoved =
        g_signal_connect(browser->session, "session-media-removed",
                         G_CALLBACK(do_media_removed), browser);
    browser->sigThumbReady = g_signal_connect(
        browser->loader, "pixbuf-loaded", G_CALLBACK(do_thumb_loaded), browser);

    count = entangle_session_get_media_count(browser->session);
    for (int i = 0; i < count; i++) {
        EntangleMedia *media = entangle_session_get_media(browser->session, i);

        if (ENTANGLE_IS_IMAGE(media))
            entangle_pixbuf_loader_load(ENTANGLE_PIXBUF_LOADER(browser->loader),
                                        ENTANGLE_IMAGE(media));
    }

    if (count) {
        entangle_session_browser_select_media(browser, count - 1);
    }
}

static void
entangle_session_browser_get_property(GObject *object,
                                      guint prop_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
    EntangleSessionBrowser *browser = ENTANGLE_SESSION_BROWSER(object);

    switch (prop_id) {
    case PROP_SESSION:
        g_value_set_object(value, browser->session);
        break;

    case PROP_LOADER:
        g_value_set_object(value, browser->loader);
        break;

    case PROP_HADJUSTMENT:
        g_value_set_object(value, browser->hadjustment);
        break;

    case PROP_VADJUSTMENT:
        g_value_set_object(value, browser->vadjustment);
        break;

    case PROP_HSCROLL_POLICY:
        g_value_set_enum(value, browser->hscroll_policy);
        break;

    case PROP_VSCROLL_POLICY:
        g_value_set_enum(value, browser->vscroll_policy);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
entangle_session_browser_set_property(GObject *object,
                                      guint prop_id,
                                      const GValue *value,
                                      GParamSpec *pspec)
{
    EntangleSessionBrowser *browser = ENTANGLE_SESSION_BROWSER(object);

    ENTANGLE_DEBUG("Set prop on session browser %d", prop_id);

    switch (prop_id) {
    case PROP_SESSION:
        entangle_session_browser_set_session(browser,
                                             g_value_get_object(value));
        break;

    case PROP_LOADER:
        entangle_session_browser_set_thumbnail_loader(
            browser, g_value_get_object(value));
        break;

    case PROP_HADJUSTMENT:
        entangle_session_browser_set_hadjustment(browser,
                                                 g_value_get_object(value));
        break;

    case PROP_VADJUSTMENT:
        entangle_session_browser_set_vadjustment(browser,
                                                 g_value_get_object(value));
        break;

    case PROP_HSCROLL_POLICY:
        browser->hscroll_policy = g_value_get_enum(value);
        gtk_widget_queue_resize(GTK_WIDGET(browser));
        break;

    case PROP_VSCROLL_POLICY:
        browser->vscroll_policy = g_value_get_enum(value);
        gtk_widget_queue_resize(GTK_WIDGET(browser));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
entangle_session_browser_realize(GtkWidget *widget)
{
    g_return_if_fail(ENTANGLE_IS_SESSION_BROWSER(widget));

    EntangleSessionBrowser *browser = ENTANGLE_SESSION_BROWSER(widget);
    GtkAllocation allocation;
    GdkWindow *window;
    GdkWindowAttr attributes;
    gint attributes_mask;

    gtk_widget_set_realized(widget, TRUE);

    gtk_widget_get_allocation(widget, &allocation);

    browser->itemsize = allocation.height - (browser->margin * 2);

    if (browser->session) {
        browser->binwidth =
            (browser->itemsize + browser->margin) *
                entangle_session_get_media_count(browser->session) +
            browser->margin;
    } else {
        browser->binwidth = allocation.width;
    }

    /* Make the main, clipping window */
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.x = allocation.x;
    attributes.y = allocation.y;
    attributes.width = allocation.width;
    attributes.height = allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.visual = gtk_widget_get_visual(widget);
    attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK;

    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

    window = gdk_window_new(gtk_widget_get_parent_window(widget), &attributes,
                            attributes_mask);
    gtk_widget_set_window(widget, window);
    gdk_window_set_user_data(window, widget);

    gtk_widget_get_allocation(widget, &allocation);

    /* Make the window for the icon view */
    attributes.x = 0;
    attributes.y = 0;
    attributes.width = browser->binwidth;
    attributes.height = browser->itemsize;
    attributes.event_mask =
        (GDK_EXPOSURE_MASK | GDK_SCROLL_MASK | GDK_POINTER_MOTION_MASK |
         GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK |
         GDK_KEY_RELEASE_MASK) |
        gtk_widget_get_events(widget);

    browser->bin_window = gdk_window_new(window, &attributes, attributes_mask);
    gdk_window_set_user_data(browser->bin_window, widget);

    gdk_window_show(browser->bin_window);
}

static void
entangle_session_browser_unrealize(GtkWidget *widget)
{
    g_return_if_fail(ENTANGLE_IS_SESSION_BROWSER(widget));

    EntangleSessionBrowser *browser = ENTANGLE_SESSION_BROWSER(widget);

    gdk_window_set_user_data(browser->bin_window, NULL);
    gdk_window_destroy(browser->bin_window);
    browser->bin_window = NULL;

    GTK_WIDGET_CLASS(entangle_session_browser_parent_class)->unrealize(widget);
}

static int
entangle_session_browser_get_index_at_coords(EntangleSessionBrowser *browser,
                                             gint x,
                                             gint y)
{
    int idx;
    int itemx;

    if (browser->session == NULL)
        return -1;

    idx = x / (browser->itemsize + browser->margin);
    if (idx >= entangle_session_get_media_count(browser->session))
        return -1;

    itemx = (idx * (browser->itemsize + browser->margin)) + browser->margin;
    if (x < itemx || y < browser->margin ||
        y > (browser->itemsize + browser->margin))
        return -1;

    return idx;
}

/**
 * entangle_session_browser_get_media_at_coords:
 * @browser: (transfer none): the session browser
 * @x: the horizontal co-ordinate
 * @y: the vertical co-ordinate
 *
 * Retrieve the media displayed at the co-ordinates (@x, @y)
 *
 * Returns: (transfer none): the media, or NULL
 */
EntangleMedia *
entangle_session_browser_get_media_at_coords(EntangleSessionBrowser *browser,
                                             gint x,
                                             gint y)
{
    g_return_val_if_fail(ENTANGLE_IS_SESSION_BROWSER(browser), NULL);

    int idx = entangle_session_browser_get_index_at_coords(browser, x, y);

    if (idx >= 0)
        return entangle_session_get_media(browser->session, idx);
    else
        return NULL;
}

static gboolean
entangle_session_browser_button_press(GtkWidget *widget, GdkEventButton *event)
{
    g_return_val_if_fail(ENTANGLE_IS_SESSION_BROWSER(widget), FALSE);

    EntangleSessionBrowser *browser = ENTANGLE_SESSION_BROWSER(widget);
    int idx;

    if (!browser->session)
        return FALSE;

    if (event->window != browser->bin_window)
        return FALSE;

    if (!gtk_widget_has_focus(widget))
        gtk_widget_grab_focus(widget);

    if (event->button == 1 && event->type == GDK_BUTTON_PRESS) {
        idx = entangle_session_browser_get_index_at_coords(browser, event->x,
                                                           event->y);

        if (idx >= 0 && entangle_session_browser_select_media(browser, idx)) {
            browser->dnd_start_x = event->x;
            browser->dnd_start_y = event->y;
        }
    }

    return event->button == 1;
}

static gboolean
entangle_session_browser_button_release(GtkWidget *widget,
                                        GdkEventButton *event)
{
    g_return_val_if_fail(ENTANGLE_IS_SESSION_BROWSER(widget), FALSE);

    EntangleSessionBrowser *browser = ENTANGLE_SESSION_BROWSER(widget);

    if (event->button == 1 && event->type == GDK_BUTTON_RELEASE) {
        browser->dnd_start_x = -1;
        browser->dnd_start_y = -1;
    }

    return event->button == 1;
}

static void
entangle_session_browser_convert_widget_to_bin_window_coords(
    EntangleSessionBrowser *browser,
    gint wx,
    gint wy,
    gint *bx,
    gint *by)
{
    gint x, y;

    g_return_if_fail(ENTANGLE_SESSION_BROWSER(browser));

    if (browser->bin_window)
        gdk_window_get_position(browser->bin_window, &x, &y);
    else
        x = y = 0;

    if (bx)
        *bx = wx - x;
    if (by)
        *by = wy - y;
}

static gboolean
entangle_session_browser_query_tooltip(GtkWidget *widget,
                                       gint x,
                                       gint y,
                                       gboolean keyboard_mode G_GNUC_UNUSED,
                                       GtkTooltip *tooltip,
                                       gpointer user_data G_GNUC_UNUSED)
{
    g_return_val_if_fail(ENTANGLE_IS_SESSION_BROWSER(widget), FALSE);

    EntangleSessionBrowser *browser = ENTANGLE_SESSION_BROWSER(widget);
    gint bx = 0;
    gint by = 0;
    GdkRectangle rect;
    EntangleMedia *media;
    const gchar *filename;

    entangle_session_browser_convert_widget_to_bin_window_coords(browser, x, y,
                                                                 &bx, &by);

    media = entangle_session_browser_get_media_at_coords(browser, bx, by);

    if (media == NULL) {
        return FALSE;
    }

    rect.x = x - (x % (browser->itemsize + browser->margin));
    rect.y = browser->margin;
    rect.height = browser->itemsize;
    rect.width = browser->itemsize;

    filename = entangle_media_get_filename(media);
    if (filename) {
        gtk_tooltip_set_text(tooltip, filename);
        gtk_tooltip_set_tip_area(tooltip, &rect);
        return TRUE;
    }

    return FALSE;
}

static gboolean
entangle_session_browser_scroll(GtkWidget *widget, GdkEventScroll *event)
{
    g_return_val_if_fail(ENTANGLE_IS_SESSION_BROWSER(widget), FALSE);

    EntangleSessionBrowser *browser = ENTANGLE_SESSION_BROWSER(widget);

    switch (event->direction) {
    case GDK_SCROLL_UP:
    case GDK_SCROLL_LEFT:
        gtk_adjustment_set_value(
            browser->hadjustment,
            gtk_adjustment_get_value(browser->hadjustment) -
                gtk_adjustment_get_step_increment(browser->hadjustment));
        break;
    case GDK_SCROLL_DOWN:
    case GDK_SCROLL_RIGHT:
        gtk_adjustment_set_value(
            browser->hadjustment,
            gtk_adjustment_get_value(browser->hadjustment) +
                gtk_adjustment_get_step_increment(browser->hadjustment));
        break;

    case GDK_SCROLL_SMOOTH:
    default:
        break;
    }

    return TRUE;
}

static gboolean
entangle_session_browser_motion_notify(GtkWidget *widget, GdkEventMotion *event)
{
    g_return_val_if_fail(ENTANGLE_IS_SESSION_BROWSER(widget), FALSE);

    EntangleSessionBrowser *browser = ENTANGLE_SESSION_BROWSER(widget);
    GdkPixbuf *pixbuf = NULL;
    gchar *demo = g_strdup("demo");
    GtkTargetEntry tentry[] = { {
        demo,
        GTK_TARGET_SAME_APP,
        0,
    } };
    GtkTargetList *tlist;
    GdkDragContext *ctx;
    EntangleMedia *media;
    gboolean ret = FALSE;

    if (browser->dnd_start_y == -1 || browser->dnd_start_x == -1)
        goto cleanup;

    if (!gtk_drag_check_threshold(widget, browser->dnd_start_x,
                                  browser->dnd_start_y, event->x, event->y)) {
        ret = TRUE;
        goto cleanup;
    }

    media = entangle_session_browser_get_media_at_coords(
        browser, browser->dnd_start_x, browser->dnd_start_y);

    if (!media) {
        browser->dnd_start_x = browser->dnd_start_y = -1;
        goto cleanup;
    }

    if (ENTANGLE_IS_IMAGE(media)) {
        pixbuf = entangle_pixbuf_loader_get_pixbuf(
            ENTANGLE_PIXBUF_LOADER(browser->loader), ENTANGLE_IMAGE(media));

        if (!pixbuf) {
            browser->dnd_start_x = browser->dnd_start_y = -1;
            goto cleanup;
        }
    }

    tlist = gtk_target_list_new(tentry, G_N_ELEMENTS(tentry));

    ctx = gtk_drag_begin_with_coordinates(widget, tlist, GDK_ACTION_PRIVATE, 1,
                                          (GdkEvent *)event, -1, -1);

    browser->dnd_start_x = browser->dnd_start_y = -1;
    if (pixbuf)
        gtk_drag_set_icon_pixbuf(ctx, pixbuf, 0, 0);

    gtk_target_list_unref(tlist);

    ret = TRUE;
cleanup:
    g_free(demo);
    return ret;
}

static gboolean
entangle_session_browser_key_press(GtkWidget *widget, GdkEventKey *event)
{
    g_return_val_if_fail(ENTANGLE_IS_SESSION_BROWSER(widget), FALSE);

    EntangleSessionBrowser *browser = ENTANGLE_SESSION_BROWSER(widget);

    switch (event->keyval) {
    case GDK_KEY_Right:
        if (browser->selectedIndex != -1 &&
            ((browser->selectedIndex + 1) <
             entangle_session_get_media_count(browser->session))) {
            entangle_session_browser_select_media(browser,
                                                  browser->selectedIndex + 1);
        }
        return TRUE;

    case GDK_KEY_Left:
        if (browser->selectedIndex > 0) {
            entangle_session_browser_select_media(browser,
                                                  browser->selectedIndex - 1);
        }
        return TRUE;

    default:
        return GTK_WIDGET_CLASS(entangle_session_browser_parent_class)
            ->key_press_event(widget, event);
    }
}

static void
entangle_session_browser_size_allocate(GtkWidget *widget,
                                       GtkAllocation *allocation)
{
    g_return_if_fail(ENTANGLE_IS_SESSION_BROWSER(widget));

    EntangleSessionBrowser *browser = ENTANGLE_SESSION_BROWSER(widget);

    gtk_widget_set_allocation(widget, allocation);

    browser->itemsize = allocation->height - (browser->margin * 2);

    if (browser->session) {
        browser->binwidth =
            (browser->itemsize + browser->margin) *
                entangle_session_get_media_count(browser->session) +
            browser->margin;
    } else {
        browser->binwidth = allocation->width;
    }

    if (gtk_widget_get_realized(widget)) {
        gdk_window_move_resize(gtk_widget_get_window(widget), allocation->x,
                               allocation->y, allocation->width,
                               allocation->height);
        gdk_window_resize(browser->bin_window, browser->binwidth,
                          allocation->height);
    }

    /* Delay signal emission */
    g_object_freeze_notify(G_OBJECT(browser->hadjustment));
    g_object_freeze_notify(G_OBJECT(browser->vadjustment));

    entangle_session_browser_set_hadjustment_values(browser);
    entangle_session_browser_set_vadjustment_values(browser);

    if (gtk_widget_get_realized(widget) && browser->selectedIndex != -1) {
        entangle_session_browser_scroll_to_selected(browser);
    }

    /* Emit any pending signals now */
    g_object_thaw_notify(G_OBJECT(browser->hadjustment));
    g_object_thaw_notify(G_OBJECT(browser->vadjustment));
}

static void
entangle_session_browser_destroy(GtkWidget *widget)
{
    g_return_if_fail(ENTANGLE_IS_SESSION_BROWSER(widget));

    EntangleSessionBrowser *browser = ENTANGLE_SESSION_BROWSER(widget);

    if (browser->hadjustment != NULL) {
        g_object_unref(browser->hadjustment);
        browser->hadjustment = NULL;
    }

    if (browser->vadjustment != NULL) {
        g_object_unref(browser->vadjustment);
        browser->vadjustment = NULL;
    }

    GTK_WIDGET_CLASS(entangle_session_browser_parent_class)->destroy(widget);
}

static void
entangle_session_browser_finalize(GObject *object)
{
    EntangleSessionBrowser *browser = ENTANGLE_SESSION_BROWSER(object);

    if (browser->session && browser->loader)
        do_model_unload(browser);

    if (browser->session)
        g_object_unref(browser->session);
    if (browser->loader)
        g_object_unref(browser->loader);
    if (browser->imageIcon)
        g_object_unref(browser->imageIcon);
    if (browser->videoIcon)
        g_object_unref(browser->videoIcon);

    G_OBJECT_CLASS(entangle_session_browser_parent_class)->finalize(object);
}

static void
entangle_session_browser_class_init(EntangleSessionBrowserClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->finalize = entangle_session_browser_finalize;
    object_class->get_property = entangle_session_browser_get_property;
    object_class->set_property = entangle_session_browser_set_property;

    widget_class->destroy = entangle_session_browser_destroy;
    widget_class->realize = entangle_session_browser_realize;
    widget_class->unrealize = entangle_session_browser_unrealize;
    widget_class->draw = entangle_session_browser_draw;
    widget_class->button_press_event = entangle_session_browser_button_press;
    widget_class->button_release_event =
        entangle_session_browser_button_release;
    widget_class->scroll_event = entangle_session_browser_scroll;
    widget_class->motion_notify_event = entangle_session_browser_motion_notify;
    widget_class->key_press_event = entangle_session_browser_key_press;
    widget_class->size_allocate = entangle_session_browser_size_allocate;

    g_object_class_install_property(
        object_class, PROP_SESSION,
        g_param_spec_object("session", "Session", "Session to be displayed",
                            ENTANGLE_TYPE_SESSION,
                            G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    g_object_class_install_property(
        object_class, PROP_LOADER,
        g_param_spec_object("thumbnail-loader", "Thumbnail loader",
                            "Thumbnail loader", ENTANGLE_TYPE_THUMBNAIL_LOADER,
                            G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    /* Scrollable interface properties */
    g_object_class_override_property(object_class, PROP_HADJUSTMENT,
                                     "hadjustment");
    g_object_class_override_property(object_class, PROP_VADJUSTMENT,
                                     "vadjustment");
    g_object_class_override_property(object_class, PROP_HSCROLL_POLICY,
                                     "hscroll-policy");
    g_object_class_override_property(object_class, PROP_VSCROLL_POLICY,
                                     "vscroll-policy");

    browser_signals[SIGNAL_SELECTION_CHANGED] =
        g_signal_new("selection-changed", G_TYPE_FROM_CLASS(object_class),
                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
                     g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

EntangleSessionBrowser *
entangle_session_browser_new(void)
{
    return ENTANGLE_SESSION_BROWSER(
        g_object_new(ENTANGLE_TYPE_SESSION_BROWSER, NULL));
}

static void
entangle_session_browser_load_icons(EntangleSessionBrowser *browser)
{
    GtkIconTheme *theme;

    theme = gtk_icon_theme_get_default();
    if (browser->imageIcon)
        g_object_unref(browser->imageIcon);
    browser->imageIcon =
        gtk_icon_theme_load_icon(theme, "camera-photo-symbolic", 256, 0, NULL);
    if (browser->videoIcon)
        g_object_unref(browser->videoIcon);
    browser->videoIcon =
        gtk_icon_theme_load_icon(theme, "camera-video-symbolic", 256, 0, NULL);
}

static void
entangle_session_browser_init(EntangleSessionBrowser *browser)
{
    gtk_widget_set_has_tooltip(GTK_WIDGET(browser), TRUE);

    g_signal_connect(GTK_WIDGET(browser), "query-tooltip",
                     G_CALLBACK(entangle_session_browser_query_tooltip), NULL);

    gtk_widget_queue_resize(GTK_WIDGET(browser));

    browser->margin = 6;

    gtk_widget_set_can_focus(GTK_WIDGET(browser), TRUE);

    browser->dnd_start_x = browser->dnd_start_y = -1;

    entangle_session_browser_load_icons(browser);
    g_signal_connect(GTK_WIDGET(browser), "style-updated",
                     G_CALLBACK(entangle_session_browser_load_icons), NULL);
}

/**
 * entangle_session_browser_get_selected_media:
 * @browser: (transfer none): the session browser
 *
 * Returns: (transfer full): the selected media or NULL
 */
EntangleMedia *
entangle_session_browser_get_selected_media(EntangleSessionBrowser *browser)
{
    g_return_val_if_fail(ENTANGLE_IS_SESSION_BROWSER(browser), NULL);

    if (browser->selectedMedia == NULL)
        return NULL;

    g_object_ref(browser->selectedMedia);
    return browser->selectedMedia;
}

/**
 * entangle_session_browser_get_earlier_images:
 * @browser: (transfer none): the session browser
 * @include_selected: true to include the current image in the list
 * @count: maximum number of images to return
 *
 * Get a list of images prior to the currently selected image.
 * If @include_selected is true, then the currently selected
 * image will be included in the returned list
 *
 * Returns: (transfer full)(element-type EntangleImage): the list of images
 */
GList *
entangle_session_browser_get_earlier_images(EntangleSessionBrowser *browser,
                                            gboolean include_selected,
                                            gsize count)
{
    g_return_val_if_fail(ENTANGLE_IS_SESSION_BROWSER(browser), NULL);

    GList *images = NULL;
    int idx;

    if (browser->selectedIndex < 0)
        return NULL;

    idx = browser->selectedIndex;
    if (!include_selected)
        idx--;

    while (idx >= 0 && count > 0) {
        EntangleMedia *media =
            entangle_session_get_media(browser->session, idx);
        if (ENTANGLE_IS_IMAGE(media)) {
            g_object_ref(media);
            images = g_list_append(images, media);
            count--;
        }
        idx--;
    }
    return images;
}

/**
 * entangle_session_browser_set_thumbnail_loader:
 * @browser: (transfer none): the session browser
 * @loader: (transfer none): the thumbnail loader to use
 *
 * Set the thumbnail loader to use for generating image
 * thumbnails
 */
void
entangle_session_browser_set_thumbnail_loader(EntangleSessionBrowser *browser,
                                              EntangleThumbnailLoader *loader)
{
    g_return_if_fail(ENTANGLE_IS_SESSION_BROWSER(browser));

    if (browser->loader) {
        if (browser->session)
            do_model_unload(browser);

        g_object_unref(browser->loader);
    }
    browser->loader = loader;
    if (browser->loader) {
        g_object_ref(browser->loader);

        if (browser->session)
            do_model_load(browser);
    }
}

/**
 * entangle_session_browser_get_thumbnail_loader:
 * @browser: the session browser
 *
 * Get the thumbnail loader used by the session browser
 *
 * Returns: (transfer none): the session browser
 */
EntangleThumbnailLoader *
entangle_session_browser_get_thumbnail_loader(EntangleSessionBrowser *browser)
{
    g_return_val_if_fail(ENTANGLE_IS_SESSION_BROWSER(browser), NULL);

    return browser->loader;
}

/**
 * entangle_session_browser_set_session:
 * @browser: (transfer none): the session browser
 * @session: (transfer none): the session to display
 *
 * Set the session to be displayed
 */
void
entangle_session_browser_set_session(EntangleSessionBrowser *browser,
                                     EntangleSession *session)
{
    g_return_if_fail(ENTANGLE_IS_SESSION_BROWSER(browser));

    if (browser->session) {
        if (browser->loader)
            do_model_unload(browser);
        g_object_unref(browser->session);
    }
    browser->session = session;
    if (browser->session) {
        g_object_ref(browser->session);

        if (browser->loader)
            do_model_load(browser);
    }
}

/**
 * entangle_session_browser_get_session:
 * @browser: (transfer none): the session browser
 *
 * Get the session being displayed
 *
 * Returns: (transfer none): the session
 */
EntangleSession *
entangle_session_browser_get_session(EntangleSessionBrowser *browser)
{
    g_return_val_if_fail(ENTANGLE_IS_SESSION_BROWSER(browser), NULL);

    return browser->session;
}

void
entangle_session_browser_set_background(EntangleSessionBrowser *browser,
                                        const gchar *background)
{
    g_return_if_fail(ENTANGLE_IS_SESSION_BROWSER(browser));

    gdk_rgba_parse(&browser->background, background);

    GtkWidget *widget = GTK_WIDGET(browser);
    gtk_widget_queue_draw(widget);
}

gchar *
entangle_session_browser_get_background(EntangleSessionBrowser *browser)
{
    g_return_val_if_fail(ENTANGLE_IS_SESSION_BROWSER(browser), NULL);

    return gdk_rgba_to_string(&browser->background);
}

void
entangle_session_browser_set_highlight(EntangleSessionBrowser *browser,
                                       const gchar *highlight)
{
    g_return_if_fail(ENTANGLE_IS_SESSION_BROWSER(browser));

    gdk_rgba_parse(&browser->highlight, highlight);

    GtkWidget *widget = GTK_WIDGET(browser);
    gtk_widget_queue_draw(widget);
}

gchar *
entangle_session_browser_get_highlight(EntangleSessionBrowser *browser)
{
    g_return_val_if_fail(ENTANGLE_IS_SESSION_BROWSER(browser), NULL);

    return gdk_rgba_to_string(&browser->highlight);
}

static gboolean
entangle_session_browser_draw(GtkWidget *widget, cairo_t *cr)
{
    g_return_val_if_fail(ENTANGLE_IS_SESSION_BROWSER(widget), FALSE);

    EntangleSessionBrowser *browser = ENTANGLE_SESSION_BROWSER(widget);
    int ww, wh; /* Available drawing area extents */
    int i;

    ww = gdk_window_get_width(gtk_widget_get_window(widget));
    wh = gdk_window_get_height(gtk_widget_get_window(widget));

    cairo_set_source_rgb(cr, browser->background.red, browser->background.green,
                         browser->background.blue);
    cairo_rectangle(cr, 0, 0, ww, wh);
    cairo_fill(cr);

    if (!gtk_cairo_should_draw_window(cr, browser->bin_window))
        return FALSE;

    cairo_save(cr);
    gtk_cairo_transform_to_window(cr, widget, browser->bin_window);
    cairo_set_line_width(cr, 1.);

    if (!browser->session) {
        cairo_restore(cr);
        return TRUE;
    }

    for (i = 0; i < entangle_session_get_media_count(browser->session); i++) {
        EntangleMedia *media = entangle_session_get_media(browser->session, i);
        GdkRectangle paint_area;

        paint_area.x =
            (browser->itemsize + browser->margin) * i + browser->margin;
        paint_area.y = browser->margin;
        paint_area.width = browser->itemsize;
        paint_area.height = browser->itemsize;

        if (media == browser->selectedMedia) {
            cairo_save(cr);
            cairo_set_source_rgba(cr, browser->highlight.red,
                                  browser->highlight.green,
                                  browser->highlight.blue, 1);
            cairo_rectangle(cr, paint_area.x, paint_area.y, paint_area.width,
                            paint_area.height);
            cairo_fill(cr);
            cairo_restore(cr);
        }

        cairo_save(cr);
        cairo_rectangle(cr, paint_area.x, paint_area.y, paint_area.width,
                        paint_area.height);
        cairo_clip(cr);

        if (gdk_cairo_get_clip_rectangle(cr, NULL)) {
            GdkPixbuf *pixbuf = NULL;
            gint itemsize = browser->itemsize;
            gdouble offset = 0;
            if (ENTANGLE_IS_IMAGE(media)) {
                pixbuf = entangle_pixbuf_loader_get_pixbuf(
                    ENTANGLE_PIXBUF_LOADER(browser->loader),
                    ENTANGLE_IMAGE(media));
                if (!pixbuf) {
                    pixbuf = browser->imageIcon;
                    itemsize /= 2;
                    offset = itemsize / 2.0;
                } else {
                    itemsize -= 2;
                    offset = 1;
                }
            } else if (ENTANGLE_IS_VIDEO(media)) {
                pixbuf = browser->videoIcon;
                itemsize /= 2;
                offset = itemsize / 2.0;
            }
            if (pixbuf) {
                gint pw = gdk_pixbuf_get_width(pixbuf);
                gint ph = gdk_pixbuf_get_height(pixbuf);
                double sx = itemsize / (double)pw;
                double sy = itemsize / (double)ph;

                cairo_scale(cr, sx, sy);
                gdk_cairo_set_source_pixbuf(cr, pixbuf,
                                            (paint_area.x + offset) / sx,
                                            (paint_area.y + offset) / sy);
                cairo_rectangle(cr, paint_area.x / sx, paint_area.y / sy,
                                paint_area.width / sx, paint_area.height / sy);
                cairo_fill(cr);
            } else {
                cairo_save(cr);
                cairo_set_source_rgba(cr, browser->highlight.red,
                                      browser->highlight.green,
                                      browser->highlight.blue, 0.2);
                cairo_rectangle(cr, paint_area.x, paint_area.y,
                                paint_area.width, paint_area.height);
                cairo_fill(cr);
            }
        }

        cairo_restore(cr);
    }

    cairo_restore(cr);

    return TRUE;
}

static void
entangle_session_browser_set_hadjustment_values(EntangleSessionBrowser *browser)
{
    g_return_if_fail(ENTANGLE_IS_SESSION_BROWSER(browser));

    GtkAllocation allocation;
    GtkAdjustment *adj = browser->hadjustment;

    gtk_widget_get_allocation(GTK_WIDGET(browser), &allocation);

    gtk_adjustment_configure(adj, gtk_adjustment_get_value(adj), 0.0,
                             browser->binwidth, allocation.width * 0.1,
                             allocation.width * 0.9, allocation.width);
}

static void
entangle_session_browser_set_vadjustment_values(EntangleSessionBrowser *browser)
{
    g_return_if_fail(ENTANGLE_IS_SESSION_BROWSER(browser));

    GtkAllocation allocation;
    GtkAdjustment *adj = browser->vadjustment;

    gtk_widget_get_allocation(GTK_WIDGET(browser), &allocation);

    gtk_adjustment_configure(adj, gtk_adjustment_get_value(adj), 0.0,
                             allocation.height, allocation.height * 0.1,
                             allocation.height * 0.9, allocation.height);
}

static void
entangle_session_browser_set_hadjustment(EntangleSessionBrowser *browser,
                                         GtkAdjustment *adjustment)
{
    g_return_if_fail(ENTANGLE_IS_SESSION_BROWSER(browser));

    if (adjustment && browser->hadjustment == adjustment)
        return;

    if (browser->hadjustment != NULL) {
        g_signal_handlers_disconnect_matched(browser->hadjustment,
                                             G_SIGNAL_MATCH_DATA, 0, 0, NULL,
                                             NULL, browser);
        g_object_unref(browser->hadjustment);
    }

    if (!adjustment)
        adjustment = gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

    g_signal_connect(adjustment, "value-changed",
                     G_CALLBACK(entangle_session_browser_adjustment_changed),
                     browser);
    browser->hadjustment = g_object_ref_sink(adjustment);
    entangle_session_browser_set_hadjustment_values(browser);

    g_object_notify(G_OBJECT(browser), "hadjustment");
}

static void
entangle_session_browser_set_vadjustment(EntangleSessionBrowser *browser,
                                         GtkAdjustment *adjustment)
{
    g_return_if_fail(ENTANGLE_IS_SESSION_BROWSER(browser));

    if (adjustment && browser->vadjustment == adjustment)
        return;

    if (browser->vadjustment != NULL) {
        g_signal_handlers_disconnect_matched(browser->vadjustment,
                                             G_SIGNAL_MATCH_DATA, 0, 0, NULL,
                                             NULL, browser);
        g_object_unref(browser->vadjustment);
    }

    if (!adjustment)
        adjustment = gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

    g_signal_connect(adjustment, "value-changed",
                     G_CALLBACK(entangle_session_browser_adjustment_changed),
                     browser);
    browser->vadjustment = g_object_ref_sink(adjustment);
    entangle_session_browser_set_vadjustment_values(browser);

    g_object_notify(G_OBJECT(browser), "vadjustment");
}

static void
entangle_session_browser_adjustment_changed(GtkAdjustment *adjustment
                                                G_GNUC_UNUSED,
                                            EntangleSessionBrowser *browser)
{
    g_return_if_fail(ENTANGLE_IS_SESSION_BROWSER(browser));

    if (gtk_widget_get_realized(GTK_WIDGET(browser))) {
        gdk_window_move(browser->bin_window,
                        -gtk_adjustment_get_value(browser->hadjustment),
                        -gtk_adjustment_get_value(browser->vadjustment));
    }
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
