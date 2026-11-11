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

#ifndef __ENTANGLE_CAMERA_H__
#define __ENTANGLE_CAMERA_H__

#include <gio/gio.h>
#include <glib-object.h>

#include "entangle-camera-file.h"
#include "entangle-control-group.h"
#include "entangle-progress.h"

G_BEGIN_DECLS

#define ENTANGLE_TYPE_CAMERA (entangle_camera_get_type())
G_DECLARE_FINAL_TYPE(EntangleCamera, entangle_camera, ENTANGLE, CAMERA, GObject)

EntangleCamera *
entangle_camera_new(const char *model,
                    const char *port,
                    const char *serial,
                    gboolean hasCapture,
                    gboolean hasPreview,
                    gboolean hasSettings);

const char *
entangle_camera_get_model(EntangleCamera *cam);
const char *
entangle_camera_get_port(EntangleCamera *cam);

gboolean
entangle_camera_open(EntangleCamera *cam, GError **error);
void
entangle_camera_open_async(EntangleCamera *cam,
                           GCancellable *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer user_data);
gboolean
entangle_camera_open_finish(EntangleCamera *cam,
                            GAsyncResult *result,
                            GError **error);

gboolean
entangle_camera_close(EntangleCamera *cam, GError **error);
void
entangle_camera_close_async(EntangleCamera *cam,
                            GCancellable *cancellable,
                            GAsyncReadyCallback callback,
                            gpointer user_data);
gboolean
entangle_camera_close_finish(EntangleCamera *cam,
                             GAsyncResult *result,
                             GError **error);

gboolean
entangle_camera_get_opened(EntangleCamera *cam);

char *
entangle_camera_get_summary(EntangleCamera *cam);
char *
entangle_camera_get_manual(EntangleCamera *cam);
char *
entangle_camera_get_driver(EntangleCamera *cam);

EntangleCameraFile *
entangle_camera_capture_image(EntangleCamera *cam, GError **error);
void
entangle_camera_capture_image_async(EntangleCamera *cam,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data);
EntangleCameraFile *
entangle_camera_capture_image_finish(EntangleCamera *cam,
                                     GAsyncResult *result,
                                     GError **error);

EntangleCameraFile *
entangle_camera_preview_image(EntangleCamera *cam, GError **error);
void
entangle_camera_preview_image_async(EntangleCamera *cam,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data);
EntangleCameraFile *
entangle_camera_preview_image_finish(EntangleCamera *cam,
                                     GAsyncResult *result,
                                     GError **error);

gboolean
entangle_camera_download_file(EntangleCamera *cam,
                              EntangleCameraFile *file,
                              GError **error);
void
entangle_camera_download_file_async(EntangleCamera *cam,
                                    EntangleCameraFile *file,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data);
gboolean
entangle_camera_download_file_finish(EntangleCamera *cam,
                                     GAsyncResult *result,
                                     GError **error);

gboolean
entangle_camera_delete_file(EntangleCamera *cam,
                            EntangleCameraFile *file,
                            GError **error);
void
entangle_camera_delete_file_async(EntangleCamera *cam,
                                  EntangleCameraFile *file,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data);
gboolean
entangle_camera_delete_file_finish(EntangleCamera *cam,
                                   GAsyncResult *result,
                                   GError **error);

gboolean
entangle_camera_process_events(EntangleCamera *cam,
                               guint64 waitms,
                               GError **error);
void
entangle_camera_process_events_async(EntangleCamera *cam,
                                     guint64 waitms,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data);
gboolean
entangle_camera_process_events_finish(EntangleCamera *cam,
                                      GAsyncResult *result,
                                      GError **error);

gboolean
entangle_camera_set_viewfinder(EntangleCamera *cam,
                               gboolean enabled,
                               GError **error);
void
entangle_camera_set_viewfinder_async(EntangleCamera *cam,
                                     gboolean enabled,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data);
gboolean
entangle_camera_set_viewfinder_finish(EntangleCamera *cam,
                                      GAsyncResult *result,
                                      GError **error);

gboolean
entangle_camera_autofocus(EntangleCamera *cam, GError **error);
void
entangle_camera_autofocus_async(EntangleCamera *cam,
                                GCancellable *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer user_data);
gboolean
entangle_camera_autofocus_finish(EntangleCamera *cam,
                                 GAsyncResult *result,
                                 GError **error);

gboolean
entangle_camera_set_focus_area(EntangleCamera *cam,
                               gint x,
                               gint y,
                               GError **error);
void
entangle_camera_set_focus_area_async(EntangleCamera *cam,
                                     gint x,
                                     gint y,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data);
gboolean
entangle_camera_set_focus_area_finish(EntangleCamera *cam,
                                      GAsyncResult *result,
                                      GError **error);
void
entangle_camera_get_focus_area_range(EntangleCamera *cam,
                                     gint *minx,
                                     gint *maxx,
                                     gint *miny,
                                     gint *maxy);

