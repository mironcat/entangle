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

#ifndef __ENTANGLE_CAMERA_AUTOMATA_H__
#define __ENTANGLE_CAMERA_AUTOMATA_H__

#include <glib-object.h>

#include "entangle-camera.h"
#include "entangle-image.h"
#include "entangle-session.h"

G_BEGIN_DECLS

#define ENTANGLE_TYPE_CAMERA_AUTOMATA (entangle_camera_automata_get_type())
G_DECLARE_FINAL_TYPE(EntangleCameraAutomata,
                     entangle_camera_automata,
                     ENTANGLE,
                     CAMERA_AUTOMATA,
                     GObject)

EntangleCameraAutomata *
entangle_camera_automata_new(void);

void
entangle_camera_automata_capture_async(EntangleCameraAutomata *automata,
                                       GCancellable *cancel,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data);

gboolean
entangle_camera_automata_capture_finish(EntangleCameraAutomata *automata,
                                        GAsyncResult *res,
                                        GError **error);

void
entangle_camera_automata_preview_async(EntangleCameraAutomata *automata,
                                       GCancellable *cancel,
                                       GCancellable *confirm,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data);

gboolean
entangle_camera_automata_preview_finish(EntangleCameraAutomata *automata,
                                        GAsyncResult *res,
                                        GError **error);

void
entangle_camera_automata_set_camera(EntangleCameraAutomata *automata,
                                    EntangleCamera *camera);
EntangleCamera *
entangle_camera_automata_get_camera(EntangleCameraAutomata *automata);

void
entangle_camera_automata_set_session(EntangleCameraAutomata *automata,
                                     EntangleSession *session);
EntangleSession *
entangle_camera_automata_get_session(EntangleCameraAutomata *automata);

void
entangle_camera_automata_set_delete_file(EntangleCameraAutomata *automata,
                                         gboolean value);
gboolean
entangle_camera_automata_get_delete_file(EntangleCameraAutomata *automata);
void
entangle_camera_automata_set_capture_rotation(EntangleCameraAutomata *automata,
                                              gint rotation);
void
entangle_camera_automata_set_capture_transform(EntangleCameraAutomata *automata,
                                               gint rotation,
                                               gboolean flipH,
                                               gboolean flipV);

G_END_DECLS

#endif /* __ENTANGLE_CAMERA_H__ */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
