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

#ifndef __ENTANGLE_IMAGE_HISTOGRAM_H__
#define __ENTANGLE_IMAGE_HISTOGRAM_H__

#include <gtk/gtk.h>

#include "entangle-image-loader.h"

G_BEGIN_DECLS

#define ENTANGLE_TYPE_IMAGE_HISTOGRAM            (entangle_image_histogram_get_type ())
G_DECLARE_FINAL_TYPE(EntangleImageHistogram, entangle_image_histogram, ENTANGLE, IMAGE_HISTOGRAM, GtkDrawingArea)

EntangleImageHistogram *entangle_image_histogram_new(void);

void entangle_image_histogram_set_image(EntangleImageHistogram *histogram,
                                        EntangleImage *image);
EntangleImage *entangle_image_histogram_get_image(EntangleImageHistogram *histogram);

void entangle_image_histogram_set_histogram_linear(EntangleImageHistogram *histogram,
                                                   gboolean linear);
gboolean entangle_image_histogram_get_histogram_linear(EntangleImageHistogram *histogram);

G_END_DECLS

#endif /* __ENTANGLE_IMAGE_HISTOGRAM_H__ */


/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
