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

#ifndef __ENTANGLE_APPLICATION_H__
#define __ENTANGLE_APPLICATION_H__

#include <gtk/gtk.h>
#include <libpeas/peas.h>

#include "entangle-camera-list.h"
#include "entangle-preferences.h"

G_BEGIN_DECLS

#define ENTANGLE_TYPE_APPLICATION (entangle_application_get_type())
G_DECLARE_FINAL_TYPE(EntangleApplication,
                     entangle_application,
                     ENTANGLE,
                     APPLICATION,
                     GtkApplication)

EntangleApplication *
entangle_application_new(void);

EntangleCameraList *
entangle_application_get_active_cameras(EntangleApplication *application);
EntangleCameraList *
entangle_application_get_supported_cameras(EntangleApplication *application);
EntanglePreferences *
entangle_application_get_preferences(EntangleApplication *application);
PeasEngine *
entangle_application_get_plugin_engine(EntangleApplication *application);

G_END_DECLS

#endif /* __ENTANGLE_APPLICATION_H__ */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
