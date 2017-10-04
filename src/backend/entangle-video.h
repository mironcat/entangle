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

#ifndef __ENTANGLE_VIDEO_H__
#define __ENTANGLE_VIDEO_H__

#include "entangle-media.h"
#include <gst/base/base.h>

G_BEGIN_DECLS

#define ENTANGLE_TYPE_VIDEO            (entangle_video_get_type ())
#define ENTANGLE_VIDEO(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), ENTANGLE_TYPE_VIDEO, EntangleVideo))
#define ENTANGLE_VIDEO_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), ENTANGLE_TYPE_VIDEO, EntangleVideoClass))
#define ENTANGLE_IS_VIDEO(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ENTANGLE_TYPE_VIDEO))
#define ENTANGLE_IS_VIDEO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), ENTANGLE_TYPE_VIDEO))
#define ENTANGLE_VIDEO_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), ENTANGLE_TYPE_VIDEO, EntangleVideoClass))


typedef struct _EntangleVideo EntangleVideo;
typedef struct _EntangleVideoPrivate EntangleVideoPrivate;
typedef struct _EntangleVideoClass EntangleVideoClass;

struct _EntangleVideo
{
    EntangleMedia parent;

    EntangleVideoPrivate *priv;
};

struct _EntangleVideoClass
{
    EntangleMediaClass parent_class;
};


GType entangle_video_get_type(void) G_GNUC_CONST;

EntangleVideo *entangle_video_new_file(const char *filename);
EntangleVideo *entangle_video_new_source(GstBaseSrc *source);

GstBaseSrc *entangle_video_get_source(EntangleVideo *video);
void entangle_video_set_source(EntangleVideo *video,
                               GstBaseSrc *source);

G_END_DECLS

#endif /* __ENTANGLE_VIDEO_H__ */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
