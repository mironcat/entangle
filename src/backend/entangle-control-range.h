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

#ifndef __ENTANGLE_CONTROL_RANGE_H__
#define __ENTANGLE_CONTROL_RANGE_H__

#include <glib-object.h>

#include "entangle-control.h"

G_BEGIN_DECLS

#define ENTANGLE_TYPE_CONTROL_RANGE (entangle_control_range_get_type())
G_DECLARE_FINAL_TYPE(EntangleControlRange,
                     entangle_control_range,
                     ENTANGLE,
                     CONTROL_RANGE,
                     EntangleControl)

EntangleControlRange *
entangle_control_range_new(const char *path,
                           int id,
                           const char *label,
                           const char *info,
                           gboolean readonly,
                           float min,
                           float max,
                           float step);

float
entangle_control_range_get_min(EntangleControlRange *range);
float
entangle_control_range_get_max(EntangleControlRange *range);
float
entangle_control_range_get_step(EntangleControlRange *range);

G_END_DECLS

#endif /* __ENTANGLE_CONTROL_RANGE_H__ */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
