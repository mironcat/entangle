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

#ifndef __ENTANGLE_VIDEO_H__
#define __ENTANGLE_VIDEO_H__

#include <gst/base/base.h>

#include "entangle-media.h"

G_BEGIN_DECLS

#define ENTANGLE_TYPE_VIDEO (entangle_video_get_type())
G_DECLARE_FINAL_TYPE(EntangleVideo,
                     entangle_video,
                     ENTANGLE,
                     VIDEO,
                     EntangleMedia)

EntangleVideo *
entangle_video_new_file(const char *filename);
EntangleVideo *
entangle_video_new_source(GstBaseSrc *source);

GstBaseSrc *
entangle_video_get_source(EntangleVideo *video);
void
entangle_video_set_source(EntangleVideo *video, GstBaseSrc *source);

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
