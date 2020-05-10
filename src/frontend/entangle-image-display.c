/*
 *  Entangle: Tethered Camera Control & Capture
 *
 *  Copyright (C) 2009-2018 Daniel P. Berrange
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

#include <math.h>
#include <string.h>

#include "entangle-image-display.h"

#include "entangle-debug.h"
#include "entangle-image.h"

struct _EntangleImageDisplay
{
    GtkDrawingArea parent;

    GList *images;

    cairo_surface_t *pixmap;
    GdkRGBA bkg;

    gboolean autoscale;
    gdouble scale;

    gdouble aspectRatio;
    gdouble maskOpacity;
    gboolean maskEnabled;
    gboolean overexposureHighlighting;

    gchar *textOverlay;

    gboolean focusPoint;
    EntangleImageDisplayGrid gridDisplay;
};

G_DEFINE_TYPE(EntangleImageDisplay,
              entangle_image_display,
              GTK_TYPE_DRAWING_AREA);

enum
{
    PROP_O,
    PROP_IMAGE,
    PROP_AUTOSCALE,
    PROP_SCALE,
    PROP_ASPECT_RATIO,
    PROP_MASK_OPACITY,
    PROP_MASK_ENABLED,
    PROP_FOCUS_POINT,
    PROP_GRID_DISPLAY,
    PROP_OVEREXPOSURE_HIGHLIGHTING,
    PROP_TEXT_OVERLAY,
};

static void
entangle_image_display_image_pixbuf_notify(GObject *image,
                                           GParamSpec *pspec,
                                           gpointer data);

static void
do_entangle_image_display_render_pixmap(EntangleImageDisplay *display)
{
    int width, height;
    GdkPixbuf *pixbuf = NULL;
    GList *tmp = display->images;
    EntangleImage *image = ENTANGLE_IMAGE(display->images->data);

    ENTANGLE_DEBUG("Setting up server pixmap for %p %s", image,
                   entangle_media_get_filename(ENTANGLE_MEDIA(image)));

    image = ENTANGLE_IMAGE(tmp->data);
    pixbuf = entangle_image_get_pixbuf(image);

    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);
    display->pixmap =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);

    /* Paint the stack of images - the first one
     * is completely opaque and determine the base
     * dimensions. Others are scaled layers on top. */
    cairo_t *cr = cairo_create(display->pixmap);
    while (tmp) {
        int pw, ph;

        image = ENTANGLE_IMAGE(tmp->data);
        pixbuf = entangle_image_get_pixbuf(image);
        pw = gdk_pixbuf_get_width(pixbuf);
        ph = gdk_pixbuf_get_height(pixbuf);

        if (pw != width || ph != height) {
            GdkPixbuf *scaled;
            scaled = gdk_pixbuf_scale_simple(pixbuf, width, height,
                                             GDK_INTERP_BILINEAR);
            gdk_cairo_set_source_pixbuf(cr, scaled, 0, 0);
            g_object_unref(scaled);
        } else {
            gdk_cairo_set_source_pixbuf(cr, pixbuf, 0, 0);
        }

        if (tmp == display->images)
            cairo_paint(cr);
        else if (tmp->next)
            cairo_paint_with_alpha(cr, 0.3);
        else
            cairo_paint_with_alpha(cr, 0.65);

        tmp = tmp->next;
    }

    if (display->overexposureHighlighting) {
        /* Check for blown highlights (overexposure), and set such pixels
           to red. */
        cairo_surface_flush(display->pixmap);
        unsigned char *data = cairo_image_surface_get_data(display->pixmap);
        gboolean blown = FALSE;
        unsigned char *p = data;
        for (int i = 0; i < height; i++) {
            for (int j = 0; j < width; j++, p += 4) {
                if (p[0] == 255 || p[1] == 255 || p[2] == 255) {
                    p[0] = 0;
                    p[1] = 0;
                    p[2] = 254;
                    blown = TRUE;
                }
            }
        }

        if (blown)
            cairo_surface_mark_dirty(display->pixmap);
    }

    cairo_destroy(cr);
}

static void
entangle_image_display_try_render_pixmap(EntangleImageDisplay *display)
{
    GList *tmp = display->images;
    gboolean missing = FALSE;

    if (!gtk_widget_get_realized(GTK_WIDGET(display))) {
        ENTANGLE_DEBUG("Skipping setup for non-realized widget");
        return;
    }

    if (display->pixmap) {
        cairo_surface_destroy(display->pixmap);
        display->pixmap = NULL;
    }

    if (!display->images)
        return;

    while (tmp) {
        EntangleImage *image = tmp->data;

        if (entangle_image_get_pixbuf(image) == NULL)
            missing = TRUE;

        tmp = tmp->next;
    }

    if (!missing) {
        do_entangle_image_display_render_pixmap(display);
    } else {
        ENTANGLE_DEBUG("Not ready to render yet");
    }
}

