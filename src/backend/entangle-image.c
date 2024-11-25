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

#include <stdio.h>
#include <string.h>

#include <glib.h>

#include "entangle-image.h"

#include "entangle-debug.h"

/**
 * SECTION:entangle-image
 * @Short_description: a media file representing a static image
 * @Title: EntangleImage
 *
 * The #EntangleImage object provides a media file that represents a static
 * image.
 */

struct _EntangleImage
{
    EntangleMedia parent;
    GdkPixbuf *pixbuf;
};

G_DEFINE_TYPE(EntangleImage, entangle_image, ENTANGLE_TYPE_MEDIA);

enum
{
    PROP_0,
    PROP_PIXBUF,
};

static void
entangle_image_get_property(GObject *object,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec)
{
    EntangleImage *image = ENTANGLE_IMAGE(object);

    switch (prop_id) {
    case PROP_PIXBUF:
        g_value_set_object(value, image->pixbuf);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
entangle_image_set_property(GObject *object,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
    EntangleImage *image = ENTANGLE_IMAGE(object);

    switch (prop_id) {
    case PROP_PIXBUF:
        if (image->pixbuf)
            g_object_unref(image->pixbuf);
        image->pixbuf = g_value_get_object(value);
        if (image->pixbuf)
            g_object_ref(image->pixbuf);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
entangle_image_finalize(GObject *object)
{
    EntangleImage *image = ENTANGLE_IMAGE(object);

    ENTANGLE_DEBUG("Finalize image %p", object);

    if (image->pixbuf)
        g_object_unref(image->pixbuf);

    G_OBJECT_CLASS(entangle_image_parent_class)->finalize(object);
}

static void
entangle_image_class_init(EntangleImageClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = entangle_image_finalize;
    object_class->get_property = entangle_image_get_property;
    object_class->set_property = entangle_image_set_property;

    g_object_class_install_property(
        object_class, PROP_PIXBUF,
        g_param_spec_object("pixbuf", "Image pixbuf", "Image pixbuf",
                            GDK_TYPE_PIXBUF,
                            G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
}

/**
 * entangle_image_new_file:
 * @filename: (transfer none): the filename of the image
 *
 * Create a new image instance for the image stored in
 * @filename.
 *
 * Returns: (transfer full): the new image
 */
EntangleImage *
entangle_image_new_file(const char *filename)
{
    return ENTANGLE_IMAGE(
        g_object_new(ENTANGLE_TYPE_IMAGE, "filename", filename, NULL));
}

/**
 * entangle_image_new_pixbuf:
 * @pixbuf: (transfer none): the pixbuf instance
 *
 * Create a new image instance for the image data stored
 * in @pixbuf
 *
 * Returns: (transfer full): the new image
 */
EntangleImage *
entangle_image_new_pixbuf(GdkPixbuf *pixbuf)
{
    return ENTANGLE_IMAGE(
        g_object_new(ENTANGLE_TYPE_IMAGE, "pixbuf", pixbuf, NULL));
}

static void
entangle_image_init(EntangleImage *image G_GNUC_UNUSED)
{
}

/**
 * entangle_image_get_pixbuf:
 * @image: (transfer none): the image instance
 *
 * Get the pixbuf associated with the image, if it is available
 *
 * Returns: (transfer none): the image pixbuf or NULL
 */
GdkPixbuf *
entangle_image_get_pixbuf(EntangleImage *image)
{
    g_return_val_if_fail(ENTANGLE_IS_IMAGE(image), NULL);

    return image->pixbuf;
}

/**
 * entangle_image_set_pixbuf:
 * @image: (transfer none): the image instance
 * @pixbuf: (transfer none): the new pixbuf
 *
 * Set the pixbuf associated with the image
 */
void
entangle_image_set_pixbuf(EntangleImage *image, GdkPixbuf *pixbuf)
{
    g_return_if_fail(ENTANGLE_IS_IMAGE(image));

    g_object_set(image, "pixbuf", pixbuf, NULL);
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
