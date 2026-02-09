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

#ifndef __ENTANGLE_SCRIPT_H__
#define __ENTANGLE_SCRIPT_H__

#include <gtk/gtk.h>

#include "entangle-camera-automata.h"

G_BEGIN_DECLS

#define ENTANGLE_TYPE_SCRIPT (entangle_script_get_type())
G_DECLARE_DERIVABLE_TYPE(EntangleScript,
                         entangle_script,
                         ENTANGLE,
                         SCRIPT,
                         GObject)

struct _EntangleScriptClass
{
    /*< private >*/
    GObjectClass parent_class;

    /*< public >*/
    GtkWidget *(*get_config_widget)(EntangleScript *script);
    void (*execute_async)(EntangleScript *script,
                          EntangleCameraAutomata *automata,
                          GCancellable *cancel,
                          GAsyncReadyCallback callback,
                          gpointer user_data);
    gboolean (*execute_finish)(EntangleScript *script,
                               GAsyncResult *result,
                               GError **error);
};

const gchar *
entangle_script_get_title(EntangleScript *script);
GtkWidget *
entangle_script_get_config_widget(EntangleScript *script);
void
entangle_script_execute_async(EntangleScript *script,
                              EntangleCameraAutomata *automata,
                              GCancellable *cancel,
                              GAsyncReadyCallback callback,
                              gpointer user_data);
gboolean
entangle_script_execute_finish(EntangleScript *script,
                               GAsyncResult *result,
                               GError **error);

G_END_DECLS

#endif /* __ENTANGLE_SCRIPT_H__ */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