static void
entangle_image_display_image_pixbuf_notify(GObject *object G_GNUC_UNUSED,
                                           GParamSpec *pspec G_GNUC_UNUSED,
                                           gpointer data)
{
    g_return_if_fail(ENTANGLE_IS_IMAGE_DISPLAY(data));

    EntangleImageDisplay *display = ENTANGLE_IMAGE_DISPLAY(data);

    entangle_image_display_try_render_pixmap(display);
    gtk_widget_queue_resize(GTK_WIDGET(display));
    gtk_widget_queue_draw(GTK_WIDGET(display));
}

static void
do_entangle_image_display_connect(EntangleImageDisplay *display,
                                  EntangleImage *image)
{
    g_signal_connect(image, "notify::pixbuf",
                     G_CALLBACK(entangle_image_display_image_pixbuf_notify),
                     display);

    entangle_image_display_try_render_pixmap(display);
}

static void
entangle_image_display_get_property(GObject *object,
                                    guint prop_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
    EntangleImageDisplay *display = ENTANGLE_IMAGE_DISPLAY(object);

    switch (prop_id) {
    case PROP_AUTOSCALE:
        g_value_set_boolean(value, display->autoscale);
        break;

    case PROP_SCALE:
        g_value_set_float(value, display->scale);
        break;

    case PROP_ASPECT_RATIO:
        g_value_set_float(value, display->aspectRatio);
        break;

    case PROP_MASK_OPACITY:
        g_value_set_float(value, display->maskOpacity);
        break;

    case PROP_MASK_ENABLED:
        g_value_set_boolean(value, display->maskEnabled);
        break;

    case PROP_FOCUS_POINT:
        g_value_set_boolean(value, display->focusPoint);
        break;

    case PROP_GRID_DISPLAY:
        g_value_set_enum(value, display->gridDisplay);
        break;

    case PROP_OVEREXPOSURE_HIGHLIGHTING:
        g_value_set_boolean(value, display->overexposureHighlighting);
        break;

    case PROP_TEXT_OVERLAY:
        g_value_set_string(value, display->textOverlay);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
entangle_image_display_set_property(GObject *object,
                                    guint prop_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
    EntangleImageDisplay *display = ENTANGLE_IMAGE_DISPLAY(object);

    ENTANGLE_DEBUG("Set prop on image display %d", prop_id);

    switch (prop_id) {
    case PROP_AUTOSCALE:
        entangle_image_display_set_autoscale(display,
                                             g_value_get_boolean(value));
        break;

    case PROP_SCALE:
        entangle_image_display_set_scale(display, g_value_get_float(value));
        break;

    case PROP_ASPECT_RATIO:
        entangle_image_display_set_aspect_ratio(display,
                                                g_value_get_float(value));
        break;

    case PROP_MASK_OPACITY:
        entangle_image_display_set_mask_opacity(display,
                                                g_value_get_float(value));
        break;

    case PROP_MASK_ENABLED:
        entangle_image_display_set_mask_enabled(display,
                                                g_value_get_boolean(value));
        break;

    case PROP_FOCUS_POINT:
        entangle_image_display_set_focus_point(display,
                                               g_value_get_boolean(value));
        break;

    case PROP_GRID_DISPLAY:
        entangle_image_display_set_grid_display(display,
                                                g_value_get_enum(value));
        break;

    case PROP_OVEREXPOSURE_HIGHLIGHTING:
        entangle_image_display_set_overexposure_highlighting(
            display, g_value_get_boolean(value));
        break;

    case PROP_TEXT_OVERLAY:
        entangle_image_display_set_text_overlay(display,
                                                g_value_get_string(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
entangle_image_display_finalize(GObject *object)
{
    EntangleImageDisplay *display = ENTANGLE_IMAGE_DISPLAY(object);
    GList *tmp = display->images;

    while (tmp) {
        EntangleImage *image = tmp->data;

        g_signal_handlers_disconnect_by_data(image, display);
        g_object_unref(image);

        tmp = tmp->next;
    }
    g_list_free(display->images);

    if (display->pixmap)
        cairo_surface_destroy(display->pixmap);

    g_free(display->textOverlay);

    G_OBJECT_CLASS(entangle_image_display_parent_class)->finalize(object);
}

static void
entangle_image_display_realize(GtkWidget *widget)
{
    g_return_if_fail(ENTANGLE_IS_IMAGE_DISPLAY(widget));

    GTK_WIDGET_CLASS(entangle_image_display_parent_class)->realize(widget);

    EntangleImageDisplay *display = ENTANGLE_IMAGE_DISPLAY(widget);

    entangle_image_display_try_render_pixmap(display);
}

static void
entangle_image_display_draw_focus_point(GtkWidget *widget, cairo_t *cr)
{
    g_return_if_fail(ENTANGLE_IS_IMAGE_DISPLAY(widget));

    gint ww, wh, cx, cy;

    ww = gdk_window_get_width(gtk_widget_get_window(widget));
    wh = gdk_window_get_height(gtk_widget_get_window(widget));

    cx = ww / 2;
    cy = wh / 2;

    cairo_set_source_rgba(cr, 0.7, 0, 0, 1);

#define SIZE 12
#define GAP 4
#define WIDTH 3

    /* top left */
    cairo_move_to(cr, cx - SIZE, cy - SIZE);
    cairo_line_to(cr, cx - GAP, cy - SIZE);
    cairo_line_to(cr, cx - GAP, cy - SIZE + WIDTH);
    cairo_line_to(cr, cx - SIZE + WIDTH, cy - SIZE + WIDTH);
    cairo_line_to(cr, cx - SIZE + WIDTH, cy - GAP);
    cairo_line_to(cr, cx - SIZE, cy - GAP);
    cairo_move_to(cr, cx - SIZE, cy - SIZE);
    cairo_fill(cr);

    /* top right */
    cairo_move_to(cr, cx + GAP, cy - SIZE);
    cairo_line_to(cr, cx + SIZE, cy - SIZE);
    cairo_line_to(cr, cx + SIZE, cy - GAP);
    cairo_line_to(cr, cx + SIZE - WIDTH, cy - GAP);
    cairo_line_to(cr, cx + SIZE - WIDTH, cy - SIZE + WIDTH);
    cairo_line_to(cr, cx + GAP, cy - SIZE + WIDTH);
    cairo_move_to(cr, cx + GAP, cy - SIZE);
    cairo_fill(cr);

    /* bottom left */
    cairo_move_to(cr, cx - SIZE, cy + SIZE);
    cairo_line_to(cr, cx - SIZE, cy + GAP);
    cairo_line_to(cr, cx - SIZE + WIDTH, cy + GAP);
    cairo_line_to(cr, cx - SIZE + WIDTH, cy + SIZE - WIDTH);
    cairo_line_to(cr, cx - GAP, cy + SIZE - WIDTH);
    cairo_line_to(cr, cx - GAP, cy + SIZE);
    cairo_line_to(cr, cx - SIZE, cy + SIZE);
    cairo_fill(cr);

    /* bottom right */
    cairo_move_to(cr, cx + WIDTH, cy + SIZE);
    cairo_line_to(cr, cx + SIZE, cy + SIZE);
    cairo_line_to(cr, cx + SIZE, cy + WIDTH);
    cairo_line_to(cr, cx + SIZE - WIDTH, cy + WIDTH);
    cairo_line_to(cr, cx + SIZE - WIDTH, cy + SIZE - WIDTH);
    cairo_line_to(cr, cx + WIDTH, cy + SIZE - WIDTH);
    cairo_line_to(cr, cx + WIDTH, cy + SIZE);
    cairo_fill(cr);
}

static void
entangle_image_display_draw_grid_display(GtkWidget *widget,
                                         cairo_t *cr,
                                         gdouble mx,
                                         gdouble my)
{
    g_return_if_fail(ENTANGLE_IS_IMAGE_DISPLAY(widget));

    EntangleImageDisplay *display = ENTANGLE_IMAGE_DISPLAY(widget);
    gint ww = gdk_window_get_width(gtk_widget_get_window(widget));
    gint wh = gdk_window_get_height(gtk_widget_get_window(widget));
    gdouble cx = (ww - (mx * 2)) / 2.0;
    gdouble cy = (wh - (my * 2)) / 2.0;
    gdouble offx[4], offy[4];
    gint count;
    gint i;

    cairo_set_source_rgba(cr, 0.7, 0, 0, 1);

    if (display->focusPoint) {
        /* Cut out the area where the focus point lives
         * to avoid lines going through it */
        cairo_rectangle(cr, mx, my, ww - (mx * 2), wh - (my * 2));
        cairo_rectangle(cr, ww - mx - cx + SIZE, my + cy - SIZE, -1 * SIZE * 2,
                        SIZE * 2);
        cairo_clip(cr);
    }

    switch (display->gridDisplay) {
    case ENTANGLE_IMAGE_DISPLAY_GRID_CENTER_LINES:
        count = 1;
        offx[0] = (ww - (mx * 2)) / 2;
        offy[0] = (wh - (my * 2)) / 2;
        break;

    case ENTANGLE_IMAGE_DISPLAY_GRID_RULE_OF_3RDS:
        count = 2;
        offx[0] = (ww - (mx * 2)) / 3;
        offy[0] = (wh - (my * 2)) / 3;
        offx[1] = (ww - (mx * 2)) / 3 * 2;
        offy[1] = (wh - (my * 2)) / 3 * 2;
        break;

    case ENTANGLE_IMAGE_DISPLAY_GRID_QUARTERS:
        count = 3;
        offx[0] = (ww - (mx * 2)) / 4;
        offy[0] = (wh - (my * 2)) / 4;
        offx[1] = (ww - (mx * 2)) / 4 * 2;
        offy[1] = (wh - (my * 2)) / 4 * 2;
        offx[2] = (ww - (mx * 2)) / 4 * 3;
        offy[2] = (wh - (my * 2)) / 4 * 3;
        break;

    case ENTANGLE_IMAGE_DISPLAY_GRID_RULE_OF_5THS:
        count = 4;
        offx[0] = (ww - (mx * 2)) / 5;
        offy[0] = (wh - (my * 2)) / 5;
        offx[1] = (ww - (mx * 2)) / 5 * 2;
        offy[1] = (wh - (my * 2)) / 5 * 2;
        offx[2] = (ww - (mx * 2)) / 5 * 3;
        offy[2] = (wh - (my * 2)) / 5 * 3;
        offx[3] = (ww - (mx * 2)) / 5 * 4;
        offy[3] = (wh - (my * 2)) / 5 * 4;
        break;

    case ENTANGLE_IMAGE_DISPLAY_GRID_GOLDEN_SECTIONS:
        count = 2;
        offx[0] = (ww - (mx * 2)) / 100 * 38;
        offy[0] = (wh - (my * 2)) / 100 * 38;
        offx[1] = (ww - (mx * 2)) / 100 * 62;
        offy[1] = (wh - (my * 2)) / 100 * 62;
        break;

    case ENTANGLE_IMAGE_DISPLAY_GRID_NONE:
    default:
        count = 0;
        break;
    }

    for (i = 0; i < count; i++) {
        /* Horizontal line */
        cairo_move_to(cr, mx, my + offy[i] - 1);
        cairo_line_to(cr, ww - mx, my + offy[i] - 1);
        cairo_line_to(cr, ww - mx, my + offy[i]);
        cairo_line_to(cr, mx, my + offy[i]);
        cairo_line_to(cr, mx, my + offy[i] - 1);
        cairo_fill(cr);

        /* Vertical line */
        cairo_move_to(cr, mx + offx[i] - 1, my);
        cairo_line_to(cr, mx + offx[i], my);
        cairo_line_to(cr, mx + offx[i], wh - my);
        cairo_line_to(cr, mx + offx[i] - 1, wh - my);
        cairo_line_to(cr, mx + offx[i] - 1, my);
        cairo_fill(cr);
    }
}

static void
entangle_image_display_draw_text(EntangleImageDisplay *display, cairo_t *cr)
{
    PangoLayout *baselayout = pango_cairo_create_layout(cr);
    PangoLayout *scaledlayout = pango_cairo_create_layout(cr);
    PangoFontDescription *basefont;
    PangoFontDescription *scaledfont = pango_font_description_new();
    int w, h, ww, wh;
    int size;
    int ox, oy;
    double sx, sy;
    char *fontspec;

    cairo_set_source_rgba(cr, 0.8, 0.8, 0.8, 0.8);

    ww = gdk_window_get_width(gtk_widget_get_window(GTK_WIDGET(display)));
    wh = gdk_window_get_height(gtk_widget_get_window(GTK_WIDGET(display)));

    basefont = pango_font_description_from_string("Arial 25");

    pango_layout_set_font_description(baselayout, basefont);
    pango_layout_set_text(baselayout, display->textOverlay, -1);
    pango_cairo_update_layout(cr, baselayout);

    pango_layout_get_pixel_size(baselayout, &w, &h);

    sx = (ww * 0.8) / w;
    sy = (wh * 0.8) / h;

    if (sx > sy) {
        size = 25 * sy;
        ox = (ww - (w * sy)) / 2;
        oy = (wh - (h * sy)) / 2;
    } else {
        size = 25 * sx;
        ox = (ww - (w * sx)) / 2;
        oy = (wh - (h * sx)) / 2;
    }

    cairo_translate(cr, ox, oy);

    fontspec = g_strdup_printf("Arial %d", (int)size);
    scaledfont = pango_font_description_from_string(fontspec);

    pango_layout_set_font_description(scaledlayout, scaledfont);
    pango_layout_set_text(scaledlayout, display->textOverlay, -1);
    pango_cairo_update_layout(cr, scaledlayout);

    pango_cairo_show_layout(cr, scaledlayout);

    g_object_unref(baselayout);
    g_object_unref(scaledlayout);
    pango_font_description_free(basefont);
    pango_font_description_free(scaledfont);
}

static gboolean
entangle_image_display_draw(GtkWidget *widget, cairo_t *cr)
{
    g_return_val_if_fail(ENTANGLE_IS_IMAGE_DISPLAY(widget), FALSE);

    EntangleImageDisplay *display = ENTANGLE_IMAGE_DISPLAY(widget);
    int ww, wh;            /* Available drawing area extents */
    int pw = 0, ph = 0;    /* Original image size */
    double iw, ih;         /* Desired image size */
    double mx = 0, my = 0; /* Offset of image within available area */
    double sx = 1, sy = 1; /* Amount to scale by */
    double aspectWin, aspectImage = 0.0;

    ww = gdk_window_get_width(gtk_widget_get_window(widget));
    wh = gdk_window_get_height(gtk_widget_get_window(widget));
    aspectWin = (double)ww / (double)wh;

    if (display->pixmap) {
        pw = cairo_image_surface_get_width(display->pixmap);
        ph = cairo_image_surface_get_height(display->pixmap);
        aspectImage = (double)pw / (double)ph;
    }

    /* Decide what size we're going to draw the image */
    if (display->autoscale) {
        if (aspectWin > aspectImage) {
            /* Match drawn height to widget height, scale width preserving
             * aspect */
            ih = (double)wh;
            iw = (double)ih * aspectImage;
        } else if (aspectWin < aspectImage) {
            /* Match drawn width to widget width, scale height preserving aspect
             */
            iw = (double)ww;
            ih = (double)iw / aspectImage;
        } else {
            /* Match drawn size to widget size */
            iw = (double)ww;
            ih = (double)wh;
        }
    } else {
        if (display->scale > 0) {
            /* Scale image larger */
            iw = (double)pw * display->scale;
            ih = (double)ph * display->scale;
        } else {
            /* Use native image size */
            iw = (double)pw;
            ih = (double)ph;
        }
    }

    /* Calculate any offset within available area */
    mx = (ww - iw) / 2;
    my = (wh - ih) / 2;

    /* Calculate the scale factor for drawing */
    sx = iw / pw;
    sy = ih / ph;

    ENTANGLE_DEBUG("Got win %dx%d image %dx%d, autoscale=%d scale=%lf", ww, wh,
                   pw, ph, display->autoscale ? 1 : 0, display->scale);
    ENTANGLE_DEBUG("Drawing image %lf,%lf at %lf %lf sclaed %lfx%lf", iw, ih,
                   mx, my, sx, sy);

    /* We need to fill the background first */
    cairo_save(cr);
    cairo_set_source_rgb(cr, display->bkg.red, display->bkg.green,
                         display->bkg.blue);
    cairo_rectangle(cr, 0, 0, ww, wh);

    /* Next cut out the inner area where the pixmap
       will be drawn. This avoids 'flashing' since we're
       not double-buffering. Note we're using the undocumented
       behaviour of drawing the rectangle from right to left
       to cut out the whole */
    if (display->pixmap)
        cairo_rectangle(cr, mx + iw, my, -1 * iw, ih);
    cairo_fill(cr);
    cairo_restore(cr);

    /* Draw the actual image(s) */
    if (display->pixmap) {
        cairo_matrix_t m;
        cairo_get_matrix(cr, &m);
        cairo_scale(cr, sx, sy);

        cairo_set_source_surface(cr, display->pixmap, mx / sx, my / sy);
        cairo_paint(cr);
        cairo_set_matrix(cr, &m);
    }

    cairo_save(cr);

    /* Draw the focus point */
    if (display->focusPoint)
        entangle_image_display_draw_focus_point(widget, cr);

    /* Draw the grid lines */
    entangle_image_display_draw_grid_display(widget, cr, mx, my);

    cairo_restore(cr);

    /* Finally a possible aspect ratio mask */
    if (display->pixmap && display->maskEnabled &&
        (fabs(display->aspectRatio - aspectImage) > 0.005)) {
        cairo_set_source_rgba(cr, 0, 0, 0, display->maskOpacity);

        if (display->aspectRatio > aspectImage) { /* mask top & bottom */
            double ah = (ih - (iw / display->aspectRatio)) / 2;

            cairo_rectangle(cr, mx, my, iw, ah);
            cairo_fill(cr);

            cairo_rectangle(cr, mx, my + ih - ah, iw, ah);
            cairo_fill(cr);
        } else { /* mask left & right */
            double aw = (iw - (ih * display->aspectRatio)) / 2;

            cairo_rectangle(cr, mx, my, aw, ih);
            cairo_fill(cr);

            cairo_rectangle(cr, mx + iw - aw, my, aw, ih);
            cairo_fill(cr);
        }
    }

    if (display->textOverlay)
        entangle_image_display_draw_text(display, cr);

    return TRUE;
}

static void
entangle_image_display_get_preferred_width(GtkWidget *widget,
                                           gint *minwidth,
                                           gint *natwidth)
{
    g_return_if_fail(ENTANGLE_IS_IMAGE_DISPLAY(widget));

    EntangleImageDisplay *display = ENTANGLE_IMAGE_DISPLAY(widget);
    GdkPixbuf *pixbuf = NULL;
    EntangleImage *image = entangle_image_display_get_image(display);

    if (image)
        pixbuf = entangle_image_get_pixbuf(image);

    if (!pixbuf) {
        *minwidth = *natwidth = 100;
        ENTANGLE_DEBUG("No image, size request 100,100");
        return;
    }

    if (display->autoscale) {
        /* For best fit mode, we'll say 100x100 is the smallest we're happy
         * to draw to. Whatever the container allocates us beyond that will
         * be used filled with the image */
        *minwidth = *natwidth = 100;
    } else {
        /* Start a 1-to-1 mode */
        *minwidth = *natwidth = gdk_pixbuf_get_width(pixbuf);
        if (display->scale > 0) {
            /* Scaling mode */
            *minwidth = *natwidth = (int)((double)*minwidth * display->scale);
        }
    }
}

static void
entangle_image_display_get_preferred_height(GtkWidget *widget,
                                            gint *minheight,
                                            gint *natheight)
{
    g_return_if_fail(ENTANGLE_IS_IMAGE_DISPLAY(widget));

    EntangleImageDisplay *display = ENTANGLE_IMAGE_DISPLAY(widget);
    GdkPixbuf *pixbuf = NULL;
    EntangleImage *image = entangle_image_display_get_image(display);

    if (image)
        pixbuf = entangle_image_get_pixbuf(image);

    if (!pixbuf) {
        *minheight = *natheight = 100;
        ENTANGLE_DEBUG("No image, size request 100,100");
        return;
    }

    if (display->autoscale) {
        /* For best fit mode, we'll say 100x100 is the smallest we're happy
         * to draw to. Whatever the container allocates us beyond that will
         * be used filled with the image */
        *minheight = *natheight = 100;
    } else {
        /* Start a 1-to-1 mode */
        *minheight = *natheight = gdk_pixbuf_get_height(pixbuf);
        if (display->scale > 0) {
            /* Scaling mode */
            *minheight = *natheight =
                (int)((double)*minheight * display->scale);
        }
    }
}

static void
entangle_image_display_class_init(EntangleImageDisplayClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->finalize = entangle_image_display_finalize;
    object_class->get_property = entangle_image_display_get_property;
    object_class->set_property = entangle_image_display_set_property;

    widget_class->realize = entangle_image_display_realize;
    widget_class->draw = entangle_image_display_draw;
    widget_class->get_preferred_width =
        entangle_image_display_get_preferred_width;
    widget_class->get_preferred_height =
        entangle_image_display_get_preferred_height;

    g_object_class_install_property(
        object_class, PROP_IMAGE,
        g_param_spec_object("image", "Image", "Image", ENTANGLE_TYPE_IMAGE,
                            G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
    g_object_class_install_property(
        object_class, PROP_AUTOSCALE,
        g_param_spec_boolean("autoscale", "Automatic scaling",
                             "Automatically scale image to fit available area",
                             TRUE,
                             G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                 G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
    g_object_class_install_property(
        object_class, PROP_SCALE,
        g_param_spec_float(
            "scale", "Scale image",
            "Scale factor for image, 0-1 for zoom out, 1->32 for zoom in", 0.0,
            32.0, 0.0,
            G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                G_PARAM_STATIC_BLURB));

    g_object_class_install_property(
        object_class, PROP_ASPECT_RATIO,
        g_param_spec_float("aspect-ratio", "Aspect ratio",
                           "Aspect ratio to mask image to", 0.0, 100.0, 1.69,
                           G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                               G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    g_object_class_install_property(
        object_class, PROP_MASK_OPACITY,
        g_param_spec_float("mask-opacity", "Mask opacity",
                           "Mask opacity when adjusting aspect ratio", 0.0, 1.0,
                           0.5,
                           G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                               G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    g_object_class_install_property(
        object_class, PROP_MASK_ENABLED,
        g_param_spec_boolean("mask-enabled", "Mask enabled",
                             "Enable aspect ratio image mask", FALSE,
                             G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                 G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    g_object_class_install_property(
        object_class, PROP_FOCUS_POINT,
        g_param_spec_boolean("focus-point", "Focus point",
                             "Overlay center focus point", FALSE,
                             G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                 G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    g_object_class_install_property(
        object_class, PROP_GRID_DISPLAY,
        g_param_spec_enum("grid-display", "Grid display", "Grid line display",
                          ENTANGLE_TYPE_IMAGE_DISPLAY_GRID,
                          ENTANGLE_IMAGE_DISPLAY_GRID_NONE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                              G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    g_object_class_install_property(
        object_class, PROP_OVEREXPOSURE_HIGHLIGHTING,
        g_param_spec_boolean("overexposure-highlighting",
                             "Overexposure highlighting",
                             "Highlight areas of overexposure", TRUE,
                             G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                 G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
}

EntangleImageDisplay *
entangle_image_display_new(void)
{
    return ENTANGLE_IMAGE_DISPLAY(
        g_object_new(ENTANGLE_TYPE_IMAGE_DISPLAY, NULL));
}

static void
entangle_image_display_init(EntangleImageDisplay *display)
{
    display->autoscale = TRUE;
    display->maskOpacity = 0.9;
    display->aspectRatio = 1.33;
    display->maskEnabled = FALSE;
    display->overexposureHighlighting = TRUE;
}

/**
 * entangle_image_display_set_image:
 * @display: the display widget
 * @image: (allow-none)(transfer none): the image to display, or NULL
 *
 * Set the image to be displayed by the widget. This is a
 * shortcut for setting an image list of length 1.
 */
void
entangle_image_display_set_image(EntangleImageDisplay *display,
                                 EntangleImage *image)
{
    g_return_if_fail(ENTANGLE_IS_IMAGE_DISPLAY(display));
    g_return_if_fail(!image || ENTANGLE_IS_IMAGE(image));

    if (image) {
        GList *tmp = g_list_append(NULL, image);
        entangle_image_display_set_image_list(display, tmp);
        g_list_free(tmp);
    } else {
        entangle_image_display_set_image_list(display, NULL);
    }
}

/**
 * entangle_image_display_get_image:
 * @display: the display widget
 *
 * Retrieve the image being displayed. If there are multiple
 * images to be displayed, then only the first image is returned.
 *
 * Returns: (transfer none): the image being displayed, or NULL
 */
EntangleImage *
entangle_image_display_get_image(EntangleImageDisplay *display)
{
    g_return_val_if_fail(ENTANGLE_IS_IMAGE_DISPLAY(display), NULL);

    if (!display->images)
        return NULL;

    return ENTANGLE_IMAGE(display->images->data);
}

/**
 * entangle_image_display_set_image_list:
 * @display: the display widget
 * @images: (transfer none)(element-type EntangleImage): the images to display
 *
 * Set the list of images to be displayed. If multiple images
 * are provided they are overlayed with opacity
 */
void
entangle_image_display_set_image_list(EntangleImageDisplay *display,
                                      GList *images)
{
    g_return_if_fail(ENTANGLE_IS_IMAGE_DISPLAY(display));

    GList *tmp;

    tmp = display->images;
    while (tmp) {
        EntangleImage *image = ENTANGLE_IMAGE(tmp->data);

        g_signal_handlers_disconnect_by_data(image, display);
        g_object_unref(image);

        tmp = tmp->next;
    }
    g_list_free(display->images);

    display->images = NULL;

    tmp = images;
    while (tmp) {
        EntangleImage *image = ENTANGLE_IMAGE(tmp->data);

        do_entangle_image_display_connect(display, image);

        display->images = g_list_append(display->images, g_object_ref(image));

        tmp = tmp->next;
    }
    display->images = g_list_reverse(display->images);

    entangle_image_display_try_render_pixmap(display);
    gtk_widget_queue_resize(GTK_WIDGET(display));
    gtk_widget_queue_draw(GTK_WIDGET(display));
}

/**
 * entangle_image_display_get_image_list:
 * @display: the display widget
 *
 * Get the list of images being displayed
 *
 * Returns: (transfer full)(element-type EntangleImage): the list of images
 */
GList *
entangle_image_display_get_image_list(EntangleImageDisplay *display)
{
    g_return_val_if_fail(ENTANGLE_IS_IMAGE_DISPLAY(display), FALSE);

    g_list_foreach(display->images, (GFunc)g_object_ref, NULL);

    return g_list_copy(display->images);
}

void
entangle_image_display_set_autoscale(EntangleImageDisplay *display,
                                     gboolean autoscale)
{
    g_return_if_fail(ENTANGLE_IS_IMAGE_DISPLAY(display));

    display->autoscale = autoscale;

    if (gtk_widget_get_visible((GtkWidget *)display))
        gtk_widget_queue_resize(GTK_WIDGET(display));
}

gboolean
entangle_image_display_get_autoscale(EntangleImageDisplay *display)
{
    g_return_val_if_fail(ENTANGLE_IS_IMAGE_DISPLAY(display), FALSE);

    return display->autoscale;
}

void
entangle_image_display_set_scale(EntangleImageDisplay *display, gdouble scale)
{
    g_return_if_fail(ENTANGLE_IS_IMAGE_DISPLAY(display));

    display->scale = scale;

    if (gtk_widget_get_visible((GtkWidget *)display))
        gtk_widget_queue_resize(GTK_WIDGET(display));
}

gdouble
entangle_image_display_get_scale(EntangleImageDisplay *display)
{
    g_return_val_if_fail(ENTANGLE_IS_IMAGE_DISPLAY(display), 1.0);

    return display->scale;
}

void
entangle_image_display_set_aspect_ratio(EntangleImageDisplay *display,
                                        gdouble aspect)
{
    g_return_if_fail(ENTANGLE_IS_IMAGE_DISPLAY(display));

    display->aspectRatio = aspect;

    if (gtk_widget_get_visible((GtkWidget *)display))
        gtk_widget_queue_draw(GTK_WIDGET(display));
}

gdouble
entangle_image_display_get_aspect_ratio(EntangleImageDisplay *display)
{
    g_return_val_if_fail(ENTANGLE_IS_IMAGE_DISPLAY(display), 1.0);

    return display->aspectRatio;
}

void
entangle_image_display_set_mask_opacity(EntangleImageDisplay *display,
                                        gdouble opacity)
{
    g_return_if_fail(ENTANGLE_IS_IMAGE_DISPLAY(display));

    display->maskOpacity = opacity;

    if (gtk_widget_get_visible((GtkWidget *)display))
        gtk_widget_queue_draw(GTK_WIDGET(display));
}

gdouble
entangle_image_display_get_mask_opacity(EntangleImageDisplay *display)
{
    g_return_val_if_fail(ENTANGLE_IS_IMAGE_DISPLAY(display), 1.0);

    return display->maskOpacity;
}

void
entangle_image_display_set_mask_enabled(EntangleImageDisplay *display,
                                        gboolean enabled)
{
    g_return_if_fail(ENTANGLE_IS_IMAGE_DISPLAY(display));

    display->maskEnabled = enabled;

    if (gtk_widget_get_visible((GtkWidget *)display))
        gtk_widget_queue_draw(GTK_WIDGET(display));
}

gboolean
entangle_image_display_get_mask_enabled(EntangleImageDisplay *display)
{
    g_return_val_if_fail(ENTANGLE_IS_IMAGE_DISPLAY(display), FALSE);

    return display->maskEnabled;
}

void
entangle_image_display_set_focus_point(EntangleImageDisplay *display,
                                       gboolean enabled)
{
    display->focusPoint = enabled;

    if (gtk_widget_get_visible((GtkWidget *)display))
        gtk_widget_queue_draw(GTK_WIDGET(display));
}

gboolean
entangle_image_display_get_focus_point(EntangleImageDisplay *display)
{
    return display->focusPoint;
}

void
entangle_image_display_set_grid_display(EntangleImageDisplay *display,
                                        EntangleImageDisplayGrid mode)
{
    display->gridDisplay = mode;

    if (gtk_widget_get_visible((GtkWidget *)display))
        gtk_widget_queue_draw(GTK_WIDGET(display));
}

EntangleImageDisplayGrid
entangle_image_display_get_grid_display(EntangleImageDisplay *display)
{
    return display->gridDisplay;
}

void
entangle_image_display_set_overexposure_highlighting(
    EntangleImageDisplay *display,
    gboolean enabled)
{
    g_return_if_fail(ENTANGLE_IS_IMAGE_DISPLAY(display));

    display->overexposureHighlighting = enabled;

    entangle_image_display_try_render_pixmap(display);
    if (gtk_widget_get_visible((GtkWidget *)display))
        gtk_widget_queue_draw(GTK_WIDGET(display));
}

gboolean
entangle_image_display_get_overexposure_highlighting(
    EntangleImageDisplay *display)
{
    g_return_val_if_fail(ENTANGLE_IS_IMAGE_DISPLAY(display), FALSE);

    return display->overexposureHighlighting;
}

gboolean
entangle_image_display_get_loaded(EntangleImageDisplay *display)
{
    EntangleImage *image = entangle_image_display_get_image(display);
    GdkPixbuf *pixbuf = NULL;

    if (image)
        pixbuf = entangle_image_get_pixbuf(image);

    return pixbuf != NULL;
}

void
entangle_image_display_set_background(EntangleImageDisplay *display,
                                      const gchar *background)
{
    g_return_if_fail(ENTANGLE_IS_IMAGE_DISPLAY(display));

    gdk_rgba_parse(&display->bkg, background);

    if (gtk_widget_get_visible((GtkWidget *)display))
        gtk_widget_queue_draw(GTK_WIDGET(display));
}

gchar *
entangle_image_display_get_background(EntangleImageDisplay *display)
{
    g_return_val_if_fail(ENTANGLE_IS_IMAGE_DISPLAY(display), FALSE);

    return gdk_rgba_to_string(&display->bkg);
}

/**
 * entangle_image_display_set_text_overlay:
 * @display: (transfer none): the display
 * @msg: (transfer none)(nullable): the message to display, or NULL
 *
 * Set a message to render over the top of the image
 */
void
entangle_image_display_set_text_overlay(EntangleImageDisplay *display,
                                        const char *msg)
{
    g_return_if_fail(ENTANGLE_IS_IMAGE_DISPLAY(display));

    g_free(display->textOverlay);
    display->textOverlay = g_strdup(msg);
}

const char *
entangle_image_display_get_text_overlay(EntangleImageDisplay *display)
{
    g_return_val_if_fail(ENTANGLE_IS_IMAGE_DISPLAY(display), NULL);

    return display->textOverlay;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
