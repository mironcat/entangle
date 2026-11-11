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

#include <string.h>

#include <gexiv2/gexiv2.h>

#include "entangle-camera-automata.h"

#include "entangle-debug.h"

/**
 * SECTION:entangle-camera-automata
 * @Short_description: Automation for the camera capture tasks
 * @Title: EntangleCameraAutomata
 *
 * The #EntangleCameraAutomata object provides a simpler way to drive
 * the capture operations for the camera, automating the steps for
 * downloading the images after capture.
 */

struct _EntangleCameraAutomata
{
    GObject parent;
    EntangleSession *session;
    EntangleCamera *camera;

    gboolean deleteFile;

    gint captureRotation;
    gboolean captureFlipH;
    gboolean captureFlipV;

    char *deleteImageDup;

    gulong sigFileAdd;
    gulong sigFileDownload;
};

G_DEFINE_TYPE(EntangleCameraAutomata, entangle_camera_automata, G_TYPE_OBJECT);

enum
{
    PROP_0,
    PROP_SESSION,
    PROP_CAMERA,
    PROP_DELETE_FILE,
};

static void
entangle_camera_automata_get_property(GObject *object,
                                      guint prop_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
    EntangleCameraAutomata *automata = ENTANGLE_CAMERA_AUTOMATA(object);

    switch (prop_id) {
    case PROP_SESSION:
        g_value_set_object(value, automata->session);
        break;

    case PROP_CAMERA:
        g_value_set_object(value, automata->camera);
        break;

    case PROP_DELETE_FILE:
        g_value_set_boolean(value, automata->deleteFile);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
entangle_camera_automata_set_property(GObject *object,
                                      guint prop_id,
                                      const GValue *value,
                                      GParamSpec *pspec)
{
    EntangleCameraAutomata *automata = ENTANGLE_CAMERA_AUTOMATA(object);

    switch (prop_id) {
    case PROP_SESSION:
        entangle_camera_automata_set_session(automata,
                                             g_value_get_object(value));
        break;

    case PROP_CAMERA:
        entangle_camera_automata_set_camera(automata,
                                            g_value_get_object(value));
        break;

    case PROP_DELETE_FILE:
        automata->deleteFile = g_value_get_boolean(value);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
entangle_camera_automata_finalize(GObject *object)
{
    EntangleCameraAutomata *automata = ENTANGLE_CAMERA_AUTOMATA(object);

    ENTANGLE_DEBUG("Finalize camera automata %p", object);

    if (automata->camera)
        g_object_unref(automata->camera);
    if (automata->session)
        g_object_unref(automata->session);

    g_free(automata->deleteImageDup);

    G_OBJECT_CLASS(entangle_camera_automata_parent_class)->finalize(object);
}

static void
entangle_camera_automata_class_init(EntangleCameraAutomataClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = entangle_camera_automata_finalize;
    object_class->get_property = entangle_camera_automata_get_property;
    object_class->set_property = entangle_camera_automata_set_property;

    g_object_class_install_property(
        object_class, PROP_SESSION,
        g_param_spec_object("session", "Session", "Session",
                            ENTANGLE_TYPE_SESSION,
                            G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
    g_object_class_install_property(
        object_class, PROP_CAMERA,
        g_param_spec_object("camera", "Camera", "Camera", ENTANGLE_TYPE_CAMERA,
                            G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
    g_object_class_install_property(
        object_class, PROP_DELETE_FILE,
        g_param_spec_boolean("delete-file", "Delete file", "Delete file", TRUE,
                             G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                 G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    g_signal_new("camera-capture-begin", G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
                 g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
    g_signal_new("camera-capture-end", G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
                 g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

EntangleCameraAutomata *
entangle_camera_automata_new(void)
{
    return ENTANGLE_CAMERA_AUTOMATA(
        g_object_new(ENTANGLE_TYPE_CAMERA_AUTOMATA, NULL));
}

static void
entangle_camera_automata_init(EntangleCameraAutomata *automata G_GNUC_UNUSED)
{
}

typedef struct
{
    EntangleCameraAutomata *automata;
    GTask *task;
    GCancellable *cancel;
    GCancellable *confirm;
    EntangleCameraFile *file;
} EntangleCameraAutomataData;

static EntangleCameraAutomataData *
entangle_camera_automata_data_new(EntangleCameraAutomata *automata,
                                  GCancellable *cancel,
                                  GCancellable *confirm,
                                  GTask *task)
{
    EntangleCameraAutomataData *data = g_new0(EntangleCameraAutomataData, 1);
    data->automata = g_object_ref(automata);
    data->task = g_object_ref(task);
    if (cancel)
        data->cancel = g_object_ref(cancel);
    if (confirm)
        data->confirm = g_object_ref(confirm);
    return data;
}

static void
entangle_camera_automata_data_free(EntangleCameraAutomataData *data)
{
    g_object_unref(data->automata);
    if (data->file)
        g_object_unref(data->file);
    g_object_unref(data->task);
    if (data->cancel)
        g_object_unref(data->cancel);
    g_free(data);
}

static gint
entangle_camera_exif_orientation_from_transform(gint rotation,
                                                gboolean flipH,
                                                gboolean flipV)
{
    rotation = ((rotation % 360) + 360) % 360;

    /* The view transform is applied to the preview as a flip followed by
     * a clockwise rotation. The EXIF orientation has to describe the very
     * same transform, so that an external viewer shows the captured file
     * exactly like the preview did.
     *
     * EXIF orientation values:
     *   1 normal          2 mirror horizontal   3 rotate 180
     *   4 mirror vertical 5 mirror + rotate 90  6 rotate 90
     *   7 mirror + rotate 270                   8 rotate 270
     */
    switch (rotation) {
    case 90:
        if (flipH && flipV)
            return 8;
        if (flipH)
            return 7;
        if (flipV)
            return 5;
        return 6;
    case 180:
        if (flipH && flipV)
            return 1;
        if (flipH)
            return 4;
        if (flipV)
            return 2;
        return 3;
    case 270:
        if (flipH && flipV)
            return 6;
        if (flipH)
            return 5;
        if (flipV)
            return 7;
        return 8;
    default:
        if (flipH && flipV)
            return 3;
        if (flipH)
            return 2;
        if (flipV)
            return 4;
        return 1;
    }
}

static void
do_apply_capture_orientation(const gchar *path,
                             gint rotation,
                             gboolean flipH,
                             gboolean flipV)
{
    GExiv2Metadata *metadata;
    GError *error = NULL;
    gchar *value;
    gint orientation;
    gboolean image_ok;
    gboolean thumb_ok;

    if (!path)
        return;

    /* The live preview is a raw pixbuf without the camera's EXIF
     * orientation. Write exactly the preview transform so the saved file
     * matches the preview, even if the camera reported an orientation. */
    orientation = entangle_camera_exif_orientation_from_transform(
        rotation, flipH, flipV);

    metadata = gexiv2_metadata_new();
    if (!gexiv2_metadata_open_path(metadata, path, &error)) {
        ENTANGLE_DEBUG("Cannot open EXIF metadata for %s: %s", path,
                       error ? error->message : "unknown error");
        g_clear_error(&error);
        g_object_unref(metadata);
        return;
    }

    value = g_strdup_printf("%d", orientation);
    image_ok = gexiv2_metadata_set_tag_string(
        metadata, "Exif.Image.Orientation", value);
    /* Keep the embedded thumbnail/preview aligned with the main image,
     * otherwise Entangle and external viewers can disagree. */
    thumb_ok = gexiv2_metadata_set_tag_string(
        metadata, "Exif.Thumbnail.Orientation", value);

    if (!image_ok) {
        ENTANGLE_DEBUG("Failed to set EXIF orientation for %s", path);
    } else if (!gexiv2_metadata_save_file(metadata, path, &error)) {
        ENTANGLE_DEBUG("Failed to save EXIF orientation for %s: %s", path,
                       error ? error->message : "unknown error");
    } else {
        ENTANGLE_DEBUG("Applied EXIF orientation %d to %s (thumbnail %s)",
                       orientation, path, thumb_ok ? "ok" : "unavailable");
    }

    g_clear_error(&error);
    g_free(value);
    g_object_unref(metadata);
}

static void
do_entangle_camera_file_download(EntangleCamera *cam G_GNUC_UNUSED,
                                 EntangleCameraFile *file,
                                 void *opaque)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_AUTOMATA(opaque));

    EntangleCameraAutomata *automata = opaque;
    EntangleImage *image;
    gchar *localpath;

    ENTANGLE_DEBUG("File download %p %p %p", cam, file, automata);

    localpath = entangle_session_next_filename(automata->session, file);

    if (!localpath || !entangle_camera_file_save_path(file, localpath, NULL)) {
        ENTANGLE_DEBUG("Failed save path");
        goto cleanup;
    }
    ENTANGLE_DEBUG("Saved to %s", localpath);
    do_apply_capture_orientation(localpath, automata->captureRotation,
                                 automata->captureFlipH,
                                 automata->captureFlipV);
    image = entangle_image_new_file(localpath);

    entangle_session_add_media(automata->session, ENTANGLE_MEDIA(image));

    g_object_unref(image);

cleanup:
    g_free(localpath);
}

static void
do_entangle_camera_delete_finish(GObject *src,
                                 GAsyncResult *res,
                                 gpointer opaque)
{
    EntangleCameraAutomataData *data = opaque;
    EntangleCamera *camera = ENTANGLE_CAMERA(src);
    GError *error = NULL;

    if (!entangle_camera_delete_file_finish(camera, res, &error)) {
        g_task_return_error(data->task, error);
        entangle_camera_automata_data_free(data);
        return;
    }

    g_task_return_boolean(data->task, TRUE);
    entangle_camera_automata_data_free(data);
}

static void
do_entangle_camera_download_finish(GObject *src,
                                   GAsyncResult *res,
                                   gpointer opaque)
{
    EntangleCameraAutomataData *data = opaque;
    EntangleCameraAutomata *automata = data->automata;
    EntangleCamera *camera = ENTANGLE_CAMERA(src);
    GError *error = NULL;

    if (!entangle_camera_download_file_finish(camera, res, &error)) {
        g_task_return_error(data->task, error);
        /* Fallthrough to delete anyway */
    }

    if (automata->deleteFile) {
        entangle_camera_delete_file_async(
            camera, data->file, NULL, do_entangle_camera_delete_finish, data);
    } else {
        if (!error) {
            g_task_return_boolean(data->task, TRUE);
            entangle_camera_automata_data_free(data);
        } else {
            entangle_camera_automata_data_free(data);
        }
    }
}

static void
do_entangle_camera_file_add_finish(GObject *src G_GNUC_UNUSED,
                                   GAsyncResult *res G_GNUC_UNUSED,
                                   gpointer opaque G_GNUC_UNUSED)
{
    /* XXX report error ? */
}

static void
do_entangle_camera_file_add(EntangleCamera *camera,
                            EntangleCameraFile *file,
                            void *opaque)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA(camera));
    g_return_if_fail(ENTANGLE_IS_CAMERA_AUTOMATA(opaque));
    g_return_if_fail(ENTANGLE_IS_CAMERA_FILE(file));

    EntangleCameraAutomata *automata = ENTANGLE_CAMERA_AUTOMATA(opaque);
    EntangleCameraAutomataData *data;
    GTask *task;

    task = g_task_new(automata, NULL, do_entangle_camera_file_add_finish, NULL);
    data = entangle_camera_automata_data_new(automata, NULL, NULL, task);
    g_object_unref(task);
    ENTANGLE_DEBUG("File add %p %p %p", camera, file, data);

    data->file = g_object_ref(file);

    if (automata->deleteImageDup) {
        gsize len = strlen(automata->deleteImageDup);
        if (strncmp(entangle_camera_file_get_name(file),
                    automata->deleteImageDup, len) == 0) {
            g_free(automata->deleteImageDup);
            automata->deleteImageDup = NULL;
            entangle_camera_delete_file_async(camera, data->file, NULL,
                                              do_entangle_camera_delete_finish,
                                              data);
            return;
        }
    }

    entangle_camera_download_file_async(
        camera, data->file, NULL, do_entangle_camera_download_finish, data);
}

static void
do_entangle_camera_flush_events_finish(GObject *src,
                                       GAsyncResult *res,
                                       gpointer opaque)
{
    EntangleCameraAutomataData *data = opaque;
    EntangleCameraAutomata *automata = data->automata;
    EntangleCamera *camera = ENTANGLE_CAMERA(src);
    GError *error = NULL;

    if (!entangle_camera_process_events_finish(camera, res, &error)) {
        g_task_return_error(data->task, error);
        entangle_camera_automata_data_free(data);
        return;
    }

    if (g_cancellable_is_cancelled(data->cancel)) {
        if (automata->deleteFile) {
            entangle_camera_delete_file_async(camera, data->file, NULL,
                                              do_entangle_camera_delete_finish,
                                              data);
        } else {
            g_task_return_boolean(data->task, TRUE);
            entangle_camera_automata_data_free(data);
        }
    } else {
        entangle_camera_download_file_async(
            camera, data->file, NULL, do_entangle_camera_download_finish, data);
    }
}

static void
do_entangle_camera_stop_preview_finish(GObject *src,
                                       GAsyncResult *res,
                                       gpointer opaque)
{
    EntangleCameraAutomataData *data = opaque;
    EntangleCameraAutomata *automata = data->automata;
    EntangleCamera *camera = ENTANGLE_CAMERA(src);
    GError *error = NULL;

    g_signal_emit_by_name(data->automata, "camera-capture-end");

    if (!entangle_camera_set_viewfinder_finish(camera, res, &error)) {
        g_task_return_error(data->task, error);
        entangle_camera_automata_data_free(data);
        return;
    }

    if (g_cancellable_is_cancelled(data->cancel)) {
        if (automata->deleteFile) {
            entangle_camera_delete_file_async(camera, data->file, NULL,
                                              do_entangle_camera_delete_finish,
                                              data);
        } else {
            g_task_return_boolean(data->task, TRUE);
            entangle_camera_automata_data_free(data);
        }
    } else {
        entangle_camera_process_events_async(
            camera, 100, NULL, do_entangle_camera_flush_events_finish, data);
    }
}

static void
do_entangle_camera_capture_finish(GObject *src,
                                  GAsyncResult *res,
                                  gpointer opaque)
{
    EntangleCameraAutomataData *data = opaque;
    EntangleCameraAutomata *automata = data->automata;
    EntangleCamera *camera = ENTANGLE_CAMERA(src);
    GError *error = NULL;

    g_signal_emit_by_name(data->automata, "camera-capture-end");

    if (!(data->file =
              entangle_camera_capture_image_finish(camera, res, &error))) {
        g_task_return_error(data->task, error);
        entangle_camera_automata_data_free(data);
        return;
    }

    if (entangle_camera_get_has_viewfinder(automata->camera)) {
        entangle_camera_set_viewfinder_async(
            automata->camera, FALSE, NULL,
            do_entangle_camera_stop_preview_finish, data);
    } else if (g_cancellable_is_cancelled(data->cancel)) {
        if (automata->deleteFile) {
            entangle_camera_delete_file_async(camera, data->file, NULL,
                                              do_entangle_camera_delete_finish,
                                              data);
        } else {
            g_task_return_boolean(data->task, TRUE);
            entangle_camera_automata_data_free(data);
        }
    } else {
        entangle_camera_process_events_async(
            camera, 100, NULL, do_entangle_camera_flush_events_finish, data);
    }
}

void
entangle_camera_automata_capture_async(EntangleCameraAutomata *automata,
                                       GCancellable *cancel,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_AUTOMATA(automata));

    EntangleCameraAutomataData *data;
    GTask *task;

    task = g_task_new(automata, NULL, callback, user_data);
    data = entangle_camera_automata_data_new(automata, cancel, NULL, task);

    g_signal_emit_by_name(automata, "camera-capture-begin");
    entangle_camera_capture_image_async(
        automata->camera, cancel, do_entangle_camera_capture_finish, data);

    g_object_unref(task);
}

gboolean
entangle_camera_automata_capture_finish(EntangleCameraAutomata *automata,
                                        GAsyncResult *res,
                                        GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA_AUTOMATA(automata), FALSE);

    return g_task_propagate_boolean(G_TASK(res), error);
}

static void
do_entangle_camera_discard_finish(GObject *src,
                                  GAsyncResult *res,
                                  gpointer opaque)
{
    EntangleCameraAutomataData *data = opaque;
    EntangleCameraAutomata *automata = data->automata;
    EntangleCamera *camera = ENTANGLE_CAMERA(src);
    GError *error = NULL;
    char *filename;
    char *tmp;

    if (!(data->file =
              entangle_camera_capture_image_finish(camera, res, &error))) {
        g_task_return_error(data->task, error);
        entangle_camera_automata_data_free(data);
        return;
    }

    filename = g_strdup(entangle_camera_file_get_name(data->file));
    if ((tmp = strrchr(filename, '.')))
        *tmp = '\0';
    automata->deleteImageDup = filename;

    entangle_camera_delete_file_async(camera, data->file, NULL,
                                      do_entangle_camera_delete_finish, data);
}

static void
do_entangle_camera_viewfinder_finish(GObject *src,
                                     GAsyncResult *res,
                                     gpointer opaque)
{
    EntangleCameraAutomataData *data = opaque;
    EntangleCamera *camera = ENTANGLE_CAMERA(src);
    GError *error = NULL;

    if (!entangle_camera_set_viewfinder_finish(camera, res, &error)) {
        g_task_return_error(data->task, error);
    } else {
        g_task_return_boolean(data->task, TRUE);
    }
    entangle_camera_automata_data_free(data);
}

static void
do_entangle_camera_preview_finish(GObject *src,
                                  GAsyncResult *res,
                                  gpointer opaque)
{
    EntangleCameraAutomataData *data = opaque;
    EntangleCameraAutomata *automata = data->automata;
    EntangleCamera *camera = ENTANGLE_CAMERA(src);
    EntangleCameraFile *file;
    GError *error = NULL;

    if (!(file = entangle_camera_preview_image_finish(camera, res, &error))) {
        if (g_cancellable_is_cancelled(data->cancel) && automata->camera) {
            if (entangle_camera_get_has_viewfinder(automata->camera))
                entangle_camera_set_viewfinder_async(
                    automata->camera, FALSE, NULL,
                    do_entangle_camera_viewfinder_finish, data);
            else
                entangle_camera_capture_image_async(
                    automata->camera, NULL, do_entangle_camera_discard_finish,
                    data);
            g_error_free(error);
        } else {
            g_task_return_error(data->task, error);
            entangle_camera_automata_data_free(data);
        }
        return;
    }

    g_object_unref(file);

    if (g_cancellable_is_cancelled(data->cancel)) {
        if (entangle_camera_get_has_viewfinder(automata->camera))
            entangle_camera_set_viewfinder_async(
                automata->camera, FALSE, NULL,
                do_entangle_camera_viewfinder_finish, data);
        else
            entangle_camera_capture_image_async(
                automata->camera, NULL, do_entangle_camera_discard_finish,
                data);
    } else if (g_cancellable_is_cancelled(data->confirm)) {
        g_signal_emit_by_name(data->automata, "camera-capture-begin");
        g_cancellable_reset(data->confirm);
        entangle_camera_capture_image_async(automata->camera, data->cancel,
                                            do_entangle_camera_capture_finish,
                                            data);
    } else {
        entangle_camera_preview_image_async(automata->camera, data->cancel,
                                            do_entangle_camera_preview_finish,
                                            data);
    }
}

void
entangle_camera_automata_preview_async(EntangleCameraAutomata *automata,
                                       GCancellable *cancel,
                                       GCancellable *confirm,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_AUTOMATA(automata));

    EntangleCameraAutomataData *data;
    GTask *task;

    task = g_task_new(automata, NULL, callback, user_data);
    data = entangle_camera_automata_data_new(automata, cancel, confirm, task);

    entangle_camera_preview_image_async(
        automata->camera, cancel, do_entangle_camera_preview_finish, data);

    g_object_unref(task);
}

gboolean
entangle_camera_automata_preview_finish(EntangleCameraAutomata *automata,
                                        GAsyncResult *res,
                                        GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA_AUTOMATA(automata), FALSE);

    return g_task_propagate_boolean(G_TASK(res), error);
}

void
entangle_camera_automata_set_camera(EntangleCameraAutomata *automata,
                                    EntangleCamera *camera)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_AUTOMATA(automata));
    g_return_if_fail(!camera || ENTANGLE_IS_CAMERA(camera));

    if (automata->camera) {
        g_signal_handler_disconnect(automata->camera,
                                    automata->sigFileDownload);
        g_signal_handler_disconnect(automata->camera, automata->sigFileAdd);

        g_object_unref(automata->camera);
        automata->camera = NULL;
    }

    if (camera) {
        automata->camera = g_object_ref(camera);

        automata->sigFileDownload = g_signal_connect(
            automata->camera, "camera-file-downloaded",
            G_CALLBACK(do_entangle_camera_file_download), automata);
        automata->sigFileAdd =
            g_signal_connect(automata->camera, "camera-file-added",
                             G_CALLBACK(do_entangle_camera_file_add), automata);
    }
}

/**
 * entangle_camera_automata_get_camera:
 * @automata: (transfer none): the automata object
 *
 * Get the camera associated with the automata
 *
 * Returns: (transfer none): the camera or NULL
 */
EntangleCamera *
entangle_camera_automata_get_camera(EntangleCameraAutomata *automata)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA_AUTOMATA(automata), NULL);

    return automata->camera;
}

void
entangle_camera_automata_set_session(EntangleCameraAutomata *automata,
                                     EntangleSession *session)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_AUTOMATA(automata));
    g_return_if_fail(!session || ENTANGLE_IS_SESSION(session));

    if (automata->session) {
        g_object_unref(automata->session);
        automata->session = NULL;
    }

    if (session) {
        automata->session = g_object_ref(session);
    }
}