typedef enum
{
    ENTANGLE_CAMERA_MANUAL_FOCUS_STEP_IN_COARSE,
    ENTANGLE_CAMERA_MANUAL_FOCUS_STEP_IN_MEDIUM,
    ENTANGLE_CAMERA_MANUAL_FOCUS_STEP_IN_FINE,
    ENTANGLE_CAMERA_MANUAL_FOCUS_STEP_OUT_COARSE,
    ENTANGLE_CAMERA_MANUAL_FOCUS_STEP_OUT_MEDIUM,
    ENTANGLE_CAMERA_MANUAL_FOCUS_STEP_OUT_FINE,
} EntangleCameraManualFocusStep;

gboolean
entangle_camera_manualfocus(EntangleCamera *cam,
                            EntangleCameraManualFocusStep step,
                            GError **error);
void
entangle_camera_manualfocus_async(EntangleCamera *cam,
                                  EntangleCameraManualFocusStep step,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data);
gboolean
entangle_camera_manualfocus_finish(EntangleCamera *cam,
                                   GAsyncResult *result,
                                   GError **error);

gboolean
entangle_camera_manualfocus_value(EntangleCamera *cam,
                                  gint value,
                                  GError **error);
void
entangle_camera_manualfocus_value_async(EntangleCamera *cam,
                                        gint value,
                                        GCancellable *cancellable,
                                        GAsyncReadyCallback callback,
                                        gpointer user_data);
gboolean
entangle_camera_manualfocus_value_finish(EntangleCamera *cam,
                                         GAsyncResult *result,
                                         GError **error);

gboolean
entangle_camera_set_clock(EntangleCamera *cam,
                          gint64 epochsecs,
                          GError **error);
void
entangle_camera_set_clock_async(EntangleCamera *cam,
                                gint64 epochsecs,
                                GCancellable *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer user_data);
gboolean
entangle_camera_set_clock_finish(EntangleCamera *cam,
                                 GAsyncResult *result,
                                 GError **error);

typedef enum
{
    ENTANGLE_CAMERA_CAPTURE_TARGET_RAM,
    ENTANGLE_CAMERA_CAPTURE_TARGET_CARD,
} EntangleCameraCaptureTarget;

gboolean
entangle_camera_set_capture_target(EntangleCamera *cam,
                                   EntangleCameraCaptureTarget target,
                                   GError **error);
void
entangle_camera_set_capture_target_async(EntangleCamera *cam,
                                         EntangleCameraCaptureTarget target,
                                         GCancellable *cancellable,
                                         GAsyncReadyCallback callback,
                                         gpointer user_data);
gboolean
entangle_camera_set_capture_target_finish(EntangleCamera *cam,
                                          GAsyncResult *result,
                                          GError **error);

gboolean
entangle_camera_get_has_capture(EntangleCamera *cam);
gboolean
entangle_camera_get_has_preview(EntangleCamera *cam);
gboolean
entangle_camera_get_has_settings(EntangleCamera *cam);
gboolean
entangle_camera_get_has_viewfinder(EntangleCamera *cam);
gboolean
entangle_camera_get_has_autofocus(EntangleCamera *cam);
gboolean
entangle_camera_get_has_focus_area(EntangleCamera *cam);
gboolean
entangle_camera_get_has_manual_focus(EntangleCamera *cam);
void
entangle_camera_get_manual_focus_range(EntangleCamera *cam,
                                       gint *min,
                                       gint *max);
gboolean
entangle_camera_get_manual_focus_mode(EntangleCamera *cam);
gboolean
entangle_camera_set_manual_focus_mode(EntangleCamera *cam,
                                      gboolean enabled,
                                      GError **error);
void
entangle_camera_set_manual_focus_mode_async(EntangleCamera *cam,
                                            gboolean enabled,
                                            GCancellable *cancellable,
                                            GAsyncReadyCallback callback,
                                            gpointer user_data);
gboolean
entangle_camera_set_manual_focus_mode_finish(EntangleCamera *cam,
                                             GAsyncResult *result,
                                             GError **error);

gboolean
entangle_camera_load_controls(EntangleCamera *cam, GError **error);
void
entangle_camera_load_controls_async(EntangleCamera *cam,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data);
gboolean
entangle_camera_load_controls_finish(EntangleCamera *cam,
                                     GAsyncResult *result,
                                     GError **error);

gboolean
entangle_camera_save_controls(EntangleCamera *cam, GError **error);
void
entangle_camera_save_controls_async(EntangleCamera *cam,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data);
gboolean
entangle_camera_save_controls_finish(EntangleCamera *cam,
                                     GAsyncResult *result,
                                     GError **error);

EntangleControlGroup *
entangle_camera_get_controls(EntangleCamera *cam, GError **error);

void
entangle_camera_set_progress(EntangleCamera *cam, EntangleProgress *prog);
EntangleProgress *
entangle_camera_get_progress(EntangleCamera *cam);

gboolean
entangle_camera_is_mounted(EntangleCamera *cam);

void
entangle_camera_mount_async(EntangleCamera *cam,
                            GCancellable *cancellable,
                            GAsyncReadyCallback callback,
                            gpointer user_data);
gboolean
entangle_camera_mount_finish(EntangleCamera *cam,
                             GAsyncResult *result,
                             GError **error);

void
entangle_camera_unmount_async(EntangleCamera *cam,
                              GCancellable *cancellable,
                              GAsyncReadyCallback callback,
                              gpointer user_data);
gboolean
entangle_camera_unmount_finish(EntangleCamera *cam,
                               GAsyncResult *result,
                               GError **error);

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
