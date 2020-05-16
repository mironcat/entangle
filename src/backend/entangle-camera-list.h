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

#ifndef __ENTANGLE_CAMERA_LIST_H__
#define __ENTANGLE_CAMERA_LIST_H__

#include <glib-object.h>

#include "entangle-camera.h"

G_BEGIN_DECLS

#define ENTANGLE_TYPE_CAMERA_LIST (entangle_camera_list_get_type())
G_DECLARE_FINAL_TYPE(EntangleCameraList,
                     entangle_camera_list,
                     ENTANGLE,
                     CAMERA_LIST,
                     GObject)

EntangleCameraList *
entangle_camera_list_new_active(void);
EntangleCameraList *
entangle_camera_list_new_supported(void);

gboolean
entangle_camera_list_refresh(EntangleCameraList *list, GError **error);

int
entangle_camera_list_count(EntangleCameraList *list);

void
entangle_camera_list_add(EntangleCameraList *list, EntangleCamera *cam);

void
entangle_camera_list_remove(EntangleCameraList *list, EntangleCamera *cam);

EntangleCamera *
entangle_camera_list_get(EntangleCameraList *list, int entry);

GList *
entangle_camera_list_get_cameras(EntangleCameraList *list);

EntangleCamera *
entangle_camera_list_find(EntangleCameraList *list, const char *port);

G_END_DECLS

#endif /* __ENTANGLE_CAMERA_LIST_H__ */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
