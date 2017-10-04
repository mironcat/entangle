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
#include "entangle-video.h"

#define ENTANGLE_VIDEO_GET_PRIVATE(obj)                                     \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), ENTANGLE_TYPE_VIDEO, EntangleVideoPrivate))

struct _EntangleVideoPrivate {
    GstBaseSrc *source;
};

G_DEFINE_TYPE(EntangleVideo, entangle_video, ENTANGLE_TYPE_MEDIA);

enum {
    PROP_0,
    PROP_SOURCE,
};

static void entangle_video_get_property(GObject *object,
                                        guint prop_id,
                                        GValue *value,
                                        GParamSpec *pspec)
{
    EntangleVideo *picker = ENTANGLE_VIDEO(object);
    EntangleVideoPrivate *priv = picker->priv;

    switch (prop_id)
        {
        case PROP_SOURCE:
            g_value_set_object(value, priv->source);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        }
}

static void entangle_video_set_property(GObject *object,
                                        guint prop_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
    EntangleVideo *video = ENTANGLE_VIDEO(object);
    EntangleVideoPrivate *priv = video->priv;

    switch (prop_id)
        {
        case PROP_SOURCE:
            if (priv->source)
                g_object_unref(priv->source);
            priv->source = g_value_get_object(value);
            if (priv->source)
                g_object_ref(priv->source);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        }
}


static void entangle_video_finalize(GObject *object)
{
    EntangleVideo *video = ENTANGLE_VIDEO(object);
    EntangleVideoPrivate *priv = video->priv;

    ENTANGLE_DEBUG("Finalize video %p", object);

    if (priv->source)
        g_object_unref(priv->source);

    G_OBJECT_CLASS(entangle_video_parent_class)->finalize(object);
}


static void entangle_video_class_init(EntangleVideoClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = entangle_video_finalize;
    object_class->get_property = entangle_video_get_property;
    object_class->set_property = entangle_video_set_property;

    g_object_class_install_property(object_class,
                                    PROP_SOURCE,
                                    g_param_spec_object("source",
                                                        "Video source",
                                                        "Video source",
                                                        GST_TYPE_BASE_SRC,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

    g_type_class_add_private(klass, sizeof(EntangleVideoPrivate));
}


/**
 * entangle_video_new_file:
 * @filename: (transfer none): the filename of the video
 *
 * Create a new video instance for the video stored in
 * @filename.
 *
 * Returns: (transfer full): the new video
 */
EntangleVideo *entangle_video_new_file(const char *filename)
{
    return ENTANGLE_VIDEO(g_object_new(ENTANGLE_TYPE_VIDEO,
                                       "filename", filename,
                                       NULL));
}

EntangleVideo *entangle_video_new_source(GstBaseSrc *source)
{
    return ENTANGLE_VIDEO(g_object_new(ENTANGLE_TYPE_VIDEO,
                                       "source", source,
                                       NULL));
}

static void entangle_video_init(EntangleVideo *video)
{
    video->priv = ENTANGLE_VIDEO_GET_PRIVATE(video);
}


/**
 * entangle_video_get_source:
 * @video: (transfer none): the video to get the source for
 *
 * Get the live video source element
 *
 * Returns: (transfer none): the video source
 */
GstBaseSrc *entangle_video_get_source(EntangleVideo *video)
{
    EntangleVideoPrivate *priv = video->priv;

    return priv->source;
}

/**
 * entangle_video_set_source:
 * @video: (transfer none): the video to set the source for
 * @source: (transfer none): the new video source or NULL
 *
 * Set the live video source element
 */
void entangle_video_set_source(EntangleVideo *video,
                               GstBaseSrc *source)
{
    EntangleVideoPrivate *priv = video->priv;

    if (priv->source)
        g_object_unref(source);
    priv->source = source;
    if (priv->source)
        g_object_ref(source);
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