/**
 * entangle_camera_automata_get_session:
 * @automata: (transfer none): the automata object
 *
 * Get the session associated with the automata
 *
 * Returns: (transfer none): the session or NULL
 */
EntangleSession *
entangle_camera_automata_get_session(EntangleCameraAutomata *automata)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA_AUTOMATA(automata), NULL);

    return automata->session;
}

void
entangle_camera_automata_set_delete_file(EntangleCameraAutomata *automata,
                                         gboolean value)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_AUTOMATA(automata));

    automata->deleteFile = value;
}

gboolean
entangle_camera_automata_get_delete_file(EntangleCameraAutomata *automata)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA_AUTOMATA(automata), TRUE);

    return automata->deleteFile;
}

void
entangle_camera_automata_set_capture_rotation(EntangleCameraAutomata *automata,
                                              gint rotation)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_AUTOMATA(automata));

    rotation = ((rotation % 360) + 360) % 360;
    if (rotation % 90 != 0)
        rotation = 0;

    automata->captureRotation = rotation;
}

void
entangle_camera_automata_set_capture_transform(EntangleCameraAutomata *automata,
                                               gint rotation,
                                               gboolean flipH,
                                               gboolean flipV)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_AUTOMATA(automata));

    entangle_camera_automata_set_capture_rotation(automata, rotation);
    automata->captureFlipH = flipH;
    automata->captureFlipV = flipV;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
