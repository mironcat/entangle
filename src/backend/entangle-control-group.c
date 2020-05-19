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

#include <stdio.h>

#include "entangle-control-group.h"

#include "entangle-debug.h"

/**
 * SECTION:entangle-control-group
 * @Short_description: a device control that groups a set of child controls
 * @Title: EntangleControlGroup
 *
 * The #EntangleControlGroup object provides a device control that is a
 * container for a further set of device controls.
 */

struct _EntangleControlGroup
{
    EntangleControl parent;
    size_t ncontrol;
    EntangleControl **controls;
};

G_DEFINE_TYPE(EntangleControlGroup,
              entangle_control_group,
              ENTANGLE_TYPE_CONTROL);

static void
entangle_control_group_finalize(GObject *object)
{
    EntangleControlGroup *control = ENTANGLE_CONTROL_GROUP(object);

    for (int i = 0; i < control->ncontrol; i++) {
        g_object_unref(control->controls[i]);
    }
    g_free(control->controls);

    G_OBJECT_CLASS(entangle_control_group_parent_class)->finalize(object);
}

static void
entangle_control_group_class_init(EntangleControlGroupClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = entangle_control_group_finalize;
}

EntangleControlGroup *
entangle_control_group_new(const gchar *path,
                           gint id,
                           const gchar *label,
                           const gchar *info,
                           gboolean readonly)
{
    g_return_val_if_fail(path != NULL, NULL);
    g_return_val_if_fail(label != NULL, NULL);

    return ENTANGLE_CONTROL_GROUP(
        g_object_new(ENTANGLE_TYPE_CONTROL_GROUP, "path", path, "id", id,
                     "label", label, "info", info, "readonly", readonly, NULL));
}

static void
entangle_control_group_init(EntangleControlGroup *control G_GNUC_UNUSED)
{}

/**
 * entangle_control_group_add:
 * @group: (transfer none): the group to add to
 * @control: (transfer none): the child control to add
 *
 * Adds the @control to @group
 */
void
entangle_control_group_add(EntangleControlGroup *group,
                           EntangleControl *control)
{
    g_return_if_fail(ENTANGLE_IS_CONTROL_GROUP(group));
    g_return_if_fail(ENTANGLE_IS_CONTROL(control));

    group->controls =
        g_renew(EntangleControl *, group->controls, group->ncontrol + 1);
    group->controls[group->ncontrol++] = control;
    g_object_ref(control);
}

/**
 * entangle_control_group_count:
 * @group: (transfer none): the control group
 *
 * Get the number of controls which are immediate
 * children of this group
 *
 * Returns: the number of child controls
 */
guint
entangle_control_group_count(EntangleControlGroup *group)
{
    g_return_val_if_fail(ENTANGLE_IS_CONTROL_GROUP(group), 0);

    return group->ncontrol;
}

/**
 * entangle_control_group_get:
 * @group: (transfer none): the control group
 * @idx: the index of the control to fetch
 *
 * Returns: (transfer none): the control at index @idx, or NULL
 */
EntangleControl *
entangle_control_group_get(EntangleControlGroup *group, gint idx)
{
    g_return_val_if_fail(ENTANGLE_IS_CONTROL_GROUP(group), NULL);

    if (idx < 0 || idx >= group->ncontrol)
        return NULL;

    return group->controls[idx];
}

/**
 * entangle_control_group_get_by_path:
 * @group: the control group
 * @path: unique path of the control
 *
 * Get the control which has the path @path
 *
 * Returns: (transfer none): the control with path @path, or NULL
 */
EntangleControl *
entangle_control_group_get_by_path(EntangleControlGroup *group,
                                   const gchar *path)
{
    g_return_val_if_fail(ENTANGLE_IS_CONTROL_GROUP(group), NULL);
    g_return_val_if_fail(path != NULL, NULL);

    size_t i;

    for (i = 0; i < group->ncontrol; i++) {
        if (g_str_equal(path, entangle_control_get_path(group->controls[i]))) {
            return group->controls[i];
        } else if (ENTANGLE_IS_CONTROL_GROUP(group->controls[i])) {
            EntangleControl *control = entangle_control_group_get_by_path(
                ENTANGLE_CONTROL_GROUP(group->controls[i]), path);
            if (control)
                return control;
        }
    }

    return NULL;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
