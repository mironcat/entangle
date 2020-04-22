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

#include <string.h>

#include "entangle-media-statusbar.h"

#include "entangle-debug.h"
#include "entangle-media.h"

struct _EntangleMediaStatusbar
{
    GtkBox parent;

    gulong mediaNotifyID;
    EntangleMedia *media;

    GtkWidget *metaBox;

    GtkWidget *metaAperture;
    GtkWidget *metaShutter;
    GtkWidget *metaIso;
    GtkWidget *metaFocal;
    GtkWidget *metaDimensions;
};

G_DEFINE_TYPE(EntangleMediaStatusbar, entangle_media_statusbar, GTK_TYPE_BOX);

enum
{
    PROP_O,
    PROP_MEDIA,
};

static void
entangle_media_statusbar_get_property(GObject *object,
                                      guint prop_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
    EntangleMediaStatusbar *statusbar = ENTANGLE_MEDIA_STATUSBAR(object);

    switch (prop_id) {
    case PROP_MEDIA:
        g_value_set_object(value, statusbar->media);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
entangle_media_statusbar_set_property(GObject *object,
                                      guint prop_id,
                                      const GValue *value,
                                      GParamSpec *pspec)
{
    EntangleMediaStatusbar *statusbar = ENTANGLE_MEDIA_STATUSBAR(object);

    ENTANGLE_DEBUG("Set prop on media statusbar %d", prop_id);

    switch (prop_id) {
    case PROP_MEDIA:
        entangle_media_statusbar_set_media(statusbar,
                                           g_value_get_object(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
entangle_media_statusbar_finalize(GObject *object)
{
    EntangleMediaStatusbar *statusbar = ENTANGLE_MEDIA_STATUSBAR(object);

    if (statusbar->media) {
        g_signal_handler_disconnect(statusbar->media, statusbar->mediaNotifyID);
        g_object_unref(statusbar->media);
    }

    G_OBJECT_CLASS(entangle_media_statusbar_parent_class)->finalize(object);
}

static gboolean
entangle_media_statusbar_draw(GtkWidget *widget, cairo_t *cr)
{
    int ww, wh;
    ww = gdk_window_get_width(gtk_widget_get_window(widget));
    wh = gdk_window_get_height(gtk_widget_get_window(widget));

    cairo_save(cr);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.66);
    cairo_rectangle(cr, 0, 0, ww, wh);

    cairo_fill(cr);
    cairo_restore(cr);

    GTK_WIDGET_CLASS(entangle_media_statusbar_parent_class)->draw(widget, cr);
    return TRUE;
}

static void
entangle_media_statusbar_class_init(EntangleMediaStatusbarClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->finalize = entangle_media_statusbar_finalize;
    object_class->get_property = entangle_media_statusbar_get_property;
    object_class->set_property = entangle_media_statusbar_set_property;

    widget_class->draw = entangle_media_statusbar_draw;

    g_object_class_install_property(
        object_class, PROP_MEDIA,
        g_param_spec_object("media", "Media", "Media", ENTANGLE_TYPE_MEDIA,
                            G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
}

EntangleMediaStatusbar *
entangle_media_statusbar_new(void)
{
    return ENTANGLE_MEDIA_STATUSBAR(
        g_object_new(ENTANGLE_TYPE_MEDIA_STATUSBAR, NULL));
}

static void
entangle_media_statusbar_init(EntangleMediaStatusbar *statusbar)
{
    g_return_if_fail(ENTANGLE_IS_MEDIA_STATUSBAR(statusbar));

    statusbar->metaBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    g_object_set(statusbar->metaBox, "homogeneous", TRUE, NULL);
    g_object_set(statusbar->metaBox, "expand", TRUE, NULL);

    gtk_container_set_border_width(GTK_CONTAINER(statusbar->metaBox), 6);

    statusbar->metaAperture = gtk_label_new("");
    statusbar->metaShutter = gtk_label_new("");
    statusbar->metaFocal = gtk_label_new("");
    statusbar->metaIso = gtk_label_new("");
    statusbar->metaDimensions = gtk_label_new("");

    gtk_box_pack_start(GTK_BOX(statusbar->metaBox), statusbar->metaAperture,
                       TRUE, TRUE, 6);
    gtk_box_pack_start(GTK_BOX(statusbar->metaBox), statusbar->metaShutter,
                       TRUE, TRUE, 6);
    gtk_box_pack_start(GTK_BOX(statusbar->metaBox), statusbar->metaFocal, TRUE,
                       TRUE, 6);
    gtk_box_pack_start(GTK_BOX(statusbar->metaBox), statusbar->metaIso, TRUE,
                       TRUE, 6);
    gtk_box_pack_start(GTK_BOX(statusbar->metaBox), statusbar->metaDimensions,
                       TRUE, TRUE, 6);

    gtk_container_add(GTK_CONTAINER(statusbar), statusbar->metaBox);

    gtk_widget_show_all(GTK_WIDGET(statusbar));
}

static void
entangle_media_statusbar_update_labels(EntangleMediaStatusbar *statusbar)
{
    g_return_if_fail(ENTANGLE_IS_MEDIA_STATUSBAR(statusbar));

    GExiv2Metadata *metadata =
        statusbar->media
            ? entangle_media_get_metadata(ENTANGLE_MEDIA(statusbar->media))
            : NULL;
    gchar *shutter = NULL;
    gchar *aperture = NULL;
    gchar *focal = NULL;
    gchar *iso = NULL;
    gchar *dimensions = NULL;

    if (metadata) {
        gint nom, den;
        gint fnumn, fnumd;
        gdouble fnum;
        guint isonum;
        gdouble focalnum;

        if (gexiv2_metadata_has_tag(metadata, "Exif.Photo.ExposureTime")) {
            gexiv2_metadata_get_exif_tag_rational(
                metadata, "Exif.Photo.ExposureTime", &nom, &den);
            if (den == 10)
                shutter = g_strdup_printf("%0.1lf secs", (double)nom / 10.0);
            else if (nom == 10)
                shutter = g_strdup_printf("1/%0.0lf secs", (double)den / 10.0);
            else
                shutter = g_strdup_printf("%d/%d secs", nom, den);
        }

        if ((gexiv2_metadata_has_tag(metadata, "Exif.Photo.FNumber") &&
             gexiv2_metadata_get_exif_tag_rational(
                 metadata, "Exif.Photo.FNumber", &fnumn, &fnumd)) ||
            (gexiv2_metadata_has_tag(metadata, "Exif.Photo.Aperture") &&
             gexiv2_metadata_get_exif_tag_rational(
                 metadata, "Exif.Photo.Aperture", &fnumn, &fnumd))) {
            fnum = (double)fnumn / (double)fnumd;
            if (fnum < 10.0)
                aperture = g_strdup_printf("f/%1.1f", fnum);
            else
                aperture = g_strdup_printf("f/%2.0f", fnum);
        }

        if (gexiv2_metadata_has_tag(metadata, "Exif.Photo.ISOSpeedRatings")) {
            isonum = gexiv2_metadata_get_iso_speed(metadata);
            iso = g_strdup_printf("ISO %d", isonum);
        }

        if (gexiv2_metadata_has_tag(metadata, "Exif.Photo.FocalLength")) {
            gexiv2_metadata_get_exif_tag_rational(
                metadata, "Exif.Photo.FocalLength", &nom, &den);
            if (den > 0.0001) {
                focalnum = (nom / den);
                focal = g_strdup_printf("%0.0lf mm", focalnum);
            }
        }

        dimensions = g_strdup_printf(
            "%d x %d", gexiv2_metadata_get_pixel_width(metadata),
            gexiv2_metadata_get_pixel_height(metadata));
    }

    gtk_label_set_text(GTK_LABEL(statusbar->metaShutter),
                       shutter ? shutter : "");
    gtk_label_set_text(GTK_LABEL(statusbar->metaAperture),
                       aperture ? aperture : "");
    gtk_label_set_text(GTK_LABEL(statusbar->metaFocal), focal ? focal : "");
    gtk_label_set_text(GTK_LABEL(statusbar->metaIso), iso ? iso : "");
    gtk_label_set_text(GTK_LABEL(statusbar->metaDimensions),
                       dimensions ? dimensions : "");

    g_free(shutter);
    g_free(aperture);
    g_free(focal);
    g_free(iso);
    g_free(dimensions);
}

static void
entangle_media_statusbar_media_metadata_notify(GObject *media G_GNUC_UNUSED,
                                               GParamSpec *pspec G_GNUC_UNUSED,
                                               gpointer data)
{
    g_return_if_fail(ENTANGLE_IS_MEDIA_STATUSBAR(data));

    EntangleMediaStatusbar *statusbar = ENTANGLE_MEDIA_STATUSBAR(data);

    entangle_media_statusbar_update_labels(statusbar);
}

/**
 * entangle_media_statusbar_set_media:
 * @statusbar: (transfer none): the status bar widget
 * @media: (transfer none)(allow-none): the media to display status for, or NULL
 *
 * Set the media to display status information for
 */
void
entangle_media_statusbar_set_media(EntangleMediaStatusbar *statusbar,
                                   EntangleMedia *media)
{
    g_return_if_fail(ENTANGLE_IS_MEDIA_STATUSBAR(statusbar));
    g_return_if_fail(!media || ENTANGLE_IS_MEDIA(media));

    if (statusbar->media) {
        g_signal_handler_disconnect(statusbar->media, statusbar->mediaNotifyID);
        g_object_unref(statusbar->media);
    }
    statusbar->media = media;
    if (statusbar->media) {
        g_object_ref(statusbar->media);
        statusbar->mediaNotifyID = g_signal_connect(
            statusbar->media, "notify::metadata",
            G_CALLBACK(entangle_media_statusbar_media_metadata_notify),
            statusbar);
    }

    entangle_media_statusbar_update_labels(statusbar);
    gtk_widget_queue_draw(GTK_WIDGET(statusbar));
}

/**
 * entangle_media_statusbar_get_media:
 * @statusbar: (transfer none): the status bar widget
 *
 * Retrieve the media that status is currently displayed for
 *
 * Returns: (transfer none): the media displayed
 */
EntangleMedia *
entangle_media_statusbar_get_media(EntangleMediaStatusbar *statusbar)
{
    g_return_val_if_fail(ENTANGLE_IS_MEDIA_STATUSBAR(statusbar), NULL);

    return statusbar->media;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
