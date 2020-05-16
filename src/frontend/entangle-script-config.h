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

#ifndef __ENTANGLE_SCRIPT_CONFIG_H__
#define __ENTANGLE_SCRIPT_CONFIG_H__

#include <gtk/gtk.h>

#include "entangle-script.h"

G_BEGIN_DECLS

#define ENTANGLE_TYPE_SCRIPT_CONFIG (entangle_script_config_get_type())
G_DECLARE_FINAL_TYPE(EntangleScriptConfig,
                     entangle_script_config,
                     ENTANGLE,
                     SCRIPT_CONFIG,
                     GtkBox)

EntangleScriptConfig *
entangle_script_config_new(void);

void
entangle_script_config_add_script(EntangleScriptConfig *config,
                                  EntangleScript *script);
void
entangle_script_config_remove_script(EntangleScriptConfig *config,
                                     EntangleScript *script);

gboolean
entangle_script_config_has_scripts(EntangleScriptConfig *config);

EntangleScript *
entangle_script_config_get_selected(EntangleScriptConfig *config);

G_END_DECLS

#endif /* __ENTANGLE_SCRIPT_CONFIG_H__ */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
