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

#ifndef __ENTANGLE_CAMERA_MANAGER_H__
#define __ENTANGLE_CAMERA_MANAGER_H__

#include <gtk/gtk.h>

#include "entangle-application.h"
#include "entangle-camera.h"
#include "entangle-script.h"

G_BEGIN_DECLS

#define ENTANGLE_TYPE_CAMERA_MANAGER (entangle_camera_manager_get_type())
G_DECLARE_FINAL_TYPE(EntangleCameraManager,
                     entangle_camera_manager,
                     ENTANGLE,
                     CAMERA_MANAGER,
                     GtkApplicationWindow)

EntangleCameraManager *
entangle_camera_manager_new(EntangleApplication *app);

void
entangle_camera_manager_add_script(EntangleCameraManager *manager,
                                   EntangleScript *script);
void
entangle_camera_manager_remove_script(EntangleCameraManager *manager,
                                      EntangleScript *script);

void
entangle_camera_manager_capture(EntangleCameraManager *manager);
void
entangle_camera_manager_preview_begin(EntangleCameraManager *manager);
void
entangle_camera_manager_preview_cancel(EntangleCameraManager *manager);

void
entangle_camera_manager_set_camera(EntangleCameraManager *manager,
                                   EntangleCamera *cam);
EntangleCamera *
entangle_camera_manager_get_camera(EntangleCameraManager *manager);

void
entangle_camera_manager_set_media_context_menu(EntangleCameraManager *manager,
                                               gboolean enabled);
gboolean
entangle_camera_manager_get_media_context_menu(EntangleCameraManager *manager);

G_END_DECLS

#endif /* __ENTANGLE_CAMERA_MANAGER_H__ */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
