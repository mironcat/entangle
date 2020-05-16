/*
 *  Entangle: Tethered Camera Control & Capture
 *
 *  Copyright (C) 2009-2014 Daniel P. Berrangé
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

#ifndef __ENTANGLE_SCRIPT_SIMPLE_H__
#define __ENTANGLE_SCRIPT_SIMPLE_H__

#include "entangle-script.h"

G_BEGIN_DECLS

#define ENTANGLE_TYPE_SCRIPT_SIMPLE (entangle_script_simple_get_type())
G_DECLARE_DERIVABLE_TYPE(EntangleScriptSimple,
                         entangle_script_simple,
                         ENTANGLE,
                         SCRIPT_SIMPLE,
                         EntangleScript)

struct _EntangleScriptSimpleClass
{
    EntangleScriptClass parent_class;

    void (*execute)(EntangleScriptSimple *script,
                    EntangleCameraAutomata *automata,
                    GCancellable *cancel,
                    GTask *result);

    GObject *(*init_task_data)(EntangleScriptSimple *script);
};

void
entangle_script_simple_return_task_error(EntangleScriptSimple *script,
                                         GTask *result,
                                         const gchar *message);

GObject *
entangle_script_simple_init_task_data(EntangleScriptSimple *script);
GObject *
entangle_script_simple_get_task_data(EntangleScriptSimple *script,
                                     GTask *result);

G_END_DECLS

#endif /* __ENTANGLE_SCRIPT_SIMPLE_H__ */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
