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

#ifndef __ENTANGLE_CAMERA_PREFERENCES_H__
#define __ENTANGLE_CAMERA_PREFERENCES_H__

#include <glib-object.h>

#include "entangle-camera.h"

G_BEGIN_DECLS

#define ENTANGLE_TYPE_CAMERA_PREFERENCES                                       \
    (entangle_camera_preferences_get_type())
G_DECLARE_FINAL_TYPE(EntangleCameraPreferences,
                     entangle_camera_preferences,
                     ENTANGLE,
                     CAMERA_PREFERENCES,
                     GObject)

EntangleCameraPreferences *
entangle_camera_preferences_new(void);

void
entangle_camera_preferences_set_camera(EntangleCameraPreferences *preferences,
                                       EntangleCamera *camera);
EntangleCamera *
entangle_camera_preferences_get_camera(EntangleCameraPreferences *preferences);

gchar **
entangle_camera_preferences_get_controls(EntangleCameraPreferences *prefs);
void
entangle_camera_preferences_set_controls(EntangleCameraPreferences *prefs,
                                         const gchar *const *controls);

G_END_DECLS

#endif /* __ENTANGLE_CAMERA_PREFERENCES_H__ */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
