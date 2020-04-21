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

#ifndef __ENTANGLE_THUMBNAIL_LOADER_H__
#define __ENTANGLE_THUMBNAIL_LOADER_H__

#include <glib-object.h>
#include "entangle-pixbuf-loader.h"

G_BEGIN_DECLS

#define ENTANGLE_TYPE_THUMBNAIL_LOADER            (entangle_thumbnail_loader_get_type ())
G_DECLARE_FINAL_TYPE(EntangleThumbnailLoader, entangle_thumbnail_loader, ENTANGLE, THUMBNAIL_LOADER, EntanglePixbufLoader)

EntangleThumbnailLoader *entangle_thumbnail_loader_new(int width,
                                                       int height);

G_END_DECLS

#endif /* __ENTANGLE_THUMBNAIL_LOADER_H__ */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
