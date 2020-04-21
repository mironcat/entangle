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

#ifndef __ENTANGLE_CONTROL_GROUP_H__
#define __ENTANGLE_CONTROL_GROUP_H__

#include <glib-object.h>

#include "entangle-control.h"

G_BEGIN_DECLS

#define ENTANGLE_TYPE_CONTROL_GROUP            (entangle_control_group_get_type ())
G_DECLARE_FINAL_TYPE(EntangleControlGroup, entangle_control_group, ENTANGLE, CONTROL_GROUP, EntangleControl)

EntangleControlGroup* entangle_control_group_new(const gchar *path,
                                                 gint id,
                                                 const gchar *label,
                                                 const gchar *info,
                                                 gboolean readonly);

void entangle_control_group_add(EntangleControlGroup *group,
                            EntangleControl *control);

guint entangle_control_group_count(EntangleControlGroup *group);
EntangleControl *entangle_control_group_get(EntangleControlGroup *group, int idx);
EntangleControl *entangle_control_group_get_by_path(EntangleControlGroup *group,
                                                    const gchar *path);


G_END_DECLS

#endif /* __ENTANGLE_CONTROL_GROUP_H__ */


/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
