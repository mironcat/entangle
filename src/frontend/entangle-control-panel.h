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

#ifndef __ENTANGLE_CONTROL_PANEL_H__
#define __ENTANGLE_CONTROL_PANEL_H__

#include <gtk/gtk.h>

#include "entangle-camera-preferences.h"

G_BEGIN_DECLS

#define ENTANGLE_TYPE_CONTROL_PANEL (entangle_control_panel_get_type())
G_DECLARE_FINAL_TYPE(EntangleControlPanel,
                     entangle_control_panel,
                     ENTANGLE,
                     CONTROL_PANEL,
                     GtkExpander)

EntangleControlPanel *
entangle_control_panel_new(EntangleCameraPreferences *prefs);

EntangleCameraPreferences *
entangle_control_panel_get_camera_preferences(EntangleControlPanel *panel);

gboolean
entangle_control_panel_get_has_controls(EntangleControlPanel *panel);

G_END_DECLS

#endif /* __ENTANGLE_CONTROL_PANEL_H__ */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
