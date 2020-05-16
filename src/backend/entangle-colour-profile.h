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

#ifndef __ENTANGLE_COLOUR_PROFILE_H__
#define __ENTANGLE_COLOUR_PROFILE_H__

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib-object.h>

#include "entangle-colour-profile-enums.h"

G_BEGIN_DECLS

#define ENTANGLE_TYPE_COLOUR_PROFILE (entangle_colour_profile_get_type())
G_DECLARE_FINAL_TYPE(EntangleColourProfile,
                     entangle_colour_profile,
                     ENTANGLE,
                     COLOUR_PROFILE,
                     GObject)

#define ENTANGLE_TYPE_COLOUR_PROFILE_TRANSFORM                                 \
    (entangle_colour_profile_transform_get_type())
G_DECLARE_FINAL_TYPE(EntangleColourProfileTransform,
                     entangle_colour_profile_transform,
                     ENTANGLE,
                     COLOUR_PROFILE_TRANSFORM,
                     GObject)

typedef enum
{
    ENTANGLE_COLOUR_PROFILE_INTENT_PERCEPTUAL,
    ENTANGLE_COLOUR_PROFILE_INTENT_REL_COLOURIMETRIC,
    ENTANGLE_COLOUR_PROFILE_INTENT_SATURATION,
    ENTANGLE_COLOUR_PROFILE_INTENT_ABS_COLOURIMETRIC,
} EntangleColourProfileIntent;

EntangleColourProfile *
entangle_colour_profile_new_file(const char *filename);
EntangleColourProfile *
entangle_colour_profile_new_data(GByteArray *data);

const char *
entangle_colour_profile_filename(EntangleColourProfile *profile);

char *
entangle_colour_profile_description(EntangleColourProfile *profile);
char *
entangle_colour_profile_manufacturer(EntangleColourProfile *profile);
char *
entangle_colour_profile_model(EntangleColourProfile *profile);
char *
entangle_colour_profile_copyright(EntangleColourProfile *profile);

EntangleColourProfileTransform *
entangle_colour_profile_transform_new(EntangleColourProfile *src,
                                      EntangleColourProfile *dst,
                                      EntangleColourProfileIntent intent);

GdkPixbuf *
entangle_colour_profile_transform_apply(EntangleColourProfileTransform *trans,
                                        GdkPixbuf *srcpixbuf);

G_END_DECLS

#endif /* __ENTANGLE_COLOUR_PROFILE_H__ */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
