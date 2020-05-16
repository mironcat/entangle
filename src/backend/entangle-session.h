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

#ifndef __ENTANGLE_SESSION_H__
#define __ENTANGLE_SESSION_H__

#include <glib-object.h>

#include "entangle-camera-file.h"
#include "entangle-media.h"

G_BEGIN_DECLS

#define ENTANGLE_TYPE_SESSION (entangle_session_get_type())
G_DECLARE_FINAL_TYPE(EntangleSession,
                     entangle_session,
                     ENTANGLE,
                     SESSION,
                     GObject)

EntangleSession *
entangle_session_new(const char *directory, const char *filenamePattern);

const char *
entangle_session_directory(EntangleSession *session);
const char *
entangle_session_filename_pattern(EntangleSession *session);

char *
entangle_session_next_filename(EntangleSession *session,
                               EntangleCameraFile *file);

gboolean
entangle_session_load(EntangleSession *session);

void
entangle_session_add_media(EntangleSession *session, EntangleMedia *media);
void
entangle_session_remove_media(EntangleSession *session, EntangleMedia *media);

int
entangle_session_get_media_count(EntangleSession *session);

EntangleMedia *
entangle_session_get_media(EntangleSession *session, int idx);

G_END_DECLS

#endif /* __ENTANGLE_SESSION_H__ */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
