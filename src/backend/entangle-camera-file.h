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

#ifndef __ENTANGLE_CAMERA_FILE_H__
#define __ENTANGLE_CAMERA_FILE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define ENTANGLE_TYPE_CAMERA_FILE            (entangle_camera_file_get_type ())
G_DECLARE_FINAL_TYPE(EntangleCameraFile, entangle_camera_file, ENTANGLE, CAMERA_FILE, GObject)

EntangleCameraFile *entangle_camera_file_new(const char *folder,
                                             const char *name);

const char *entangle_camera_file_get_folder(EntangleCameraFile *file);
const char *entangle_camera_file_get_name(EntangleCameraFile *file);

gboolean entangle_camera_file_save_path(EntangleCameraFile *file,
                                        const char *localpath,
                                        GError **err);
gboolean entangle_camera_file_save_uri(EntangleCameraFile *file,
                                       const char *uri,
                                       GError **err);

GByteArray *entangle_camera_file_get_data(EntangleCameraFile *file);
void entangle_camera_file_set_data(EntangleCameraFile *file, GByteArray *data);

const gchar *entangle_camera_file_get_mimetype(EntangleCameraFile *file);
void entangle_camera_file_set_mimetype(EntangleCameraFile *file, const gchar *mimetype);

G_END_DECLS

#endif /* __ENTANGLE_CAMERA_H__ */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
