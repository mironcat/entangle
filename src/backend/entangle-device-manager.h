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

#ifndef __ENTANGLE_DEVICE_MANAGER_H__
#define __ENTANGLE_DEVICE_MANAGER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define ENTANGLE_TYPE_DEVICE_MANAGER (entangle_device_manager_get_type())
G_DECLARE_FINAL_TYPE(EntangleDeviceManager,
                     entangle_device_manager,
                     ENTANGLE,
                     DEVICE_MANAGER,
                     GObject)

EntangleDeviceManager *
entangle_device_manager_new(void);

char *
entangle_device_manager_serial_id(EntangleDeviceManager *manager,
                                  const char *devpath);

G_END_DECLS

#endif /* __ENTANGLE_DEVICE_MANAGER_H__ */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
