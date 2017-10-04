/*
 *  Entangle: Tethered Camera Control & Capture
 *
 *  Copyright (C) 2009-2017 Daniel P. Berrange
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

#define ENTANGLE_TYPE_MEDIA_POPUP            (entangle_media_popup_get_type ())
#define ENTANGLE_MEDIA_POPUP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), ENTANGLE_TYPE_MEDIA_POPUP, EntangleMediaPopup))
#define ENTANGLE_MEDIA_POPUP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), ENTANGLE_TYPE_MEDIA_POPUP, EntangleMediaPopupClass))
#define ENTANGLE_IS_MEDIA_POPUP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ENTANGLE_TYPE_MEDIA_POPUP))
#define ENTANGLE_IS_MEDIA_POPUP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), ENTANGLE_TYPE_MEDIA_POPUP))
#define ENTANGLE_MEDIA_POPUP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), ENTANGLE_TYPE_MEDIA_POPUP, EntangleMediaPopupClass))


typedef struct _EntangleMediaPopup EntangleMediaPopup;
typedef struct _EntangleMediaPopupClass EntangleMediaPopupClass;

struct _EntangleMediaPopup
{
    GtkWindow parent;
};

struct _EntangleMediaPopupClass
{
    GtkWindowClass parent_class;
};


GType entangle_media_popup_get_type(void) G_GNUC_CONST;

EntangleMediaPopup* entangle_media_popup_new(void);

void entangle_media_popup_show(EntangleMediaPopup *popup,
                               GtkWindow *parent,
                               int x, int y);
void entangle_media_popup_move_to_monitor(EntangleMediaPopup *popup, gint monitor);
void entangle_media_popup_show_on_monitor(EntangleMediaPopup *popup, gint monitor);

void entangle_media_popup_set_media(EntangleMediaPopup *popup, EntangleMedia *media);
EntangleMedia *entangle_media_popup_get_media(EntangleMediaPopup *popup);

void entangle_media_popup_set_background(EntangleMediaPopup *popup,
                                         const gchar *background);
gchar *entangle_media_popup_get_background(EntangleMediaPopup *popup);


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
