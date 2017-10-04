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

#ifndef __ENTANGLE_MEDIA_H__
#define __ENTANGLE_MEDIA_H__

#include <glib-object.h>
#include <gexiv2/gexiv2.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "entangle-control-group.h"

G_BEGIN_DECLS

#define ENTANGLE_TYPE_MEDIA            (entangle_media_get_type ())
#define ENTANGLE_MEDIA(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), ENTANGLE_TYPE_MEDIA, EntangleMedia))
#define ENTANGLE_MEDIA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), ENTANGLE_TYPE_MEDIA, EntangleMediaClass))
#define ENTANGLE_IS_MEDIA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ENTANGLE_TYPE_MEDIA))
#define ENTANGLE_IS_MEDIA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), ENTANGLE_TYPE_MEDIA))
#define ENTANGLE_MEDIA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), ENTANGLE_TYPE_MEDIA, EntangleMediaClass))


typedef struct _EntangleMedia EntangleMedia;
typedef struct _EntangleMediaPrivate EntangleMediaPrivate;
typedef struct _EntangleMediaClass EntangleMediaClass;

struct _EntangleMedia
{
    GObject parent;

    EntangleMediaPrivate *priv;
};

struct _EntangleMediaClass
{
    GObjectClass parent_class;
};


GType entangle_media_get_type(void) G_GNUC_CONST;

const char *entangle_media_get_filename(EntangleMedia *media);

time_t entangle_media_get_last_modified(EntangleMedia *media);
off_t entangle_media_get_file_size(EntangleMedia *media);

gboolean entangle_media_delete(EntangleMedia *media, GError **error);

GExiv2Metadata *entangle_media_get_metadata(EntangleMedia *media);
void entangle_media_set_metadata(EntangleMedia *media,
                                 GExiv2Metadata *metadata);

G_END_DECLS

#endif /* __ENTANGLE_MEDIA_H__ */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
