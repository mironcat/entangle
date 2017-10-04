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

#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "entangle-debug.h"
#include "entangle-media.h"

#define ENTANGLE_MEDIA_GET_PRIVATE(obj)                                     \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), ENTANGLE_TYPE_MEDIA, EntangleMediaPrivate))

struct _EntangleMediaPrivate {
    char *filename;
    GExiv2Metadata *metadata;

    gboolean dirty;
    struct stat st;
};

G_DEFINE_TYPE(EntangleMedia, entangle_media, G_TYPE_OBJECT);

enum {
    PROP_0,
    PROP_FILENAME,
    PROP_METADATA,
};

static void entangle_media_get_property(GObject *object,
                                        guint prop_id,
                                        GValue *value,
                                        GParamSpec *pspec)
{
    EntangleMedia *picker = ENTANGLE_MEDIA(object);
    EntangleMediaPrivate *priv = picker->priv;

    switch (prop_id)
        {
        case PROP_FILENAME:
            g_value_set_string(value, priv->filename);
            break;

        case PROP_METADATA:
            g_value_set_object(value, priv->metadata);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        }
}

static void entangle_media_set_property(GObject *object,
                                        guint prop_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
    EntangleMedia *media = ENTANGLE_MEDIA(object);
    EntangleMediaPrivate *priv = media->priv;

    switch (prop_id)
        {
        case PROP_FILENAME:
            g_free(priv->filename);
            priv->filename = g_value_dup_string(value);
            priv->dirty = TRUE;
            break;

        case PROP_METADATA:
            if (priv->metadata)
                g_object_unref(priv->metadata);
            priv->metadata = g_value_get_object(value);
            if (priv->metadata)
                g_object_ref(priv->metadata);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        }
}


static void entangle_media_finalize(GObject *object)
{
    EntangleMedia *media = ENTANGLE_MEDIA(object);
    EntangleMediaPrivate *priv = media->priv;

    ENTANGLE_DEBUG("Finalize media %p", object);

    if (priv->metadata)
        g_object_unref(priv->metadata);

    g_free(priv->filename);

    G_OBJECT_CLASS(entangle_media_parent_class)->finalize(object);
}


static void entangle_media_class_init(EntangleMediaClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = entangle_media_finalize;
    object_class->get_property = entangle_media_get_property;
    object_class->set_property = entangle_media_set_property;

    g_object_class_install_property(object_class,
                                    PROP_FILENAME,
                                    g_param_spec_string("filename",
                                                        "Media filename",
                                                        "Full path to media file",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

    g_object_class_install_property(object_class,
                                    PROP_METADATA,
                                    g_param_spec_object("metadata",
                                                        "Media metadata",
                                                        "Media metadata",
                                                        GEXIV2_TYPE_METADATA,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

    g_type_class_add_private(klass, sizeof(EntangleMediaPrivate));
}


static void entangle_media_init(EntangleMedia *media)
{
    EntangleMediaPrivate *priv;

    priv = media->priv = ENTANGLE_MEDIA_GET_PRIVATE(media);

    priv->dirty = TRUE;
}


/**
 * entangle_media_get_filename:
 * @media: (transfer none): the media instance
 *
 * Get the filename associated with the media, if it has one.
 *
 * Returns: (transfer none): the filename or NULL
 */
const char *entangle_media_get_filename(EntangleMedia *media)
{
    g_return_val_if_fail(ENTANGLE_IS_MEDIA(media), NULL);

    EntangleMediaPrivate *priv = media->priv;
    return priv->filename;
}


static gboolean entangle_media_load(EntangleMedia *media)
{
    EntangleMediaPrivate *priv = media->priv;

    if (!priv->dirty)
        return TRUE;

    if (stat(priv->filename, &priv->st) < 0) {
        memset(&priv->st, 0, sizeof priv->st);
        return FALSE;
    }

    priv->dirty = FALSE;
    return TRUE;
}


/**
 * entangle_media_get_last_modified:
 * @media: (transfer none): the media instance
 *
 * Get the time at which the media was last modified, if
 * it is backed by a file on disk
 *
 * Returns: the last modification time in seconds since epoch, or 0
 */
time_t entangle_media_get_last_modified(EntangleMedia *media)
{
    g_return_val_if_fail(ENTANGLE_IS_MEDIA(media), 0);

    EntangleMediaPrivate *priv = media->priv;

    if (!entangle_media_load(media))
        return 0;

    return priv->st.st_mtime;
}


/**
 * entangle_media_get_file_size:
 * @media: (transfer none): the media instance
 *
 * Get the size of the media on disk, if it is backed by
 * a file on disk
 *
 * Returns: the size in bytes, or 0
 */
off_t entangle_media_get_file_size(EntangleMedia *media)
{
    g_return_val_if_fail(ENTANGLE_IS_MEDIA(media), 0);

    EntangleMediaPrivate *priv = media->priv;

    if (!entangle_media_load(media))
        return 0;

    return priv->st.st_size;
}


/**
 * entange_media_delete:
 * @media: (transfer none): the media instance
 *
 * Delete the file on disk.
 *
 * Returns: TRUE if the file was deleted
 */
gboolean entangle_media_delete(EntangleMedia *media, GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_MEDIA(media), FALSE);

    EntangleMediaPrivate *priv = media->priv;
    GFile *file = g_file_new_for_path(priv->filename);

    return g_file_delete(file, NULL, error);
}


/**
 * entangle_media_get_metadata:
 * @media: (transfer none): the media instance
 *
 * Get the metadata associated with the media, if it is available
 *
 * Returns: (transfer none): the media metadata or NULL
 */
GExiv2Metadata *entangle_media_get_metadata(EntangleMedia *media)
{
    g_return_val_if_fail(ENTANGLE_IS_MEDIA(media), NULL);

    EntangleMediaPrivate *priv = media->priv;
    return priv->metadata;
}


/**
 * entangle_media_set_metadata:
 * @media: (transfer none): the media instance
 * @metadata: (transfer none): the new metadata
 *
 * Set the metadata associated with the media
 */
void entangle_media_set_metadata(EntangleMedia *media,
                                 GExiv2Metadata *metadata)
{
    g_return_if_fail(ENTANGLE_IS_MEDIA(media));

    g_object_set(media, "metadata", metadata, NULL);
}



/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
