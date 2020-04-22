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

#ifndef __ENTANGLE_MEDIA_POPUP_H__
#define __ENTANGLE_MEDIA_POPUP_H__

#include <glib-object.h>

#include "entangle-media.h"

G_BEGIN_DECLS

#define ENTANGLE_TYPE_MEDIA_POPUP (entangle_media_popup_get_type())
G_DECLARE_FINAL_TYPE(EntangleMediaPopup,
                     entangle_media_popup,
                     ENTANGLE,
                     MEDIA_POPUP,
                     GtkWindow)

EntangleMediaPopup *
entangle_media_popup_new(void);

void
entangle_media_popup_show(EntangleMediaPopup *popup,
                          GtkWindow *parent,
                          int x,
                          int y);
void
entangle_media_popup_move_to_monitor(EntangleMediaPopup *popup, gint monitor);
void
entangle_media_popup_show_on_monitor(EntangleMediaPopup *popup, gint monitor);

void
entangle_media_popup_set_media(EntangleMediaPopup *popup, EntangleMedia *media);
EntangleMedia *
entangle_media_popup_get_media(EntangleMediaPopup *popup);

void
entangle_media_popup_set_background(EntangleMediaPopup *popup,
                                    const gchar *background);
gchar *
entangle_media_popup_get_background(EntangleMediaPopup *popup);

G_END_DECLS

#endif /* __ENTANGLE_MEDIA_POPUP_H__ */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
