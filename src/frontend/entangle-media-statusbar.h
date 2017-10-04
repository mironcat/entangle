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

#ifndef __ENTANGLE_MEDIA_STATUSBAR_H__
#define __ENTANGLE_MEDIA_STATUSBAR_H__

#include <gtk/gtk.h>

#include "entangle-media.h"

G_BEGIN_DECLS

#define ENTANGLE_TYPE_MEDIA_STATUSBAR            (entangle_media_statusbar_get_type ())
#define ENTANGLE_MEDIA_STATUSBAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), ENTANGLE_TYPE_MEDIA_STATUSBAR, EntangleMediaStatusbar))
#define ENTANGLE_MEDIA_STATUSBAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), ENTANGLE_TYPE_MEDIA_STATUSBAR, EntangleMediaStatusbarClass))
#define ENTANGLE_IS_MEDIA_STATUSBAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ENTANGLE_TYPE_MEDIA_STATUSBAR))
#define ENTANGLE_IS_MEDIA_STATUSBAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), ENTANGLE_TYPE_MEDIA_STATUSBAR))
#define ENTANGLE_MEDIA_STATUSBAR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), ENTANGLE_TYPE_MEDIA_STATUSBAR, EntangleMediaStatusbarClass))


typedef struct _EntangleMediaStatusbar EntangleMediaStatusbar;
typedef struct _EntangleMediaStatusbarPrivate EntangleMediaStatusbarPrivate;
typedef struct _EntangleMediaStatusbarClass EntangleMediaStatusbarClass;

struct _EntangleMediaStatusbar
{
    GtkBox parent;

    EntangleMediaStatusbarPrivate *priv;
};

struct _EntangleMediaStatusbarClass
{
    GtkEventBoxClass parent_class;

};

GType entangle_media_statusbar_get_type(void) G_GNUC_CONST;

EntangleMediaStatusbar* entangle_media_statusbar_new(void);

void entangle_media_statusbar_set_media(EntangleMediaStatusbar *statusbar,
                                        EntangleMedia *media);
EntangleMedia *entangle_media_statusbar_get_media(EntangleMediaStatusbar *statusbar);

G_END_DECLS

#endif /* __ENTANGLE_MEDIA_STATUSBAR_H__ */


/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
