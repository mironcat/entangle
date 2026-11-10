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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gphoto2.h>

#include "entangle-camera.h"

#include "entangle-camera-enums.h"
#include "entangle-control-button.h"
#include "entangle-control-choice.h"
#include "entangle-control-date.h"
#include "entangle-control-group.h"
#include "entangle-control-range.h"
#include "entangle-control-text.h"
#include "entangle-control-toggle.h"
#include "entangle-debug.h"

/**
 * SECTION:entangle-camera
 * @Short_description: a digital SLR camera device
 * @Title: EntangleCamera
 *
 * The #EntangleCamera object provides a way to control a digital
 * SLR camera device.
 */

struct _EntangleCamera
{
    GObject parent;
    GMutex lock;
    GCond jobCond;
    gboolean jobActive;

    GPContext *ctx;
    CameraAbilitiesList *caps;
    GPPortInfoList *ports;
    Camera *cam;

    CameraWidget *widgets;
    EntangleControlGroup *controls;
    GHashTable *controlPaths;

    EntangleProgress *progress;

    char *lastError;

    char *model;  /* R/O */
    char *port;   /* R/O */
    char *serial; /* R/O */

    char *manual;
    char *summary;
    char *driver;

    gboolean hasCapture;
    gboolean hasPreview;
    gboolean hasSettings;
    gboolean hasViewfinder;
    gboolean hasAutofocus;
    gboolean hasFocusArea;
    gboolean hasManualFocus;

    GList *supportedFNumbers;
    gboolean fnumberProbed;
};

G_DEFINE_TYPE(EntangleCamera, entangle_camera, G_TYPE_OBJECT);

enum
{
    PROP_0,
    PROP_MODEL,
    PROP_PORT,
    PROP_SERIAL,
    PROP_MANUAL,
    PROP_SUMMARY,
    PROP_DRIVER,
    PROP_PROGRESS,
    PROP_HAS_CAPTURE,
    PROP_HAS_PREVIEW,
    PROP_HAS_SETTINGS,
    PROP_HAS_VIEWFINDER,
};

#define ENTANGLE_CAMERA_ERROR entangle_camera_error_quark()

static GQuark
entangle_camera_error_quark(void)
{
    return g_quark_from_static_string("entangle-camera-error");
}

static EntangleControl *
do_build_controls(EntangleCamera *cam,
                  const char *path,
                  CameraWidget *widget,
                  GError **error);
static gboolean
do_load_controls(EntangleCamera *cam,
                 const char *path,
                 CameraWidget *widget,
                 GError **error);

struct EntangleCameraEventData
{
    EntangleCamera *cam;
    GObject *arg;
    char *signame;
};

static gboolean
entangle_camera_emit_idle(gpointer opaque)
{
    struct EntangleCameraEventData *data = opaque;

    g_signal_emit_by_name(data->cam, data->signame, data->arg);

    g_free(data->signame);
    g_object_unref(data->cam);
    if (data->arg)
        g_object_unref(data->arg);
    g_free(data);
    return FALSE;
}

static void
entangle_camera_emit_deferred(EntangleCamera *cam,
                              const char *signame,
                              GObject *arg)
{
    struct EntangleCameraEventData *data =
        g_new0(struct EntangleCameraEventData, 1);
    data->cam = cam;
    data->arg = arg;
    data->signame = g_strdup(signame);
    g_object_ref(cam);
    if (arg)
        g_object_ref(arg);

    g_idle_add(entangle_camera_emit_idle, data);
}

static void
entangle_camera_begin_job(EntangleCamera *cam)
{
    g_object_ref(cam);

    while (cam->jobActive) {
        g_cond_wait(&cam->jobCond, &cam->lock);
    }

    cam->jobActive = TRUE;
    g_free(cam->lastError);
    cam->lastError = NULL;
    g_mutex_unlock(&cam->lock);
}

static void
entangle_camera_end_job(EntangleCamera *cam)
{
    cam->jobActive = FALSE;
    g_cond_broadcast(&cam->jobCond);
    g_mutex_lock(&cam->lock);
    g_object_unref(cam);
}

static void
entangle_camera_get_property(GObject *object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
    EntangleCamera *cam = ENTANGLE_CAMERA(object);

    switch (prop_id) {
    case PROP_MODEL:
        g_value_set_string(value, cam->model);
        break;

    case PROP_PORT:
        g_value_set_string(value, cam->port);
        break;

    case PROP_SERIAL:
        g_value_set_string(value, cam->serial);
        break;

    case PROP_MANUAL:
        g_value_set_string(value, cam->manual);
        break;

    case PROP_SUMMARY:
        g_value_set_string(value, cam->summary);
        break;

    case PROP_DRIVER:
        g_value_set_string(value, cam->driver);
        break;

    case PROP_PROGRESS:
        g_value_set_object(value, cam->progress);
        break;

    case PROP_HAS_CAPTURE:
        g_value_set_boolean(value, cam->hasCapture);
        break;

    case PROP_HAS_PREVIEW:
        g_value_set_boolean(value, cam->hasPreview);
        break;

    case PROP_HAS_SETTINGS:
        g_value_set_boolean(value, cam->hasSettings);
        break;

    case PROP_HAS_VIEWFINDER:
        g_value_set_boolean(value, cam->hasViewfinder);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
entangle_camera_set_property(GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    EntangleCamera *cam = ENTANGLE_CAMERA(object);

    switch (prop_id) {
    case PROP_MODEL:
        g_free(cam->model);
        cam->model = g_value_dup_string(value);
        break;

    case PROP_PORT:
        g_free(cam->port);
        cam->port = g_value_dup_string(value);
        break;

    case PROP_SERIAL:
        g_free(cam->serial);
        cam->serial = g_value_dup_string(value);
        break;

    case PROP_PROGRESS:
        entangle_camera_set_progress(cam, g_value_get_object(value));
        break;

    case PROP_HAS_CAPTURE:
        cam->hasCapture = g_value_get_boolean(value);
        ENTANGLE_DEBUG("Set has capture %d", cam->hasCapture);
        break;

    case PROP_HAS_PREVIEW:
        cam->hasPreview = g_value_get_boolean(value);
        ENTANGLE_DEBUG("Set has preview %d", cam->hasPreview);
        break;

    case PROP_HAS_SETTINGS:
        cam->hasSettings = g_value_get_boolean(value);
        ENTANGLE_DEBUG("Set has settings %d", cam->hasSettings);
        break;

    case PROP_HAS_VIEWFINDER:
        cam->hasViewfinder = g_value_get_boolean(value);
        ENTANGLE_DEBUG("Set has viewfinder %d", cam->hasViewfinder);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
entangle_camera_finalize(GObject *object)
{
    EntangleCamera *cam = ENTANGLE_CAMERA(object);

    ENTANGLE_DEBUG("Finalize camera %p", object);

    if (cam->progress)
        g_object_unref(cam->progress);
    if (cam->cam) {
        gp_camera_exit(cam->cam, cam->ctx);
        gp_camera_free(cam->cam);
    }
    if (cam->widgets)
        gp_widget_unref(cam->widgets);
    if (cam->controls)
        g_object_unref(cam->controls);
    if (cam->controlPaths)
        g_hash_table_unref(cam->controlPaths);
    if (cam->supportedFNumbers)
        g_list_free_full(cam->supportedFNumbers, g_free);
    if (cam->ports)
        gp_port_info_list_free(cam->ports);
    if (cam->caps)
        gp_abilities_list_free(cam->caps);
    gp_context_unref(cam->ctx);
    g_free(cam->driver);
    g_free(cam->summary);
    g_free(cam->manual);
    g_free(cam->serial);
    g_free(cam->model);
    g_free(cam->port);
    g_free(cam->lastError);
    g_mutex_clear(&cam->lock);
    g_cond_clear(&cam->jobCond);

    G_OBJECT_CLASS(entangle_camera_parent_class)->finalize(object);
}

static void
entangle_camera_class_init(EntangleCameraClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = entangle_camera_finalize;
    object_class->get_property = entangle_camera_get_property;
    object_class->set_property = entangle_camera_set_property;

    g_signal_new("camera-file-added", G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
                 g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1,
                 ENTANGLE_TYPE_CAMERA_FILE);

    g_signal_new("camera-file-captured", G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
                 g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1,
                 ENTANGLE_TYPE_CAMERA_FILE);

    g_signal_new("camera-file-previewed", G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
                 g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1,
                 ENTANGLE_TYPE_CAMERA_FILE);

    g_signal_new("camera-file-downloaded", G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
                 g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1,
                 ENTANGLE_TYPE_CAMERA_FILE);

    g_signal_new("camera-file-deleted", G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
                 g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1,
                 ENTANGLE_TYPE_CAMERA_FILE);

    g_signal_new("camera-opened", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST,
                 0, NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

    g_signal_new("camera-closed", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST,
                 0, NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

    g_signal_new("camera-controls-changed", G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
                 g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

    g_object_class_install_property(
        object_class, PROP_MODEL,
        g_param_spec_string(
            "model", "Camera model", "Model name of the camera", NULL,
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_NAME |
                G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
    g_object_class_install_property(
        object_class, PROP_PORT,
        g_param_spec_string(
            "port", "Camera port", "Device port of the camera", NULL,
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_NAME |
                G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    g_object_class_install_property(
        object_class, PROP_SERIAL,
        g_param_spec_string(
            "serial", "Camera serial", "Device serial of the camera", NULL,
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_NAME |
                G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    g_object_class_install_property(
        object_class, PROP_SUMMARY,
        g_param_spec_string("summary", "Camera summary", "Camera summary", NULL,
                            G_PARAM_READABLE | G_PARAM_STATIC_NAME |
                                G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    g_object_class_install_property(
        object_class, PROP_MANUAL,
        g_param_spec_string("manual", "Camera manual", "Camera manual", NULL,
                            G_PARAM_READABLE | G_PARAM_STATIC_NAME |
                                G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    g_object_class_install_property(
        object_class, PROP_DRIVER,
        g_param_spec_string("driver", "Camera driver info",
                            "Camera driver information", NULL,
                            G_PARAM_READABLE | G_PARAM_STATIC_NAME |
                                G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    g_object_class_install_property(
        object_class, PROP_PROGRESS,
        g_param_spec_object("progress", "Progress updater",
                            "Operation progress updater",
                            ENTANGLE_TYPE_PROGRESS,
                            G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    g_object_class_install_property(
        object_class, PROP_HAS_CAPTURE,
        g_param_spec_boolean("has-capture", "Capture supported",
                             "Whether image capture is supported", FALSE,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                 G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                 G_PARAM_STATIC_BLURB));
    g_object_class_install_property(
        object_class, PROP_HAS_PREVIEW,
        g_param_spec_boolean("has-preview", "Preview supported",
                             "Whether image preview is supported", FALSE,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                 G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
                                 G_PARAM_STATIC_BLURB));
    g_object_class_install_property(
        object_class, PROP_HAS_SETTINGS,
        g_param_spec_boolean(
            "has-settings", "Settings supported",
            "Whether camera settings configuration is supported", FALSE,
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_NAME |
                G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
    g_object_class_install_property(
        object_class, PROP_HAS_VIEWFINDER,
        g_param_spec_boolean(
            "has-viewfinder", "Viewfinder supported",
            "Whether camera viewfinder configuration is supported", FALSE,
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_NAME |
                G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
    ENTANGLE_DEBUG("install prog done");
}

EntangleCamera *
entangle_camera_new(const char *model,
                    const char *port,
                    const char *serial,
                    gboolean hasCapture,
                    gboolean hasPreview,
                    gboolean hasSettings)
{
    return ENTANGLE_CAMERA(
        g_object_new(ENTANGLE_TYPE_CAMERA, "model", model, "port", port,
                     "serial", serial, "has-capture", hasCapture, "has-preview",
                     hasPreview, "has-settings", hasSettings, NULL));
}

static void
entangle_camera_init(EntangleCamera *cam)
{
    g_mutex_init(&cam->lock);
    g_cond_init(&cam->jobCond);
}

/**
 * entangle_camera_get_model:
 * @cam: (transfer none): the camera
 *
 * Get the camera model name
 *
 * Returns: (transfer none): the model name
 */
const char *
entangle_camera_get_model(EntangleCamera *cam)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), NULL);

    return cam->model;
}

/**
 * entangle_camera_get_port:
 * @cam: (transfer none): the camera
 *
 * Get the camera port name
 *
 * Returns: (transfer none): the port name
 */
const char *
entangle_camera_get_port(EntangleCamera *cam)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), NULL);

    return cam->port;
}

struct EntangleCameraProgressData
{
    EntangleCamera *cam;
    enum
    {
        ENTANGLE_CAMERA_PROGRESS_START,
        ENTANGLE_CAMERA_PROGRESS_UPDATE,
        ENTANGLE_CAMERA_PROGRESS_STOP
    } op;
    float value;
    char *msg;
};

static gboolean
entangle_camera_progress_idle(gpointer opaque)
{
    struct EntangleCameraProgressData *data = opaque;
    EntangleCamera *cam = data->cam;

    if (cam->progress) {
        switch (data->op) {
        case ENTANGLE_CAMERA_PROGRESS_START:
            entangle_progress_start(cam->progress, data->value, data->msg);
            break;

        case ENTANGLE_CAMERA_PROGRESS_UPDATE:
            entangle_progress_update(cam->progress, data->value);
            break;

        case ENTANGLE_CAMERA_PROGRESS_STOP:
            entangle_progress_stop(cam->progress);
            break;

        default:
            break;
        }
    }

    if (data->op == ENTANGLE_CAMERA_PROGRESS_START)
        g_free(data->msg);
    g_object_unref(data->cam);
    g_free(data);

    return FALSE;
}

static unsigned int
do_entangle_camera_progress_start(GPContext *ctx G_GNUC_UNUSED,
                                  float target,
                                  const char *msg,
                                  void *opaque)
{
    EntangleCamera *cam = opaque;
    struct EntangleCameraProgressData *data =
        g_new0(struct EntangleCameraProgressData, 1);

    data->cam = g_object_ref(cam);
    data->op = ENTANGLE_CAMERA_PROGRESS_START;
    data->value = target;
    data->msg = g_strdup(msg);

    g_idle_add(entangle_camera_progress_idle, data);

    return 0; /* XXX what is this actually useful for ? */
}

static void
do_entangle_camera_progress_update(GPContext *ctx G_GNUC_UNUSED,
                                   unsigned int id G_GNUC_UNUSED,
                                   float current,
                                   void *opaque)
{
    EntangleCamera *cam = opaque;
    struct EntangleCameraProgressData *data =
        g_new0(struct EntangleCameraProgressData, 1);

    data->cam = g_object_ref(cam);
    data->op = ENTANGLE_CAMERA_PROGRESS_UPDATE;
    data->value = current;

    g_idle_add(entangle_camera_progress_idle, data);
}

static void
do_entangle_camera_progress_stop(GPContext *ctx G_GNUC_UNUSED,
                                 unsigned int id G_GNUC_UNUSED,
                                 void *opaque)
{
    EntangleCamera *cam = opaque;
    struct EntangleCameraProgressData *data =
        g_new0(struct EntangleCameraProgressData, 1);

    data->cam = g_object_ref(cam);
    data->op = ENTANGLE_CAMERA_PROGRESS_STOP;

    g_idle_add(entangle_camera_progress_idle, data);
}

static void
do_entangle_camera_error(GPContext *ctx G_GNUC_UNUSED,
                         const char *msg,
                         void *data)
{
    EntangleCamera *cam = data;

    g_free(cam->lastError);
    cam->lastError = g_strdup(msg);
    ENTANGLE_DEBUG("Got error %s", cam->lastError);
}

/**
 * entangle_camera_open:
 * @cam: (transfer none): the camera
 * @error: a GError, or NULL
 *
 * Attempt to open to and initialize the camera. This
 * may fail if the camera is in use by another application,
 * has gone to sleep or has been disconnected from the port.
 *
 * This block execution of the caller until completion.
 *
 * Returns: TRUE if the camera is connected, FALSE on error
 */
gboolean
entangle_camera_open(EntangleCamera *cam, GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    int i;
    GPPortInfo port;
    CameraAbilities cap;
    CameraText txt;
    int err;
    gboolean ret = FALSE;

    ENTANGLE_DEBUG("Conencting to cam");

    g_mutex_lock(&cam->lock);

    if (cam->cam != NULL) {
        ret = TRUE;
        goto cleanup;
    }

    cam->ctx = gp_context_new();

    if (gp_abilities_list_new(&cam->caps) != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Cannot initialize gphoto2 abilities"));
        goto cleanup;
    }

    if (gp_abilities_list_load(cam->caps, cam->ctx) != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Cannot load gphoto2 abilities"));
        goto cleanup;
    }

    if (gp_port_info_list_new(&cam->ports) != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Cannot initialize gphoto2 ports"));
        goto cleanup;
    }

    if (gp_port_info_list_load(cam->ports) != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Cannot load gphoto2 ports"));
        goto cleanup;
    }

    gp_context_set_error_func(cam->ctx, do_entangle_camera_error, cam);
    gp_context_set_progress_funcs(cam->ctx, do_entangle_camera_progress_start,
                                  do_entangle_camera_progress_update,
                                  do_entangle_camera_progress_stop, cam);

    i = gp_port_info_list_lookup_path(cam->ports, cam->port);
    gp_port_info_list_get_info(cam->ports, i, &port);

    i = gp_abilities_list_lookup_model(cam->caps, cam->model);
    gp_abilities_list_get_abilities(cam->caps, i, &cap);

    gp_camera_new(&cam->cam);
    gp_camera_set_abilities(cam->cam, cap);
    gp_camera_set_port_info(cam->cam, port);

    entangle_camera_begin_job(cam);
    err = gp_camera_init(cam->cam, cam->ctx);
    entangle_camera_end_job(cam);

    if (err != GP_OK) {
        gp_camera_unref(cam->cam);
        cam->cam = NULL;
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to initialize camera: %s"), cam->lastError);
        goto cleanup;
    }

    /* Update capabilities as a sanity-check against orignal constructor */
    cam->hasCapture = cam->hasPreview = cam->hasSettings = FALSE;
    if (cap.operations & GP_OPERATION_CAPTURE_IMAGE)
        cam->hasCapture = TRUE;
    if (cap.operations & GP_OPERATION_CAPTURE_PREVIEW)
        cam->hasPreview = TRUE;
    if (cap.operations & GP_OPERATION_CONFIG)
        cam->hasSettings = TRUE;
    cam->hasViewfinder = FALSE;

    gp_camera_get_summary(cam->cam, &txt, cam->ctx);
    cam->summary = g_strdup(txt.text);

    gp_camera_get_manual(cam->cam, &txt, cam->ctx);
    cam->manual = g_strdup(txt.text);

    gp_camera_get_about(cam->cam, &txt, cam->ctx);
    cam->driver = g_strdup(txt.text);

    ENTANGLE_DEBUG("ok");
    ret = TRUE;

cleanup:
    g_mutex_unlock(&cam->lock);
    if (ret)
        entangle_camera_emit_deferred(cam, "camera-opened", NULL);
    return ret;
}

static void
entangle_camera_open_helper(GTask *task,
                            gpointer object,
                            gpointer task_data G_GNUC_UNUSED,
                            GCancellable *cancellable G_GNUC_UNUSED)
{
    GError *error = NULL;

    if (!entangle_camera_open(ENTANGLE_CAMERA(object), &error)) {
        g_task_return_error(task, error);
    } else {
        g_task_return_boolean(task, TRUE);
    }
}

/**
 * entangle_camera_open_async:
 * @cam: (transfer none): the camera
 * @cancellable: (nullable): optional GCancellable object, NULL to ignore.
 * @callback: (scope async): a GAsyncReadyCallback to call when the request is
 * satisfied.
 * @user_data: the data to pass to callback function.
 *
 * Attempt to open to and initialize the camera. This
 * may fail if the camera is in use by another application,
 * has gone to sleep or has been disconnected from the port.
 *
 * This will execute in the background, and invoke @callback
 * when complete, whereupon entangle_camera_open_finish
 * can be used to check the status
 */
void
entangle_camera_open_async(EntangleCamera *cam,
                           GCancellable *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA(cam));

    GTask *task = g_task_new(cam, cancellable, callback, user_data);

    g_task_run_in_thread(task, entangle_camera_open_helper);
    g_object_unref(task);
}

/**
 * entangle_camera_open_finish:
 * @cam: (transfer none): the camera
 * @result: a GAsyncResult
 * @error: a GError, or NULL
 *
 * Check the completion status of a previous call to
 * entangle_camera_open_async
 *
 * Returns: TRUE if the camera is opened, FALSE on error
 */
gboolean
entangle_camera_open_finish(EntangleCamera *cam,
                            GAsyncResult *result,
                            GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    return g_task_propagate_boolean(G_TASK(result), error);
}

/**
 * entangle_camera_close:
 * @cam: (transfer none): the camera
 * @error: a GError, or NULL
 *
 * Close from the camera, enabling it to be used by
 * other applications.
 *
 * This block execution of the caller until completion.
 *
 * Returns: TRUE if the camera is closed, FALSE on error
 */
gboolean
entangle_camera_close(EntangleCamera *cam, GError **error G_GNUC_UNUSED)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    gboolean ret = FALSE;

    ENTANGLE_DEBUG("Closeing from cam");

    g_mutex_lock(&cam->lock);

    if (cam->cam == NULL) {
        ret = TRUE;
        goto cleanup;
    }

    entangle_camera_begin_job(cam);
    gp_camera_exit(cam->cam, cam->ctx);
    entangle_camera_end_job(cam);

    if (cam->widgets) {
        gp_widget_unref(cam->widgets);
        cam->widgets = NULL;
    }
    if (cam->controls) {
        g_object_unref(cam->controls);
        cam->controls = NULL;
    }
    if (cam->controlPaths) {
        g_hash_table_unref(cam->controlPaths);
        cam->controlPaths = NULL;
    }

    g_free(cam->driver);
    g_free(cam->manual);
    g_free(cam->summary);
    cam->driver = cam->manual = cam->summary = NULL;

    if (cam->ports) {
        gp_port_info_list_free(cam->ports);
        cam->ports = NULL;
    }
    if (cam->caps) {
        gp_abilities_list_free(cam->caps);
        cam->caps = NULL;
    }
    gp_context_unref(cam->ctx);
    cam->ctx = NULL;

    gp_camera_unref(cam->cam);
    cam->cam = NULL;
    cam->hasViewfinder = FALSE;

    ret = TRUE;
cleanup:
    g_mutex_unlock(&cam->lock);
    if (ret)
        entangle_camera_emit_deferred(cam, "camera-closed", NULL);
    return ret;
}

static void
entangle_camera_close_helper(GTask *task,
                             gpointer object,
                             gpointer task_data G_GNUC_UNUSED,
                             GCancellable *cancellable G_GNUC_UNUSED)
{
    GError *error = NULL;

    if (!entangle_camera_close(ENTANGLE_CAMERA(object), &error)) {
        g_task_return_error(task, error);
    } else {
        g_task_return_boolean(task, TRUE);
    }
}

/**
 * entangle_camera_close_async:
 * @cam: (transfer none): the camera
 * @cancellable: (nullable): optional GCancellable object, NULL to ignore.
 * @callback: (scope async): a GAsyncReadyCallback to call when the request is
 * satisfied.
 * @user_data: the data to pass to callback function.
 *
 * Close from the camera, enabling it to be used by
 * other applications.
 *
 * This will execute in the background, and invoke @callback
 * when complete, whereupon entangle_camera_open_async
 * can be used to check the status
 */
void
entangle_camera_close_async(EntangleCamera *cam,
                            GCancellable *cancellable,
                            GAsyncReadyCallback callback,
                            gpointer user_data)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA(cam));

    GTask *task = g_task_new(cam, cancellable, callback, user_data);

    g_task_run_in_thread(task, entangle_camera_close_helper);
    g_object_unref(task);
}

/**
 * entangle_camera_close_finish:
 * @cam: (transfer none): the camera
 * @result: a GAsyncResult
 * @error: a GError, or NULL
 *
 * Check the completion status of a previous call to
 * entangle_camera_close_async
 *
 * Returns: TRUE if the camera is closed, FALSE on error
 */
gboolean
entangle_camera_close_finish(EntangleCamera *cam,
                             GAsyncResult *result,
                             GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    return g_task_propagate_boolean(G_TASK(result), error);
}

/**
 * entangle_camera_get_opened:
 * @cam: (transfer none): the camera
 *
 * Determine if the camera is currently opened
 *
 * Returns: TRUE if the camera is opened, FALSE otherwise
 */
gboolean
entangle_camera_get_opened(EntangleCamera *cam)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    gboolean ret;

    g_mutex_lock(&cam->lock);
    ret = cam->cam != NULL ? TRUE : FALSE;
    g_mutex_unlock(&cam->lock);
    return ret;
}

/**
 * entangle_camera_get_summary:
 * @cam: (transfer none): the camera
 *
 * Get the camera summary text. This is only available
 * while the camera is opened
 *
 * Returns: (transfer full): the camera summary
 */
char *
entangle_camera_get_summary(EntangleCamera *cam)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), NULL);

    char *ret;

    g_mutex_lock(&cam->lock);
    ret = g_strdup(cam->summary);
    g_mutex_unlock(&cam->lock);

    return ret;
}

/**
 * entangle_camera_get_manual:
 * @cam: (transfer none): the camera
 *
 * Get the camera manual text. This is only available
 * while the camera is opened
 *
 * Returns: (transfer full): the camera manual
 */
char *
entangle_camera_get_manual(EntangleCamera *cam)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), NULL);

    char *ret;

    g_mutex_lock(&cam->lock);
    ret = g_strdup(cam->manual);
    g_mutex_unlock(&cam->lock);

    return ret;
}

/**
 * entangle_camera_get_driver:
 * @cam: (transfer none): the camera
 *
 * Get the camera driver information text. This is only available
 * while the camera is opened
 *
 * Returns: (transfer full): the camera driver information
 */
char *
entangle_camera_get_driver(EntangleCamera *cam)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), NULL);

    char *ret;

    g_mutex_lock(&cam->lock);
    ret = g_strdup(cam->driver);
    g_mutex_unlock(&cam->lock);

    return ret;
}

/**
 * entangle_camera_capture_image:
 * @cam: (transfer none): the camera
 * @error: a GError, or NULL
 *
 * Trigger the camera shutter and download the first resulting
 * image. If the camera is shooting in multiple formats (eg JPEG
 * and RAW) this method will only return the first format captured
 * The caller should watch for signal notifications to detect any
 * additional images
 *
 * This can only be invoked when the camera is opened.
 *
 * This block execution of the caller until completion.
 *
 * Returns: (transfer full): the captured image or NULL
 */
EntangleCameraFile *
entangle_camera_capture_image(EntangleCamera *cam, GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), NULL);

    CameraFilePath camerapath;
    EntangleCameraFile *file = NULL;
    int err;

    g_mutex_lock(&cam->lock);

    if (!cam->cam) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Cannot capture image while not opened"));
        goto cleanup;
    }

    ENTANGLE_DEBUG("Starting capture");
    entangle_camera_begin_job(cam);
    err = gp_camera_capture(cam->cam, GP_CAPTURE_IMAGE, &camerapath, cam->ctx);
    entangle_camera_end_job(cam);
    if (err != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to capture image: %s"), cam->lastError);
        goto cleanup;
    }

    file = entangle_camera_file_new(camerapath.folder, camerapath.name);

    entangle_camera_emit_deferred(cam, "camera-file-captured", G_OBJECT(file));

cleanup:
    g_mutex_unlock(&cam->lock);
    return file;
}

static void
entangle_camera_capture_image_helper(GTask *task,
                                     gpointer object,
                                     gpointer task_data G_GNUC_UNUSED,
                                     GCancellable *cancellable G_GNUC_UNUSED)
{
    EntangleCameraFile *file;
    GError *error = NULL;

    if (!(file =
              entangle_camera_capture_image(ENTANGLE_CAMERA(object), &error))) {
        g_task_return_error(task, error);
    } else {
        g_task_return_pointer(task, file, g_object_unref);
    }
}

/**
 * entangle_camera_capture_image_async:
 * @cam: (transfer none): the camera
 * @cancellable: (nullable): optional GCancellable object, NULL to ignore.
 * @callback: (scope async): a GAsyncReadyCallback to call when the request is
 * satisfied.
 * @user_data: the data to pass to callback function.
 *
 * Trigger the camera shutter and download the first resulting
 * image. If the camera is shooting in multiple formats (eg JPEG
 * and RAW) this method will only return the first format captured
 * The caller should watch for signal notifications to detect any
 * additional images
 *
 * This can only be invoked when the camera is opened.
 *
 * This will execute in the background, and invoke @callback
 * when complete, whereupon entangle_camera_capture_image_finish
 * can be used to check the status
 */
void
entangle_camera_capture_image_async(EntangleCamera *cam,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA(cam));

    GTask *task = g_task_new(cam, cancellable, callback, user_data);

    g_task_run_in_thread(task, entangle_camera_capture_image_helper);
    g_object_unref(task);
}

/**
 * entangle_camera_capture_image_finish:
 * @cam: (transfer none): the camera
 * @result: a GAsyncResult
 * @error: a GError, or NULL
 *
 * Check the completion status of a previous call to
 * entangle_camera_capture_image_async.
 *
 * Returns: (transfer full): the captured image or NULL
 */
EntangleCameraFile *
entangle_camera_capture_image_finish(EntangleCamera *cam,
                                     GAsyncResult *result,
                                     GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), NULL);

    return g_task_propagate_pointer(G_TASK(result), error);
}

/**
 * entangle_camera_preview_image:
 * @cam: (transfer none): the camera
 * @error: a GError, or NULL
 *
 * Enable "live view", if not already enabled, and capture a
 * low resolution preview image. The "live view" mode will
 * remain enabled after execution.
 *
 * This can only be invoked when the camera is opened.
 *
 * This block execution of the caller until completion.
 *
 * Returns: (transfer full): the captured image or NULL
 */
EntangleCameraFile *
entangle_camera_preview_image(EntangleCamera *cam, GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), NULL);

    EntangleCameraFile *file = NULL;
    CameraFile *datafile = NULL;
    const char *mimetype = NULL;
    GByteArray *data = NULL;
    const char *rawdata;
    unsigned long int rawdatalen;
    const char *name;
    int err;

    g_mutex_lock(&cam->lock);

    if (!cam->cam) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Cannot preview image while not opened"));
        goto cleanup;
    }

    gp_file_new(&datafile);

    ENTANGLE_DEBUG("Starting preview");
    entangle_camera_begin_job(cam);
    err = gp_camera_capture_preview(cam->cam, datafile, cam->ctx);
    entangle_camera_end_job(cam);

    if (err != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to capture preview: %s"), cam->lastError);
        goto cleanup;
    }

    if (gp_file_get_data_and_size(datafile, &rawdata, &rawdatalen) != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to get file data: %s"), cam->lastError);
        goto cleanup;
    }

    if (gp_file_get_name(datafile, &name) != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to get filename: %s"), cam->lastError);
        goto cleanup;
    }

    file = entangle_camera_file_new(NULL, NULL);

    if (gp_file_get_mime_type(datafile, &mimetype) == GP_OK)
        entangle_camera_file_set_mimetype(file, mimetype);

    data = g_byte_array_new();
    g_byte_array_append(data, (const guint8 *)rawdata, rawdatalen);

    entangle_camera_file_set_data(file, data);
    g_byte_array_unref(data);

    entangle_camera_emit_deferred(cam, "camera-file-previewed", G_OBJECT(file));

cleanup:
    if (datafile)
        gp_file_unref(datafile);
    g_mutex_unlock(&cam->lock);
    return file;
}

static void
entangle_camera_preview_image_helper(GTask *task,
                                     gpointer object,
                                     gpointer task_data G_GNUC_UNUSED,
                                     GCancellable *cancellable G_GNUC_UNUSED)
{
    EntangleCameraFile *file;
    GError *error = NULL;

    if (!(file =
              entangle_camera_preview_image(ENTANGLE_CAMERA(object), &error))) {
        g_task_return_error(task, error);
    } else {
        g_task_return_pointer(task, file, g_object_unref);
    }
}

/**
 * entangle_camera_preview_image_async:
 * @cam: (transfer none): the camera
 * @cancellable: (nullable): optional GCancellable object, NULL to ignore.
 * @callback: (scope async): a GAsyncReadyCallback to call when the request is
 * satisfied.
 * @user_data: the data to pass to callback function.
 *
 * Enable "live view", if not already enabled, and capture a
 * low resolution preview image. The "live view" mode will
 * remain enabled after execution.
 *
 * This can only be invoked when the camera is opened.
 *
 * This will execute in the background, and invoke @callback
 * when complete, whereupon entangle_camera_preview_image_finish
 * can be used to check the status
 */
void
entangle_camera_preview_image_async(EntangleCamera *cam,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA(cam));

    GTask *task = g_task_new(cam, cancellable, callback, user_data);

    g_task_run_in_thread(task, entangle_camera_preview_image_helper);
    g_object_unref(task);
}

/**
 * entangle_camera_preview_image_finish:
 * @cam: (transfer none): the camera
 * @result: a GAsyncResult
 * @error: a GError, or NULL
 *
 * Check the completion status of a previous call to
 * entangle_camera_preview_image_async.
 *
 * Returns: (transfer full): the captured image or NULL
 */
EntangleCameraFile *
entangle_camera_preview_image_finish(EntangleCamera *cam,
                                     GAsyncResult *result,
                                     GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), NULL);

    return g_task_propagate_pointer(G_TASK(result), error);
}

/**
 * entangle_camera_download_file:
 * @cam: (transfer none): the camera
 * @file: (transfer none): the file whose contents to download
 * @error: a GError, or NULL
 *
 * Download the data associated with @file and set the data
 * on @file.
 *
 * This can only be invoked when the camera is opened.
 *
 * This block execution of the caller until completion.
 *
 * Returns: TRUE if the file was downloaded, FALSE on error
 */
gboolean
entangle_camera_download_file(EntangleCamera *cam,
                              EntangleCameraFile *file,
                              GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);
    g_return_val_if_fail(ENTANGLE_IS_CAMERA_FILE(file), FALSE);

    CameraFile *datafile = NULL;
    const char *data;
    unsigned long int datalen;
    GByteArray *filedata;
    gboolean ret = FALSE;
    int err;

    g_mutex_lock(&cam->lock);

    if (!cam->cam) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Cannot download file while not opened"));
        goto cleanup;
    }

    ENTANGLE_DEBUG("Downloading '%s' from '%s'",
                   entangle_camera_file_get_name(file),
                   entangle_camera_file_get_folder(file));

    gp_file_new(&datafile);

    ENTANGLE_DEBUG("Getting file data");
    entangle_camera_begin_job(cam);
    err = gp_camera_file_get(cam->cam, entangle_camera_file_get_folder(file),
                             entangle_camera_file_get_name(file),
                             GP_FILE_TYPE_NORMAL, datafile, cam->ctx);
    g_usleep(1000 * 100);
    entangle_camera_end_job(cam);

    if (err != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to get camera file: %s"), cam->lastError);
        goto cleanup;
    }

    ENTANGLE_DEBUG("Fetching data");
    if (gp_file_get_data_and_size(datafile, &data, &datalen) != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to get file data: %s"), cam->lastError);
        goto cleanup;
    }

    filedata = g_byte_array_new();
    g_byte_array_append(filedata, (const guint8 *)data, datalen);

    entangle_camera_file_set_data(file, filedata);
    g_byte_array_unref(filedata);

    entangle_camera_emit_deferred(cam, "camera-file-downloaded",
                                  G_OBJECT(file));

    ret = TRUE;

cleanup:
    ENTANGLE_DEBUG("Error");
    if (datafile)
        gp_file_unref(datafile);
    g_mutex_unlock(&cam->lock);
    return ret;
}

static void
entangle_camera_download_file_helper(GTask *task,
                                     gpointer object,
                                     gpointer task_data,
                                     GCancellable *cancellable G_GNUC_UNUSED)
{
    EntangleCameraFile *file = ENTANGLE_CAMERA_FILE(task_data);
    GError *error = NULL;

    if (!entangle_camera_download_file(ENTANGLE_CAMERA(object), file, &error)) {
        g_task_return_error(task, error);
    } else {
        g_task_return_boolean(task, TRUE);
    }
}

/**
 * entangle_camera_download_file_async:
 * @cam: (transfer none): the camera
 * @file: (transfer none): the file whose contents to download
 * @cancellable: (nullable): optional GCancellable object, NULL to ignore.
 * @callback: (scope async): a GAsyncReadyCallback to call when the request is
 * satisfied.
 * @user_data: the data to pass to callback function.
 *
 * Download the data associated with @file and set the data
 * on @file.
 *
 * This can only be invoked when the camera is opened.
 *
 * This will execute in the background, and invoke @callback
 * when complete, whereupon entangle_camera_download_file_finish
 * can be used to check the status
 */
void
entangle_camera_download_file_async(EntangleCamera *cam,
                                    EntangleCameraFile *file,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA(cam));
    g_return_if_fail(ENTANGLE_IS_CAMERA_FILE(file));

    GTask *task = g_task_new(cam, cancellable, callback, user_data);

    g_object_ref(file);
    g_task_set_task_data(task, file, g_object_unref);

    g_task_run_in_thread(task, entangle_camera_download_file_helper);
    g_object_unref(task);
}

/**
 * entangle_camera_download_file_finish:
 * @cam: (transfer none): the camera
 * @result: a GAsyncResult
 * @error: a GError, or NULL
 *
 * Check the completion status of a previous call to
 * entangle_camera_download_file_async.
 *
 * Returns: TRUE if the file was downloaded, FALSE on error
 */
gboolean
entangle_camera_download_file_finish(EntangleCamera *cam,
                                     GAsyncResult *result,
                                     GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    return g_task_propagate_boolean(G_TASK(result), error);
}

/**
 * entangle_camera_delete_file:
 * @cam: (transfer none): the camera
 * @file: (transfer none): the file to delete
 * @error: a GError, or NULL
 *
 * Delete @file from the camera capture target.
 *
 * This can only be invoked when the camera is opened.
 *
 * This block execution of the caller until completion.
 *
 * Returns: TRUE if the file was deleted, FALSE on error
 */
gboolean
entangle_camera_delete_file(EntangleCamera *cam,
                            EntangleCameraFile *file,
                            GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);
    g_return_val_if_fail(ENTANGLE_IS_CAMERA_FILE(file), FALSE);

    gboolean ret = FALSE;
    int err;

    g_mutex_lock(&cam->lock);

    if (!cam->cam) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Cannot delete file while not opened"));
        goto cleanup;
    }

    ENTANGLE_DEBUG("Deleting '%s' from '%s'",
                   entangle_camera_file_get_name(file),
                   entangle_camera_file_get_folder(file));

    entangle_camera_begin_job(cam);
    err = gp_camera_file_delete(cam->cam, entangle_camera_file_get_folder(file),
                                entangle_camera_file_get_name(file), cam->ctx);
    g_usleep(1000 * 100);
    entangle_camera_end_job(cam);

    if (err != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to delete file: %s"), cam->lastError);
        goto cleanup;
    }

    entangle_camera_emit_deferred(cam, "camera-file-deleted", G_OBJECT(file));

    ret = TRUE;

cleanup:
    g_mutex_unlock(&cam->lock);
    return ret;
}

static void
entangle_camera_delete_file_helper(GTask *task,
                                   gpointer object,
                                   gpointer task_data,
                                   GCancellable *cancellable G_GNUC_UNUSED)
{
    EntangleCameraFile *file = ENTANGLE_CAMERA_FILE(task_data);
    GError *error = NULL;

    if (!entangle_camera_delete_file(ENTANGLE_CAMERA(object), file, &error)) {
        g_task_return_error(task, error);
    } else {
        g_task_return_boolean(task, TRUE);
    }
}

/**
 * entangle_camera_delete_file_async:
 * @cam: (transfer none): the camera
 * @file: (transfer none): the file to delete
 * @cancellable: (nullable): optional GCancellable object, NULL to ignore.
 * @callback: (scope async): a GAsyncReadyCallback to call when the request is
 * satisfied.
 * @user_data: the data to pass to callback function.
 *
 * Delete @file from the camera capture target.
 *
 * This can only be invoked when the camera is opened.
 *
 * This will execute in the background, and invoke @callback
 * when complete, whereupon entangle_camera_delete_file_finish
 * can be used to check the status
 */
void
entangle_camera_delete_file_async(EntangleCamera *cam,
                                  EntangleCameraFile *file,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA(cam));
    g_return_if_fail(ENTANGLE_IS_CAMERA_FILE(file));

    GTask *task = g_task_new(cam, cancellable, callback, user_data);

    g_object_ref(file);
    g_task_set_task_data(task, file, g_object_unref);

    g_task_run_in_thread(task, entangle_camera_delete_file_helper);
    g_object_unref(task);
}

/**
 * entangle_camera_delete_file_finish:
 * @cam: (transfer none): the camera
 * @result: a GAsyncResult
 * @error: a GError, or NULL
 *
 * Check the completion status of a previous call to
 * entangle_camera_delete_file_async.
 *
 * Returns: TRUE if the file was deleted, FALSE on error
 */
gboolean
entangle_camera_delete_file_finish(EntangleCamera *cam,
                                   GAsyncResult *result,
                                   GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    return g_task_propagate_boolean(G_TASK(result), error);
}

/**
 * entangle_camera_process_events:
 * @cam: (transfer none): the camera
 * @waitms: the number of milliseconds to wait
 * @error: a GError, or NULL
 *
 * Wait upto @waitms milliseconds for events to arrive from
 * the camera. Signals will be emitted for any interesting
 * events that arrive. Multiple events will be processed
 * until @waitms is exceeded.
 *
 * This can only be invoked when the camera is opened.
 *
 * This block execution of the caller until completion.
 *
 * Returns: TRUE if the file was deleted, FALSE on error
 */
gboolean
entangle_camera_process_events(EntangleCamera *cam,
                               guint64 waitms,
                               GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    CameraEventType eventType = 0;
    void *eventData = NULL;
    guint64 startms, endms, donems;
    gboolean ret = FALSE;
    int err;

    g_mutex_lock(&cam->lock);

    if (!cam->cam) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Cannot wait for events while not opened"));
        goto cleanup;
    }

    startms = g_get_real_time() / 1000;

    ENTANGLE_DEBUG("Waiting for events start %llu duration %llu",
                   (unsigned long long)startms, (unsigned long long)waitms);

    donems = 0;
    do {
        entangle_camera_begin_job(cam);
        err = gp_camera_wait_for_event(cam->cam, waitms - donems, &eventType,
                                       &eventData, cam->ctx);
        entangle_camera_end_job(cam);

        if (err != GP_OK) {
            /* Some drivers (eg canon native) can't do events, so just do a
             * sleep */
            if (err == GP_ERROR_NOT_SUPPORTED) {
                ENTANGLE_DEBUG("Event wait not supported, using usleep");
                g_usleep((waitms - donems) * 1000ll);
                ret = TRUE;
                goto cleanup;
            }
            g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                        _("Unable to wait for events: %s"), cam->lastError);
            goto cleanup;
        }
        ENTANGLE_DEBUG("Event type %d", eventType);
        switch (eventType) {
        case GP_EVENT_UNKNOWN:
            if (eventData && strstr((char *)eventData, "PTP Property") &&
                strstr((char *)eventData, "changed")) {
                ENTANGLE_DEBUG("Config changed '%s'", (char *)eventData);
                /* For some reason, every time we request the camera config
                 * with gp_camera_get_config, it will be followed by an
                 * event with key 'd10d'. So we must ignore that event
                 */
                if (strstr(eventData, "d10d") == NULL)
                    entangle_camera_emit_deferred(
                        cam, "camera-controls-changed", NULL);
            } else {
                ENTANGLE_DEBUG("Unknown event '%s'", (char *)eventData);
            }
            break;

        case GP_EVENT_TIMEOUT:
            ENTANGLE_DEBUG("Wait timed out");
            break;

        case GP_EVENT_FILE_ADDED: {
            CameraFilePath *camerapath = eventData;
            EntangleCameraFile *file;

            ENTANGLE_DEBUG("File added '%s' in '%s'", camerapath->name,
                           camerapath->folder);

            file =
                entangle_camera_file_new(camerapath->folder, camerapath->name);

            entangle_camera_emit_deferred(cam, "camera-file-added",
                                          G_OBJECT(file));

            g_object_unref(file);
        } break;

        case GP_EVENT_FOLDER_ADDED: {
            CameraFilePath *camerapath = eventData;

            ENTANGLE_DEBUG("Folder added '%s' in '%s'", camerapath->name,
                           camerapath->folder);
        } break;

        case GP_EVENT_CAPTURE_COMPLETE:
            ENTANGLE_DEBUG("Capture is complete");
            break;

        default:
            ENTANGLE_DEBUG("Unexpected event received %d", eventType);
            break;
        }

        free(eventData);
        eventData = NULL;
        endms = g_get_real_time() / 1000;
        donems = endms - startms;
    } while (eventType != GP_EVENT_TIMEOUT && donems < waitms);

    ENTANGLE_DEBUG("Done waiting for events %llu", (unsigned long long)donems);

    ret = TRUE;

cleanup:
    free(eventData);
    g_mutex_unlock(&cam->lock);
    return ret;
}

static void
entangle_camera_process_events_helper(GTask *task,
                                      gpointer object,
                                      gpointer task_data,
                                      GCancellable *cancellable G_GNUC_UNUSED)
{
    guint64 *waitptr = task_data;
    GError *error = NULL;

    if (!entangle_camera_process_events(ENTANGLE_CAMERA(object), *waitptr,
                                        &error)) {
        g_task_return_error(task, error);
    } else {
        g_task_return_boolean(task, TRUE);
    }
}

/**
 * entangle_camera_process_events_async:
 * @cam: (transfer none): the camera
 * @waitms: the number of milliseconds to wait
 * @cancellable: (nullable): optional GCancellable object, NULL to ignore.
 * @callback: (scope async): a GAsyncReadyCallback to call when the request is
 * satisfied.
 * @user_data: the data to pass to callback function.
 *
 * Wait upto @waitms milliseconds for events to arrive from
 * the camera. Signals will be emitted for any interesting
 * events that arrive. Multiple events will be processed
 * until @waitms is exceeded.
 *
 * This can only be invoked when the camera is opened.
 *
 * This will execute in the background, and invoke @callback
 * when complete, whereupon entangle_camera_process_events_finish
 * can be used to check the status
 */
void
entangle_camera_process_events_async(EntangleCamera *cam,
                                     guint64 waitms,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA(cam));

    guint64 *waitptr = g_new0(guint64, 1);
    GTask *task = g_task_new(cam, cancellable, callback, user_data);

    *waitptr = waitms;
    g_task_set_task_data(task, waitptr, g_free);

    g_task_run_in_thread(task, entangle_camera_process_events_helper);
    g_object_unref(task);
}

/**
 * entangle_camera_process_events_finish:
 * @cam: (transfer none): the camera
 * @result: a GAsyncResult
 * @error: a GError, or NULL
 *
 * Check the completion status of a previous call to
 * entangle_camera_process_events_async.
 *
 * Returns: TRUE if events were processed, FALSE on error
 */
gboolean
entangle_camera_process_events_finish(EntangleCamera *cam,
                                      GAsyncResult *result,
                                      GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    return g_task_propagate_boolean(G_TASK(result), error);
}

static CameraWidget *
entangle_camera_find_widget(EntangleCamera *cam, const gchar *path)
{
    gchar **names = g_strsplit(path, "/", 0);
    CameraWidget *ret = NULL;
    CameraWidget *curr = cam->widgets;
    gsize i;

    for (i = 0; names[i] != NULL; i++) {
        CameraWidget *tmp;

        if (g_str_equal(names[i], "") || g_str_equal(names[i], "main"))
            continue;

        if (gp_widget_get_child_by_name(curr, names[i], &tmp) != GP_OK)
            goto cleanup;

        curr = tmp;
    }

    ret = curr;

cleanup:
    g_strfreev(names);
    return ret;
}

static EntangleControl *
do_build_controls(EntangleCamera *cam,
                  const char *path,
                  CameraWidget *widget,
                  GError **error)
{
    CameraWidgetType type;
    EntangleControl *ret = NULL;
    const char *name;
    char *fullpath;
    int id;
    const char *label;
    const char *info;
    int ro;

    if (gp_widget_get_type(widget, &type) != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to fetch widget type"));
        return NULL;
    }

    if (gp_widget_get_name(widget, &name) != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to fetch widget name"));
        return NULL;
    }

    gp_widget_get_id(widget, &id);
    gp_widget_get_label(widget, &label);
    gp_widget_get_info(widget, &info);
    gp_widget_get_readonly(widget, &ro);
    if (info == NULL)
        info = label;

    fullpath = g_strdup_printf("%s/%s", path, name);

    switch (type) {
        /* We treat both window and section as just groups */
    case GP_WIDGET_WINDOW: {
        EntangleControlGroup *grp;
        ENTANGLE_DEBUG("Add group %s %d %s", fullpath, id, label);
        grp = entangle_control_group_new(fullpath, id, label, info, ro);
        for (int i = 0; i < gp_widget_count_children(widget); i++) {
            CameraWidget *child;
            EntangleControl *subctl;
            const char *childname;
            if (gp_widget_get_child(widget, i, &child) != GP_OK ||
                gp_widget_get_name(child, &childname) != GP_OK) {
                g_object_unref(grp);
                goto error;
            }

            /* Actions are exposed as normal APIs
             * And "Other ptp properties" 90% dups
             */
            if (g_str_equal(childname, "actions") ||
                g_str_equal(childname, "other"))
                continue;

            if (!(subctl = do_build_controls(cam, fullpath, child, error))) {
                g_object_unref(grp);
                goto error;
            }

            entangle_control_group_add(grp, subctl);
        }

        ret = ENTANGLE_CONTROL(grp);
    } break;

    case GP_WIDGET_SECTION: {
        EntangleControlGroup *grp;
        ENTANGLE_DEBUG("Add group %s %d %s", fullpath, id, label);
        grp = entangle_control_group_new(fullpath, id, label, info, ro);
        for (int i = 0; i < gp_widget_count_children(widget); i++) {
            CameraWidget *child;
            EntangleControl *subctl;
            if (gp_widget_get_child(widget, i, &child) != GP_OK) {
                g_object_unref(grp);
                goto error;
            }
            if (!(subctl = do_build_controls(cam, fullpath, child, error))) {
                g_object_unref(grp);
                goto error;
            }

            entangle_control_group_add(grp, subctl);
        }

        ret = ENTANGLE_CONTROL(grp);
    } break;

    case GP_WIDGET_BUTTON: {
        ENTANGLE_DEBUG("Add button %s %d %s", fullpath, id, label);
        ret = ENTANGLE_CONTROL(
            entangle_control_button_new(fullpath, id, label, info, ro));
    } break;

        /* Unclear why these two are the same in libgphoto */
    case GP_WIDGET_RADIO:
    case GP_WIDGET_MENU: {
        ENTANGLE_DEBUG("Add menu %s %d %s", fullpath, id, label);
        ret = ENTANGLE_CONTROL(
            entangle_control_choice_new(fullpath, id, label, info, ro));
    } break;

    case GP_WIDGET_DATE: {
        ENTANGLE_DEBUG("Add date %s %d %s", fullpath, id, label);
        ret = ENTANGLE_CONTROL(
            entangle_control_date_new(fullpath, id, label, info, ro));
    } break;

    case GP_WIDGET_RANGE: {
        float min, max, step;
        gp_widget_get_range(widget, &min, &max, &step);
        ENTANGLE_DEBUG("Add range %s %d %s %f %f %f", fullpath, id, label,
                       (double)min, (double)max, (double)step);
        ret = ENTANGLE_CONTROL(entangle_control_range_new(
            fullpath, id, label, info, ro, min, max, step));
    } break;

    case GP_WIDGET_TEXT: {
        ENTANGLE_DEBUG("Add date %s %d %s", fullpath, id, label);
        ret = ENTANGLE_CONTROL(
            entangle_control_text_new(fullpath, id, label, info, ro));
    } break;

    case GP_WIDGET_TOGGLE: {
        ENTANGLE_DEBUG("Add date %s %d %s", fullpath, id, label);
        ret = ENTANGLE_CONTROL(
            entangle_control_toggle_new(fullpath, id, label, info, ro));
    } break;

    default:
        g_warn_if_reached();
        break;
    }

    g_hash_table_insert(cam->controlPaths, g_strdup(fullpath), ret);

error:
    g_free(fullpath);
    return ret;
}

static gboolean
entangle_str_equal_null(gchar *a, gchar *b)
{
    if (!a && !b)
        return TRUE;
    if (!a || !b)
        return FALSE;
    return g_str_equal(a, b);
}

/*
 * XXX this method causes signals to be emitted from controls
 * in non-main threads, if triggered via an _async() method.
 * Investigate if we can fix this to always run in main thread
 * to simplify thread safety for GTK frontend
 */
static gdouble
entangle_camera_parse_fnumber(const char *value)
{
    if (!value)
        return -1.0;

    if (g_str_has_prefix(value, "f/"))
        value += 2;

    return g_ascii_strtod(value, NULL);
}

/*
 * Discover the aperture range actually supported by the mounted lens.
 *
 * libgphoto2's Sony driver exposes a hard-coded list of f-numbers
 * (f/1 ... f/64) rather than the values supported by the lens. Asking
 * the camera for a value outside the lens range silently clamps it to
 * the nearest supported value, so two writes are enough to find the
 * real minimum and maximum. The original value is restored afterwards.
 */
static GList *
entangle_camera_filter_fnumber_choices(CameraWidget *widget,
                                       gdouble minf,
                                       gdouble maxf)
{
    GList *ret = NULL;
    int count = gp_widget_count_choices(widget);

    for (int i = 0; i < count; i++) {
        const char *choice = NULL;
        gdouble f;

        if (gp_widget_get_choice(widget, i, &choice) != GP_OK || !choice)
            continue;

        f = entangle_camera_parse_fnumber(choice);
        if (f >= minf - 0.001 && f <= maxf + 0.001)
            ret = g_list_append(ret, g_strdup(choice));
    }

    return ret;
}

static gchar *
entangle_camera_fnumber_cache_path(EntangleCamera *cam)
{
    gchar *name;
    gchar *path;

    if (!cam->serial || !*cam->serial)
        return NULL;

    name = g_strdup_printf("fnumber-%s", cam->serial);
    path = g_build_filename(g_get_user_cache_dir(), "entangle", name, NULL);
    ENTANGLE_DEBUG("F-number cache path '%s'", path);
    g_free(name);

    return path;
}

static GList *
entangle_camera_probe_fnumber_choices(EntangleCamera *cam,
                                      CameraWidget *widget)
{
    int count = gp_widget_count_choices(widget);
    const char *first_raw = NULL;
    const char *current_raw = NULL;
    gchar *first = NULL;
    gchar *current = NULL;
    gchar *cache_path = NULL;
    CameraWidget *readback = NULL;
    const char *value = NULL;
    gdouble minf = -1.0;
    gdouble maxf = -1.0;
    int err = GP_OK;
    GList *ret = NULL;

    if (count < 2)
        return NULL;

    if (gp_widget_get_value(widget, &current_raw) != GP_OK || !current_raw)
        return NULL;
    if (gp_widget_get_choice(widget, 0, &first_raw) != GP_OK || !first_raw)
        return NULL;

    /* gp_widget_get_value() points into the widget, so duplicate the
     * strings before gp_widget_set_value() frees the old value. */
    current = g_strdup(current_raw);
    first = g_strdup(first_raw);

    cache_path = entangle_camera_fnumber_cache_path(cam);
    if (cache_path) {
        gchar *data = NULL;

        if (g_file_get_contents(cache_path, &data, NULL, NULL)) {
            gdouble cmin = -1.0;
            gdouble cmax = -1.0;

            if (sscanf(data, "%lf %lf", &cmin, &cmax) == 2 && cmin >= 0.0 &&
                cmax >= cmin) {
                gdouble cur = entangle_camera_parse_fnumber(current);

                if (cur >= cmin - 0.001 && cur <= cmax + 0.001) {
                    ENTANGLE_DEBUG(
                        "Using cached lens aperture range f/%g - f/%g", cmin,
                        cmax);
                    ret = entangle_camera_filter_fnumber_choices(widget, cmin,
                                                                 cmax);
                    g_free(data);
                    goto out;
                }
            }
            g_free(data);
        }
    }

    gp_widget_set_value(widget, first);
    err = gp_camera_set_config(cam->cam, cam->widgets, cam->ctx);
    ENTANGLE_DEBUG("F-number probe: set '%s' -> %d", first, err);
    if (err == GP_OK &&
        gp_camera_get_single_config(cam->cam, "f-number", &readback,
                                    cam->ctx) == GP_OK) {
        if (gp_widget_get_value(readback, &value) == GP_OK && value) {
            minf = entangle_camera_parse_fnumber(value);
            ENTANGLE_DEBUG("F-number probe: minimum read back '%s'", value);
        }
        gp_widget_free(readback);
        readback = NULL;
    }

    /* Walk up from a likely maximum until the camera clamps the value.
     * Starting near f/22 avoids trying very small apertures like f/64,
     * which can take many seconds on Sony bodies. */
    int start = count - 1;
    for (int i = 0; i < count; i++) {
        const char *choice = NULL;

        if (gp_widget_get_choice(widget, i, &choice) != GP_OK || !choice)
            continue;
        if (entangle_camera_parse_fnumber(choice) >= 22.0) {
            start = i;
            break;
        }
    }

    for (int i = start; i < count; i++) {
        const char *choice = NULL;
        gdouble requested;
        gdouble actual;

        if (gp_widget_get_choice(widget, i, &choice) != GP_OK || !choice)
            continue;

        requested = entangle_camera_parse_fnumber(choice);

        gp_widget_set_value(widget, choice);
        err = gp_camera_set_config(cam->cam, cam->widgets, cam->ctx);
        ENTANGLE_DEBUG("F-number probe: set '%s' -> %d", choice, err);
        if (err != GP_OK)
            break;

        if (gp_camera_get_single_config(cam->cam, "f-number", &readback,
                                        cam->ctx) != GP_OK)
            break;

        if (gp_widget_get_value(readback, &value) != GP_OK || !value) {
            gp_widget_free(readback);
            readback = NULL;
            break;
        }

        actual = entangle_camera_parse_fnumber(value);
        ENTANGLE_DEBUG("F-number probe: maximum read back '%s'", value);
        maxf = actual;
        gp_widget_free(readback);
        readback = NULL;

        if (actual < requested - 0.001)
            break;
    }

    /* Restore the value the camera had before probing. */
    gp_widget_set_value(widget, current);
    err = gp_camera_set_config(cam->cam, cam->widgets, cam->ctx);
    ENTANGLE_DEBUG("F-number probe: restore '%s' -> %d", current, err);

    if (minf < 0.0 || maxf < 0.0 || minf > maxf)
        goto out;

    ENTANGLE_DEBUG("Lens aperture range f/%g - f/%g", (double)minf,
                   (double)maxf);

    if (cache_path) {
        gchar *dir = g_path_get_dirname(cache_path);
        gchar *data = g_strdup_printf("%g %g\n", (double)minf, (double)maxf);

        g_mkdir_with_parents(dir, 0700);
        g_file_set_contents(cache_path, data, -1, NULL);
        g_free(data);
        g_free(dir);
    }

    ret = entangle_camera_filter_fnumber_choices(widget, minf, maxf);

out:
    g_free(first);
    g_free(current);
    g_free(cache_path);
    return ret;
}

static gboolean
do_load_controls(EntangleCamera *cam,
                 const char *path,
                 CameraWidget *widget,
                 GError **error)
{
    CameraWidgetType type;
    EntangleControl *ctrl = NULL;
    const char *name;
    char *fullpath;
    int ro;
    gboolean ret = FALSE;

    if (gp_widget_get_type(widget, &type) != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to fetch widget type"));
        return FALSE;
    }

    if (gp_widget_get_name(widget, &name) != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to fetch widget name"));
        return FALSE;
    }

    gp_widget_get_readonly(widget, &ro);

    fullpath = g_strdup_printf("%s/%s", path, name);

    gboolean is_fnumber =
        g_str_has_suffix(fullpath, "/capturesettings/f-number");
    gboolean is_sony =
        cam->model && (g_str_has_prefix(cam->model, "Sony") ||
                       strstr(cam->model, "ILCE") != NULL);

    ctrl = g_hash_table_lookup(cam->controlPaths, fullpath);
    if (!ctrl) {
        ret = TRUE;
        goto cleanup;
    }
    entangle_control_set_readonly(ctrl, ro ? TRUE : FALSE);

    switch (type) {
        /* We treat both window and section as just groups */
    case GP_WIDGET_WINDOW:
    case GP_WIDGET_SECTION:
        for (int i = 0; i < gp_widget_count_children(widget); i++) {
            CameraWidget *child;
            if (gp_widget_get_child(widget, i, &child) == GP_OK)
                if (!do_load_controls(cam, fullpath, child, error))
                    goto cleanup;
        }
        break;

    case GP_WIDGET_BUTTON: {
    } break;

        /* Unclear why these two are the same in libgphoto */
    case GP_WIDGET_RADIO:
    case GP_WIDGET_MENU: {
        gchar *newValue = NULL;
        gchar *oldValue = NULL;
        g_object_get(ctrl, "value", &oldValue, NULL);
        gp_widget_get_value(widget, &newValue);
        if (!entangle_str_equal_null(newValue, oldValue)) {
            ENTANGLE_DEBUG(
                "Updating value of menu '%s' ('%s') old='%s' new='%s'",
                entangle_control_get_path(ctrl),
                entangle_control_get_label(ctrl), oldValue, newValue);
            if (is_fnumber && is_sony && !ro && !cam->fnumberProbed) {
                cam->supportedFNumbers =
                    entangle_camera_probe_fnumber_choices(cam, widget);
                cam->fnumberProbed = TRUE;
            }

            entangle_control_choice_clear_entries(
                ENTANGLE_CONTROL_CHOICE(ctrl));
            for (int i = 0; i < gp_widget_count_choices(widget); i++) {
                const char *choice;
                gp_widget_get_choice(widget, i, &choice);

                if (is_fnumber && cam->supportedFNumbers &&
                    !g_list_find_custom(cam->supportedFNumbers, choice,
                                        (GCompareFunc)g_strcmp0))
                    continue;

                ENTANGLE_DEBUG("Add choice '%s'", choice);
                entangle_control_choice_add_entry(ENTANGLE_CONTROL_CHOICE(ctrl),
                                                  choice);
            }
            g_object_set(ctrl, "value", newValue, NULL);
        }
        g_free(oldValue);
    } break;

    case GP_WIDGET_DATE: {
        int value = 0;
        g_object_set(ctrl, "value", value, NULL);
    } break;

    case GP_WIDGET_RANGE: {
        float newValue = 0.0;
        float oldValue = 0.0;
        g_object_get(ctrl, "value", &oldValue, NULL);
        gp_widget_get_value(widget, &newValue);
        if (fabs(newValue - oldValue) > 0.0001) {
            ENTANGLE_DEBUG(
                "Updating value of range '%s' ('%s') old='%f' new='%f'",
                entangle_control_get_path(ctrl),
                entangle_control_get_label(ctrl), (double)oldValue,
                (double)newValue);
            g_object_set(ctrl, "value", (double)newValue, NULL);
        }
    } break;

    case GP_WIDGET_TEXT: {
        gchar *newValue = NULL;
        gchar *oldValue = NULL;
        g_object_get(ctrl, "value", &oldValue, NULL);
        gp_widget_get_value(widget, &newValue);
        if (!entangle_str_equal_null(newValue, oldValue)) {
            ENTANGLE_DEBUG(
                "Updating value of text '%s' ('%s') old='%s' new='%s'",
                entangle_control_get_path(ctrl),
                entangle_control_get_label(ctrl), oldValue, newValue);
            g_object_set(ctrl, "value", newValue, NULL);
        }
        g_free(oldValue);
    } break;

    case GP_WIDGET_TOGGLE: {
        int i;
        gboolean newValue = 0;
        gboolean oldValue = 0;
        g_object_get(ctrl, "value", &oldValue, NULL);
        gp_widget_get_value(widget, &i);
        newValue = i ? TRUE : FALSE;
        if (newValue != oldValue) {
            ENTANGLE_DEBUG(
                "Updating value of toggle '%s' ('%s') old='%d' new='%d'",
                entangle_control_get_path(ctrl),
                entangle_control_get_label(ctrl), oldValue, newValue);
            g_object_set(ctrl, "value", newValue, NULL);
        }
    } break;

    default:
        g_warn_if_reached();
        break;
    }

    entangle_control_set_dirty(ctrl, FALSE);
    ret = TRUE;
cleanup:
    g_free(fullpath);
    return ret;
}

static gboolean
do_save_controls(EntangleCamera *cam,
                 const char *path,
                 CameraWidget *widget,
                 gboolean *dirty,
                 GError **error)
{
    CameraWidgetType type;
    EntangleControl *ctrl = NULL;
    const char *name;
    char *fullpath;
    gboolean ret = FALSE;

    if (gp_widget_get_type(widget, &type) != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to fetch widget type"));
        return FALSE;
    }

    if (gp_widget_get_name(widget, &name) != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to fetch widget name"));
        return FALSE;
    }

    fullpath = g_strdup_printf("%s/%s", path, name);
    ctrl = g_hash_table_lookup(cam->controlPaths, fullpath);
    if (!ctrl) {
        ret = TRUE;
        goto cleanup;
    }

    switch (type) {
        /* We treat both window and section as just groups */
    case GP_WIDGET_WINDOW:
    case GP_WIDGET_SECTION:
        for (int i = 0; i < gp_widget_count_children(widget); i++) {
            CameraWidget *child;
            if (gp_widget_get_child(widget, i, &child) == GP_OK)
                if (!do_save_controls(cam, fullpath, child, dirty, error))
                    goto cleanup;
        }
        break;

    case GP_WIDGET_BUTTON: {
    } break;

        /* Unclear why these two are the same in libgphoto */
    case GP_WIDGET_RADIO:
    case GP_WIDGET_MENU:
        if (entangle_control_get_dirty(ctrl)) {
            char *value = NULL;
            g_object_get(ctrl, "value", &value, NULL);
            gp_widget_set_value(widget, value);
            g_free(value);
            *dirty = TRUE;
        }
        break;

    case GP_WIDGET_DATE:
        if (entangle_control_get_dirty(ctrl)) {
            int value = 0;
            g_object_get(ctrl, "value", &value, NULL);
            *dirty = TRUE;
        }
        break;

    case GP_WIDGET_RANGE:
        if (entangle_control_get_dirty(ctrl)) {
            float value = 0.0;
            g_object_get(ctrl, "value", &value, NULL);
            gp_widget_set_value(widget, &value);
            *dirty = TRUE;
        }
        break;

    case GP_WIDGET_TEXT:
        if (entangle_control_get_dirty(ctrl)) {
            char *value = NULL;
            g_object_get(ctrl, "value", &value, NULL);
            gp_widget_set_value(widget, value);
            g_free(value);
            *dirty = TRUE;
        }
        break;

    case GP_WIDGET_TOGGLE:
        if (entangle_control_get_dirty(ctrl)) {
            gboolean value = 0;
            int i;
            g_object_get(ctrl, "value", &value, NULL);
            i = value ? 1 : 0;
            gp_widget_set_value(widget, &i);
            *dirty = TRUE;
        }
        break;

    default:
        g_warn_if_reached();
        break;
    }

    ret = TRUE;
cleanup:
    g_free(fullpath);
    return ret;
}

/**
 * entangle_camera_load_controls:
 * @cam: (transfer none): the camera
 * @error: a GError, or NULL
 *
 * Loads the configuration controls from the camera.
 *
 * This can only be invoked when the camera is opened.
 *
 * This block execution of the caller until completion.
 *
 * Returns: TRUE if the controls were loaded, FALSE on error
 */
gboolean
entangle_camera_load_controls(EntangleCamera *cam, GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    gboolean ret = FALSE;
    int err;

    g_mutex_lock(&cam->lock);

    if (cam->cam == NULL) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to load controls, camera is not opened"));
        goto cleanup;
    }

    entangle_camera_begin_job(cam);
    ENTANGLE_DEBUG("Loading control values");
    err = gp_camera_get_config(cam->cam, &cam->widgets, cam->ctx);
    if (err != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to fetch camera control configuration: %s"),
                    cam->lastError);
        goto endjob;
    }

    if (cam->controls == NULL) {
        ENTANGLE_DEBUG("Building controls");
        cam->controlPaths =
            g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
        if (!(cam->controls = ENTANGLE_CONTROL_GROUP(
                  do_build_controls(cam, "", cam->widgets, error)))) {
            g_hash_table_unref(cam->controlPaths);
            cam->controlPaths = NULL;
            goto endjob;
        }

        if (entangle_camera_find_widget(cam, "/main/actions/viewfinder") ||
            entangle_camera_find_widget(cam, "/main/actions/eosviewfinder")) {
            ENTANGLE_DEBUG("Found a viewfinder widget");
            cam->hasViewfinder = TRUE;
        } else {
            ENTANGLE_DEBUG("No viewfinder widget");
            cam->hasViewfinder = FALSE;
        }

        if (entangle_camera_find_widget(cam,
                                        "/main/actions/autofocusdrive") ||
            entangle_camera_find_widget(cam, "/main/actions/autofocus")) {
            ENTANGLE_DEBUG("Found an autofocus widget");
            cam->hasAutofocus = TRUE;
        } else {
            ENTANGLE_DEBUG("No autofocus widget");
            cam->hasAutofocus = FALSE;
        }

        if (entangle_camera_find_widget(cam,
                                        "/main/actions/spotfocusarea")) {
            ENTANGLE_DEBUG("Found a spot focus area widget");
            cam->hasFocusArea = TRUE;
        } else {
            ENTANGLE_DEBUG("No spot focus area widget");
            cam->hasFocusArea = FALSE;
        }

        if (entangle_camera_find_widget(cam,
                                        "/main/actions/manualfocusdrive") ||
            entangle_camera_find_widget(cam, "/main/actions/manualfocus")) {
            ENTANGLE_DEBUG("Found a manual focus widget");
            cam->hasManualFocus = TRUE;
        } else {
            ENTANGLE_DEBUG("No manual focus widget");
            cam->hasManualFocus = FALSE;
        }
    }

    ret = do_load_controls(cam, "", cam->widgets, error);

endjob:
    entangle_camera_end_job(cam);

cleanup:
    g_mutex_unlock(&cam->lock);
    return ret;
}

static void
entangle_camera_load_controls_helper(GTask *task,
                                     gpointer object,
                                     gpointer task_data G_GNUC_UNUSED,
                                     GCancellable *cancellable G_GNUC_UNUSED)
{
    GError *error = NULL;

    if (!entangle_camera_load_controls(ENTANGLE_CAMERA(object), &error)) {
        g_task_return_error(task, error);
    } else {
        g_task_return_boolean(task, TRUE);
    }
}

/**
 * entangle_camera_load_controls_async:
 * @cam: (transfer none): the camera
 * @cancellable: (nullable): optional GCancellable object, NULL to ignore.
 * @callback: (scope async): a GAsyncReadyCallback to call when the request is
 * satisfied.
 * @user_data: the data to pass to callback function.
 *
 * Loads the configuration controls from the camera.
 *
 * This can only be invoked when the camera is opened.
 *
 * This will execute in the background, and invoke @callback
 * when complete, whereupon entangle_camera_load_controls_finish
 * can be used to check the status
 */
void
entangle_camera_load_controls_async(EntangleCamera *cam,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA(cam));

    GTask *task = g_task_new(cam, cancellable, callback, user_data);

    g_task_run_in_thread(task, entangle_camera_load_controls_helper);
    g_object_unref(task);
}

/**
 * entangle_camera_load_controls_finish:
 * @cam: (transfer none): the camera
 * @result: a GAsyncResult
 * @error: a GError, or NULL
 *
 * Check the completion status of a previous call to
 * entangle_camera_load_controls_async.
 *
 * Returns: TRUE if the controls were loaded, FALSE on error
 */
gboolean
entangle_camera_load_controls_finish(EntangleCamera *cam,
                                     GAsyncResult *result,
                                     GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    return g_task_propagate_boolean(G_TASK(result), error);
}

/**
 * entangle_camera_save_controls:
 * @cam: (transfer none): the camera
 * @error: a GError, or NULL
 *
 * Saves the configuration controls to the camera.
 *
 * This can only be invoked when the camera is opened.
 *
 * This block execution of the caller until completion.
 *
 * Returns: TRUE if the controls were saved, FALSE on error
 */
gboolean
entangle_camera_save_controls(EntangleCamera *cam, GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    gboolean ret = FALSE;
    gboolean dirty = FALSE;
    int err;

    g_mutex_lock(&cam->lock);

    if (cam->cam == NULL) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to save controls, camera is not opened"));
        goto cleanup;
    }

    if (cam->controls == NULL) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to save controls, camera is not configurable"));
        goto cleanup;
    }

    entangle_camera_begin_job(cam);

    ENTANGLE_DEBUG("Saving controls for %p", cam);

    if (!do_save_controls(cam, "", cam->widgets, &dirty, error))
        goto endjob;

    if (!dirty) {
        ENTANGLE_DEBUG("No widgets dirty, skipping");
        goto done;
    }

    if ((err = gp_camera_set_config(cam->cam, cam->widgets, cam->ctx)) !=
        GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to save camera control configuration: %s"),
                    cam->lastError);
        goto endjob;
    }

    if (!do_load_controls(cam, "", cam->widgets, error))
        goto endjob;

done:
    ret = TRUE;

endjob:
    entangle_camera_end_job(cam);

cleanup:
    g_mutex_unlock(&cam->lock);
    return ret;
}

static void
entangle_camera_save_controls_helper(GTask *task,
                                     gpointer object,
                                     gpointer task_data G_GNUC_UNUSED,
                                     GCancellable *cancellable G_GNUC_UNUSED)
{
    GError *error = NULL;

    if (!entangle_camera_save_controls(ENTANGLE_CAMERA(object), &error)) {
        g_task_return_error(task, error);
    } else {
        g_task_return_boolean(task, TRUE);
    }
}

/**
 * entangle_camera_save_controls_async:
 * @cam: (transfer none): the camera
 * @cancellable: (nullable): optional GCancellable object, NULL to ignore.
 * @callback: (scope async): a GAsyncReadyCallback to call when the request is
 * satisfied.
 * @user_data: the data to pass to callback function.
 *
 * Saves the configuration controls to the camera.
 *
 * This can only be invoked when the camera is opened.
 *
 * This will execute in the background, and invoke @callback
 * when complete, whereupon entangle_camera_save_controls_finish
 * can be used to check the status
 */
void
entangle_camera_save_controls_async(EntangleCamera *cam,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA(cam));

    GTask *task = g_task_new(cam, cancellable, callback, user_data);

    g_task_run_in_thread(task, entangle_camera_save_controls_helper);
    g_object_unref(task);
}

/**
 * entangle_camera_save_controls_finish:
 * @cam: (transfer none): the camera
 * @result: a GAsyncResult
 * @error: a GError, or NULL
 *
 * Check the completion status of a previous call to
 * entangle_camera_save_controls_async.
 *
 * Returns: TRUE if the controls were saved, FALSE on error
 */
gboolean
entangle_camera_save_controls_finish(EntangleCamera *cam,
                                     GAsyncResult *result,
                                     GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    return g_task_propagate_boolean(G_TASK(result), error);
}

/**
 * entangle_camera_get_controls:
 * @cam: (transfer none): the camera
 * @error: a GError, or NULL
 *
 * Get the configuration controls associated with the camera.
 *
 * This can only be invoked when the camera is opened.
 *
 * Returns: (transfer full): the controls, or NULL
 */
EntangleControlGroup *
entangle_camera_get_controls(EntangleCamera *cam, GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), NULL);

    EntangleControlGroup *ret = NULL;

    g_mutex_lock(&cam->lock);

    if (cam->cam == NULL) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Controls not available when camera is closed"));
        goto cleanup;
    }

    if (cam->controls == NULL) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Controls not available for this camera"));
        goto cleanup;
    }

    ret = cam->controls;
    g_object_ref(ret);

cleanup:
    g_mutex_unlock(&cam->lock);
    return ret;
}

/**
 * entangle_camera_set_viewfinder:
 * @cam: (transfer none): the camera
 * @enabled: TRUE to turn on the view finder
 * @error: a GError, or NULL
 *
 * If @enabled is TRUE, the view finder will be activated
 * allowing preview images to be captured. If @enabled is
 * FALSE, the view finder will be deactivated.
 *
 * This can only be invoked when the camera is opened.
 *
 * This block execution of the caller until completion.
 *
 * Returns: TRUE if the viewer finder state was changed, FALSE on error
 */
gboolean
entangle_camera_set_viewfinder(EntangleCamera *cam,
                               gboolean enabled,
                               GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    gboolean ret = FALSE;
    CameraWidget *widget;
    CameraWidgetType type;
    int value;
    int err;

    g_mutex_lock(&cam->lock);
    entangle_camera_begin_job(cam);

    ENTANGLE_DEBUG("Setting viewfinder state %d", enabled);

    if (cam->cam == NULL) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Controls not available when camera is closed"));
        goto cleanup;
    }

    if (cam->widgets == NULL) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Controls not available for this camera"));
        goto cleanup;
    }

    widget = entangle_camera_find_widget(cam, "/main/actions/viewfinder");
    if (!widget)
        widget =
            entangle_camera_find_widget(cam, "/main/actions/eosviewfinder");
    if (!widget) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Viewfinder control not available with this camera"));
        goto cleanup;
    }

    if ((err = gp_widget_get_type(widget, &type)) != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to fetch widget type"));
        goto cleanup;
    }

    if (type != GP_WIDGET_TOGGLE) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Viewfinder control was not a toggle widget"));
        goto cleanup;
    }

    /*
     * Sigh. gphoto attempts to detect when the value has
     * changed and thus often doesn't apply the change in
     * value we request. We set the value twice, first to
     * the wrong value, then the right value. This ensures
     * gphoto always sees the value as changed.
     */
    value = enabled ? 0 : 1;
    if ((err = gp_widget_set_value(widget, &value)) != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Failed to set viewfinder state: %s %d"),
                    gp_port_result_as_string(err), err);
        goto cleanup;
    }
    value = enabled ? 1 : 0;
    if ((err = gp_widget_set_value(widget, &value)) != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Failed to set viewfinder state: %s %d"),
                    gp_port_result_as_string(err), err);
        goto cleanup;
    }

    if ((err = gp_camera_set_config(cam->cam, cam->widgets, cam->ctx)) !=
        GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to save camera control configuration: %s"),
                    cam->lastError);
        goto cleanup;
    }

    ret = TRUE;

cleanup:
    entangle_camera_end_job(cam);
    g_mutex_unlock(&cam->lock);
    return ret;
}

static void
entangle_camera_set_viewfinder_helper(GTask *task,
                                      gpointer object,
                                      gpointer task_data,
                                      GCancellable *cancellable G_GNUC_UNUSED)
{
    GError *error = NULL;
    gboolean enabled;

    enabled = GPOINTER_TO_INT(task_data) == 1 ? TRUE : FALSE;

    if (!entangle_camera_set_viewfinder(ENTANGLE_CAMERA(object), enabled,
                                        &error)) {
        g_task_return_error(task, error);
    } else {
        g_task_return_boolean(task, TRUE);
    }
}

/**
 * entangle_camera_set_viewfinder_async:
 * @cam: (transfer none): the camera
 * @enabled: TRUE to turn on the view finder
 * @cancellable: (nullable): optional GCancellable object, NULL to ignore.
 * @callback: (scope async): a GAsyncReadyCallback to call when the request is
 * satisfied.
 * @user_data: the data to pass to callback function.
 *
 * If @enabled is TRUE, the view finder will be activated
 * allowing preview images to be captured. If @enabled is
 * FALSE, the view finder will be deactivated.
 *
 * This can only be invoked when the camera is opened.
 *
 * This will execute in the background, and invoke @callback
 * when complete, whereupon entangle_camera_set_viewfinder_finish
 * can be used to check the status
 */
void
entangle_camera_set_viewfinder_async(EntangleCamera *cam,
                                     gboolean enabled,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA(cam));

    GTask *task = g_task_new(cam, cancellable, callback, user_data);

    g_task_set_task_data(task, GINT_TO_POINTER(enabled ? 1 : 0), NULL);

    g_task_run_in_thread(task, entangle_camera_set_viewfinder_helper);
    g_object_unref(task);
}

/**
 * entangle_camera_set_viewfinder_finish:
 * @cam: (transfer none): the camera
 * @result: a GAsyncResult
 * @error: a GError, or NULL
 *
 * Check the completion status of a previous call to
 * entangle_camera_set_viewfinder_async.
 *
 * Returns: TRUE if the viewfinder state was changed, FALSE on error
 */
gboolean
entangle_camera_set_viewfinder_finish(EntangleCamera *cam,
                                      GAsyncResult *result,
                                      GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    return g_task_propagate_boolean(G_TASK(result), error);
}

static gchar *
entangle_camera_get_focus_indication(EntangleCamera *cam)
{
    CameraWidget *widget = NULL;
    gchar *value = NULL;
    gchar *ret = NULL;

    if (gp_camera_get_single_config(cam->cam, "focusindication", &widget,
                                    cam->ctx) != GP_OK)
        return NULL;

    if (gp_widget_get_value(widget, &value) == GP_OK && value)
        ret = g_strdup(value);

    gp_widget_free(widget);
    return ret;
}

/**
 * entangle_camera_autofocus:
 * @cam: (transfer none): the camera
 * @error: a GError, or NULL
 *
 * Trigger the autofocus mechanism on the camera, waiting
 * until focus is achieved or fails.
 *
 * This can only be invoked when the camera is opened.
 *
 * This block execution of the caller until completion.
 *
 * Returns: TRUE if autofocus was achieved, FALSE on error
 */
gboolean
entangle_camera_autofocus(EntangleCamera *cam, GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    gboolean ret = FALSE;
    gboolean sonyHalfPress = FALSE;
    CameraWidget *widget = NULL;
    CameraWidgetType type;
    int value;
    int err;

    g_mutex_lock(&cam->lock);
    entangle_camera_begin_job(cam);

    ENTANGLE_DEBUG("Setting autofocus");

    if (cam->cam == NULL) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Controls not available when camera is closed"));
        goto cleanup;
    }

    if (cam->widgets == NULL) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Controls not available for this camera"));
        goto cleanup;
    }

    widget = entangle_camera_find_widget(cam, "/main/actions/autofocusdrive");
    if (!widget) {
        /* Sony exposes the shutter half-press as /main/actions/autofocus
         * rather than the Nikon/Canon/Fuji /main/actions/autofocusdrive. */
        widget = entangle_camera_find_widget(cam, "/main/actions/autofocus");
        sonyHalfPress = widget != NULL;
    }

    if (!widget) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Autofocus control not available with this camera"));
        goto cleanup;
    }

    if ((err = gp_widget_get_type(widget, &type)) != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to fetch widget type"));
        goto cleanup;
    }

    if (type != GP_WIDGET_TOGGLE) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Autofocus control was not a toggle widget"));
        goto cleanup;
    }

    if (sonyHalfPress) {
        gchar *initial = entangle_camera_get_focus_indication(cam);

        /* Press the shutter button half way and hold it while the camera
         * performs autofocus. */
        value = 1;
        if ((err = gp_widget_set_value(widget, &value)) != GP_OK) {
            g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                        _("Failed to set autofocus state: %s %d"),
                        gp_port_result_as_string(err), err);
            g_free(initial);
            goto cleanup;
        }

        if ((err = gp_camera_set_config(cam->cam, cam->widgets, cam->ctx)) !=
            GP_OK) {
            g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                        _("Unable to save camera control configuration: %s"),
                        cam->lastError);
            g_free(initial);
            goto cleanup;
        }

        /* Wait for focus to settle, but do not hold the button forever.
         * The focus indication value changes from "Unlock" to either
         * "Focus Locked" or "No Focus" once the camera is done. */
        gint64 deadline = g_get_monotonic_time() + 2 * G_TIME_SPAN_SECOND;
        while (g_get_monotonic_time() < deadline) {
            gchar *current = entangle_camera_get_focus_indication(cam);
            gboolean changed =
                current && (!initial || strcmp(current, initial) != 0);

            g_free(current);
            if (changed)
                break;

            g_usleep(100 * 1000);
        }
        g_free(initial);

        /* Release the shutter button. AF-S keeps the achieved focus. */
        value = 0;
        if ((err = gp_widget_set_value(widget, &value)) != GP_OK) {
            g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                        _("Failed to set autofocus state: %s %d"),
                        gp_port_result_as_string(err), err);
            goto cleanup;
        }

        if ((err = gp_camera_set_config(cam->cam, cam->widgets, cam->ctx)) !=
            GP_OK) {
            g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                        _("Unable to save camera control configuration: %s"),
                        cam->lastError);
            goto cleanup;
        }
    } else {
        value = 0;
        if ((err = gp_widget_set_value(widget, &value)) != GP_OK) {
            g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                        _("Failed to set autofocus state: %s %d"),
                        gp_port_result_as_string(err), err);
            goto cleanup;
        }

        value = 1;
        if ((err = gp_widget_set_value(widget, &value)) != GP_OK) {
            g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                        _("Failed to set autofocus state: %s %d"),
                        gp_port_result_as_string(err), err);
            goto cleanup;
        }

        if ((err = gp_camera_set_config(cam->cam, cam->widgets, cam->ctx)) !=
            GP_OK) {
            g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                        _("Unable to save camera control configuration: %s"),
                        cam->lastError);
            goto cleanup;
        }
    }

    ret = TRUE;

cleanup:
    entangle_camera_end_job(cam);
    g_mutex_unlock(&cam->lock);
    return ret;
}

static void
entangle_camera_autofocus_helper(GTask *task,
                                 gpointer object,
                                 gpointer task_data G_GNUC_UNUSED,
                                 GCancellable *cancellable G_GNUC_UNUSED)
{
    GError *error = NULL;

    if (!entangle_camera_autofocus(ENTANGLE_CAMERA(object), &error)) {
        g_task_return_error(task, error);
    } else {
        g_task_return_boolean(task, TRUE);
    }
}

/**
 * entangle_camera_autofocus_async:
 * @cam: (transfer none): the camera
 * @cancellable: (nullable): optional GCancellable object, NULL to ignore.
 * @callback: (scope async): a GAsyncReadyCallback to call when the request is
 * satisfied.
 * @user_data: the data to pass to callback function.
 *
 * Trigger the autofocus mechanism on the camera, waiting
 * until focus is achieved or fails.
 *
 * This can only be invoked when the camera is opened.
 *
 * This will execute in the background, and invoke @callback
 * when complete, whereupon entangle_camera_autofocus_finish
 * can be used to check the status
 */
void
entangle_camera_autofocus_async(EntangleCamera *cam,
                                GCancellable *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer user_data)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA(cam));

    GTask *task = g_task_new(cam, cancellable, callback, user_data);

    g_task_run_in_thread(task, entangle_camera_autofocus_helper);
    g_object_unref(task);
}

/**
 * entangle_camera_autofocus_finish:
 * @cam: (transfer none): the camera
 * @result: a GAsyncResult
 * @error: a GError, or NULL
 *
 * Check the completion status of a previous call to
 * entangle_camera_autofocus_async.
 *
 * Returns: TRUE if autofocus was performed, FALSE on error
 */
gboolean
entangle_camera_autofocus_finish(EntangleCamera *cam,
                                 GAsyncResult *result,
                                 GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    return g_task_propagate_boolean(G_TASK(result), error);
}

static gboolean
entangle_camera_focus_area_is_spot(const gchar *value)
{
    if (!value)
        return FALSE;

    return strstr(value, "Flexible Spot") != NULL ||
           strstr(value, "Expand Flexible") != NULL ||
           strstr(value, "Flexible") != NULL || strstr(value, "Гибк") != NULL;
}

static const gchar *
entangle_camera_focus_area_spot_choice(CameraWidget *widget)
{
    CameraWidgetType type;
    const gchar *best = NULL;
    int count = 0;
    int i;

    if (gp_widget_get_type(widget, &type) != GP_OK || type != GP_WIDGET_RADIO)
        return NULL;

    count = gp_widget_count_choices(widget);
    for (i = 0; i < count; i++) {
        const gchar *choice = NULL;

        if (gp_widget_get_choice(widget, i, &choice) != GP_OK || !choice)
            continue;

        if (strstr(choice, "Flexible Spot: M") != NULL ||
            strstr(choice, "Flexible Spot") != NULL)
            return choice;

        if (!best && entangle_camera_focus_area_is_spot(choice))
            best = choice;
    }

    return best;
}

/**
 * entangle_camera_set_focus_area:
 * @cam: (transfer none): the camera
 * @x: horizontal focus point position, in the camera's 640x480 grid
 * @y: vertical focus point position, in the camera's 640x480 grid
 * @error: a GError, or NULL
 *
 * Move the camera focus point to the given position and trigger
 * autofocus there.
 *
 * This can only be invoked when the camera is opened.
 *
 * This blocks execution of the caller until completion.
 *
 * Returns: TRUE if the focus point was set and autofocus triggered,
 *          FALSE on error
 */
gboolean
entangle_camera_set_focus_area(EntangleCamera *cam,
                               gint x,
                               gint y,
                               GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    gboolean ret = FALSE;
    CameraWidget *widget = NULL;
    CameraWidget *focusArea = NULL;
    CameraWidgetType type;
    gchar text[32];
    int err;

    g_mutex_lock(&cam->lock);
    entangle_camera_begin_job(cam);

    if (cam->cam == NULL) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Controls not available when camera is closed"));
        goto cleanup;
    }

    if (cam->widgets == NULL) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Controls not available for this camera"));
        goto cleanup;
    }

    widget = entangle_camera_find_widget(cam, "/main/actions/spotfocusarea");
    if (!widget) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Focus area control not available with this camera"));
        goto cleanup;
    }

    if ((err = gp_widget_get_type(widget, &type)) != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to fetch widget type"));
        goto cleanup;
    }

    if (type != GP_WIDGET_TEXT) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Focus area control was not a text widget"));
        goto cleanup;
    }

    x = CLAMP(x, 0, 639);
    y = CLAMP(y, 0, 479);

    /* Sony only honours the focus position in one of the Flexible Spot
     * modes. Switch out of Wide/Zone/Center if necessary. */
    focusArea = entangle_camera_find_widget(cam, "/main/settings/focusarea");
    if (!focusArea)
        focusArea =
            entangle_camera_find_widget(cam, "/main/capturesettings/focusarea");
    if (focusArea) {
        CameraWidgetType areaType;

        if (gp_widget_get_type(focusArea, &areaType) == GP_OK &&
            areaType == GP_WIDGET_RADIO) {
            gchar *current = NULL;

            gp_widget_get_value(focusArea, &current);
            if (!entangle_camera_focus_area_is_spot(current)) {
                const gchar *spot =
                    entangle_camera_focus_area_spot_choice(focusArea);

                if (spot) {
                    gp_widget_set_value(focusArea, spot);
                    if ((err = gp_camera_set_config(cam->cam, cam->widgets,
                                                    cam->ctx)) != GP_OK) {
                        g_set_error(
                            error, ENTANGLE_CAMERA_ERROR, 0,
                            _("Unable to save camera control configuration: %s"),
                            cam->lastError);
                        goto cleanup;
                    }
                }
            }
        }
    }

    g_snprintf(text, sizeof(text), "%d,%d", x, y);
    if ((err = gp_widget_set_value(widget, text)) != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Failed to set focus area: %s %d"),
                    gp_port_result_as_string(err), err);
        goto cleanup;
    }

    if ((err = gp_camera_set_config(cam->cam, cam->widgets, cam->ctx)) !=
        GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to save camera control configuration: %s"),
                    cam->lastError);
        goto cleanup;
    }

    ret = TRUE;

cleanup:
    entangle_camera_end_job(cam);
    g_mutex_unlock(&cam->lock);

    if (ret)
        ret = entangle_camera_autofocus(cam, error);

    return ret;
}

static void
entangle_camera_set_focus_area_helper(GTask *task,
                                      gpointer object,
                                      gpointer task_data,
                                      GCancellable *cancellable
                                          G_GNUC_UNUSED)
{
    gint *coords = task_data;
    GError *error = NULL;

    if (!entangle_camera_set_focus_area(ENTANGLE_CAMERA(object), coords[0],
                                        coords[1], &error)) {
        g_task_return_error(task, error);
    } else {
        g_task_return_boolean(task, TRUE);
    }
}

/**
 * entangle_camera_set_focus_area_async:
 * @cam: (transfer none): the camera
 * @x: horizontal focus point position, in the camera's 640x480 grid
 * @y: vertical focus point position, in the camera's 640x480 grid
 * @cancellable: (nullable): optional GCancellable object, NULL to ignore.
 * @callback: (scope async): a GAsyncReadyCallback to call when the request is
 * satisfied.
 * @user_data: the data to pass to callback function.
 *
 * Move the camera focus point to the given position and trigger
 * autofocus there.
 *
 * This will execute in the background, and invoke @callback when complete,
 * whereupon entangle_camera_set_focus_area_finish can be used to check
 * the status.
 */
void
entangle_camera_set_focus_area_async(EntangleCamera *cam,
                                     gint x,
                                     gint y,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA(cam));

    GTask *task = g_task_new(cam, cancellable, callback, user_data);
    gint *coords = g_new(gint, 2);

    coords[0] = x;
    coords[1] = y;
    g_task_set_task_data(task, coords, g_free);

    g_task_run_in_thread(task, entangle_camera_set_focus_area_helper);
    g_object_unref(task);
}

/**
 * entangle_camera_set_focus_area_finish:
 * @cam: (transfer none): the camera
 * @result: a GAsyncResult
 * @error: a GError, or NULL
 *
 * Check the completion status of a previous call to
 * entangle_camera_set_focus_area_async.
 *
 * Returns: TRUE if the focus area was set and autofocus triggered,
 *          FALSE on error
 */
gboolean
entangle_camera_set_focus_area_finish(EntangleCamera *cam,
                                      GAsyncResult *result,
                                      GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    return g_task_propagate_boolean(G_TASK(result), error);
}

/**
 * entangle_camera_manualfocus:
 * @cam: (transfer none): the camera
 * @step: how much to change focus by
 * @error: a GError, or NULL
 *
 * Trigger the focus mechanism on the camera, to move
 * by @step.
 *
 * This can only be invoked when the camera is opened.
 *
 * This block execution of the caller until completion.
 *
 * Returns: TRUE if autofocus was achieved, FALSE on error
 */
gboolean
entangle_camera_manualfocus(EntangleCamera *cam,
                            EntangleCameraManualFocusStep step,
                            GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    gboolean ret = FALSE;
    gboolean sony = FALSE;
    CameraWidget *widget;
    CameraWidgetType type;
    int err;

    g_mutex_lock(&cam->lock);
    entangle_camera_begin_job(cam);

    ENTANGLE_DEBUG("Setting manualfocus %d", (int)step);

    if (cam->cam == NULL) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Controls not available when camera is closed"));
        goto cleanup;
    }

    if (cam->widgets == NULL) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Controls not available for this camera"));
        goto cleanup;
    }

    widget = entangle_camera_find_widget(cam, "/main/actions/manualfocusdrive");
    if (!widget) {
        widget = entangle_camera_find_widget(cam, "/main/actions/manualfocus");
        sony = widget != NULL;
    }
    if (!widget) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Manual focus control not available with this camera"));
        goto cleanup;
    }

    if ((err = gp_widget_get_type(widget, &type)) != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to fetch widget type"));
        goto cleanup;
    }

    if (type != GP_WIDGET_RANGE && type != GP_WIDGET_RADIO) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Manual focus control was not a range or radio widget"));
        goto cleanup;
    }

    if (type == GP_WIDGET_RANGE) {
        /* Nikon */
        float values[] = {
            [ENTANGLE_CAMERA_MANUAL_FOCUS_STEP_IN_COARSE] = sony ? 7 : 512,
            [ENTANGLE_CAMERA_MANUAL_FOCUS_STEP_IN_MEDIUM] = sony ? 3 : 64,
            [ENTANGLE_CAMERA_MANUAL_FOCUS_STEP_IN_FINE] = sony ? 1 : 8,
            [ENTANGLE_CAMERA_MANUAL_FOCUS_STEP_OUT_COARSE] = sony ? -7 : -512,
            [ENTANGLE_CAMERA_MANUAL_FOCUS_STEP_OUT_MEDIUM] = sony ? -3 : -64,
            [ENTANGLE_CAMERA_MANUAL_FOCUS_STEP_OUT_FINE] = sony ? -1 : -8,
        };

        float value = values[step];
        ENTANGLE_DEBUG("Setting manualfocus range %d", (int)value);
        if ((err = gp_widget_set_value(widget, &value)) != GP_OK) {
            g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                        _("Failed to set manual focus state: %s %d"),
                        gp_port_result_as_string(err), err);
            goto cleanup;
        }
    } else {
        /* Canon */
        int idx[] = {
            [ENTANGLE_CAMERA_MANUAL_FOCUS_STEP_IN_COARSE] = 2,
            [ENTANGLE_CAMERA_MANUAL_FOCUS_STEP_IN_MEDIUM] = 1,
            [ENTANGLE_CAMERA_MANUAL_FOCUS_STEP_IN_FINE] = 0,
            [ENTANGLE_CAMERA_MANUAL_FOCUS_STEP_OUT_COARSE] = 6,
            [ENTANGLE_CAMERA_MANUAL_FOCUS_STEP_OUT_MEDIUM] = 5,
            [ENTANGLE_CAMERA_MANUAL_FOCUS_STEP_OUT_FINE] = 4,
        };
        const gchar *value;
        if ((err = gp_widget_get_choice(widget, idx[step], &value)) != GP_OK) {
            g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                        _("Failed to read manual focus choice %d: %s %d"), step,
                        gp_port_result_as_string(err), err);
            goto cleanup;
        }
        ENTANGLE_DEBUG("Setting manualfocus radio %s", value);
        if ((err = gp_widget_set_value(widget, value)) != GP_OK) {
            g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                        _("Failed to set manual focus state: %s %d"),
                        gp_port_result_as_string(err), err);
            goto cleanup;
        }
    }

    if ((err = gp_camera_set_config(cam->cam, cam->widgets, cam->ctx)) !=
        GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to save camera control configuration: %s"),
                    cam->lastError);
        goto cleanup;
    }

    if (type == GP_WIDGET_RANGE) {
        float value = 0;
        if ((err = gp_widget_set_value(widget, &value)) != GP_OK) {
            g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                        _("Failed to set manual focus state: %s %d"),
                        gp_port_result_as_string(err), err);
            goto cleanup;
        }
    } else {
        const gchar *value;
        /* Reset drive to "None" == 3 */
        if ((err = gp_widget_get_choice(widget, 3, &value)) != GP_OK) {
            g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                        _("Failed to read manual focus choice %d: %s %d"), step,
                        gp_port_result_as_string(err), err);
            goto cleanup;
        }
        ENTANGLE_DEBUG("Setting manualfocus radio %s", value);
        if ((err = gp_widget_set_value(widget, value)) != GP_OK) {
            g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                        _("Failed to set manual focus state: %s %d"),
                        gp_port_result_as_string(err), err);
            goto cleanup;
        }
    }

    ret = TRUE;

cleanup:
    entangle_camera_end_job(cam);
    g_mutex_unlock(&cam->lock);
    return ret;
}

static void
entangle_camera_manualfocus_helper(GTask *task,
                                   gpointer object,
                                   gpointer task_data,
                                   GCancellable *cancellable G_GNUC_UNUSED)
{
    GError *error = NULL;

    if (!entangle_camera_manualfocus(ENTANGLE_CAMERA(object),
                                     GPOINTER_TO_INT(task_data), &error)) {
        g_task_return_error(task, error);
    } else {
        g_task_return_boolean(task, TRUE);
    }
}

/**
 * entangle_camera_manualfocus_async:
 * @cam: (transfer none): the camera
 * @step: how much to change focus by
 * @cancellable: (nullable): optional GCancellable object, NULL to ignore.
 * @callback: (scope async): a GAsyncReadyCallback to call when the request is
 * satisfied.
 * @user_data: the data to pass to callback function.
 *
 * Trigger the focus mechanism on the camera, to move
 * by @step.
 *
 * This can only be invoked when the camera is opened.
 *
 * This will execute in the background, and invoke @callback
 * when complete, whereupon entangle_camera_manualfocus_finish
 * can be used to check the status
 */
void
entangle_camera_manualfocus_async(EntangleCamera *cam,
                                  EntangleCameraManualFocusStep step,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA(cam));

    GTask *task = g_task_new(cam, cancellable, callback, user_data);

    g_task_set_task_data(task, GINT_TO_POINTER(step), NULL);

    g_task_run_in_thread(task, entangle_camera_manualfocus_helper);
    g_object_unref(task);
}

/**
 * entangle_camera_manualfocus_finish:
 * @cam: (transfer none): the camera
 * @result: a GAsyncResult
 * @error: a GError, or NULL
 *
 * Check the completion status of a previous call to
 * entangle_camera_manualfocus_async.
 *
 * Returns: TRUE if manual focus was performed, FALSE on error
 */
gboolean
entangle_camera_manualfocus_finish(EntangleCamera *cam,
                                   GAsyncResult *result,
                                   GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    return g_task_propagate_boolean(G_TASK(result), error);
}

/**
 * entangle_camera_manualfocus_value:
 * @cam: (transfer none): the camera
 * @value: focus step in the camera's native range (Sony: -7..7)
 * @error: a GError, or NULL
 *
 * Move the lens focus by a relative number of steps.
 *
 * This can only be invoked when the camera is opened.
 *
 * Returns: TRUE if the focus step was applied, FALSE on error
 */
gboolean
entangle_camera_manualfocus_value(EntangleCamera *cam,
                                  gint value,
                                  GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    gboolean ret = FALSE;
    CameraWidget *widget;
    CameraWidgetType type;
    float fvalue;
    int err;

    g_mutex_lock(&cam->lock);
    entangle_camera_begin_job(cam);

    if (cam->cam == NULL) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Controls not available when camera is closed"));
        goto cleanup;
    }

    if (cam->widgets == NULL) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Controls not available for this camera"));
        goto cleanup;
    }

    widget = entangle_camera_find_widget(cam, "/main/actions/manualfocus");
    if (!widget) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Manual focus control not available with this camera"));
        goto cleanup;
    }

    if ((err = gp_widget_get_type(widget, &type)) != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to fetch widget type"));
        goto cleanup;
    }

    if (type != GP_WIDGET_RANGE) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Manual focus control was not a range widget"));
        goto cleanup;
    }

    value = CLAMP(value, -7, 7);
    if (value == 0) {
        ret = TRUE;
        goto cleanup;
    }

    fvalue = value;
    if ((err = gp_widget_set_value(widget, &fvalue)) != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Failed to set manual focus state: %s %d"),
                    gp_port_result_as_string(err), err);
        goto cleanup;
    }

    if ((err = gp_camera_set_config(cam->cam, cam->widgets, cam->ctx)) !=
        GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to save camera control configuration: %s"),
                    cam->lastError);
        goto cleanup;
    }

    /* Keep the widget reusable for the next relative move, but do not
     * send the zero value to the camera. */
    fvalue = 0;
    gp_widget_set_value(widget, &fvalue);
    gp_widget_set_changed(widget, FALSE);

    ret = TRUE;

cleanup:
    entangle_camera_end_job(cam);
    g_mutex_unlock(&cam->lock);
    return ret;
}

static void
entangle_camera_manualfocus_value_helper(GTask *task,
                                         gpointer object,
                                         gpointer task_data,
                                         GCancellable *cancellable
                                             G_GNUC_UNUSED)
{
    gint value = GPOINTER_TO_INT(task_data);
    GError *error = NULL;

    if (!entangle_camera_manualfocus_value(ENTANGLE_CAMERA(object), value,
                                           &error)) {
        g_task_return_error(task, error);
    } else {
        g_task_return_boolean(task, TRUE);
    }
}

/**
 * entangle_camera_manualfocus_value_async:
 * @cam: (transfer none): the camera
 * @value: focus step in the camera's native range (Sony: -7..7)
 * @cancellable: (nullable): optional GCancellable object, NULL to ignore.
 * @callback: (scope async): a GAsyncReadyCallback to call when the request is
 * satisfied.
 * @user_data: the data to pass to callback function.
 *
 * Move the lens focus by a relative number of steps.
 *
 * This will execute in the background, and invoke @callback when complete,
 * whereupon entangle_camera_manualfocus_value_finish can be used to check
 * the status.
 */
void
entangle_camera_manualfocus_value_async(EntangleCamera *cam,
                                        gint value,
                                        GCancellable *cancellable,
                                        GAsyncReadyCallback callback,
                                        gpointer user_data)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA(cam));

    GTask *task = g_task_new(cam, cancellable, callback, user_data);

    g_task_set_task_data(task, GINT_TO_POINTER(value), NULL);
    g_task_run_in_thread(task, entangle_camera_manualfocus_value_helper);
    g_object_unref(task);
}

/**
 * entangle_camera_manualfocus_value_finish:
 * @cam: (transfer none): the camera
 * @result: a GAsyncResult
 * @error: a GError, or NULL
 *
 * Check the completion status of a previous call to
 * entangle_camera_manualfocus_value_async.
 *
 * Returns: TRUE if the focus step was applied, FALSE on error
 */
gboolean
entangle_camera_manualfocus_value_finish(EntangleCamera *cam,
                                         GAsyncResult *result,
                                         GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    return g_task_propagate_boolean(G_TASK(result), error);
}

/**
 * entangle_camera_set_clock:
 * @cam: (transfer none): the camera
 * @epochsecs: new time in seconds since the epoch
 * @error: a GError, or NULL
 *
 * Update the camera clock to be @epochsecs seconds since
 * the epoch.
 *
 * This can only be invoked when the camera is opened.
 *
 * This block execution of the caller until completion.
 *
 * Returns: TRUE if the clock was changed, FALSE on error
 */
gboolean
entangle_camera_set_clock(EntangleCamera *cam, gint64 epochsecs, GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    gboolean ret = FALSE;
    CameraWidget *widget;
    CameraWidgetType type;
    time_t value;
    int err;

    g_mutex_lock(&cam->lock);
    entangle_camera_begin_job(cam);

    ENTANGLE_DEBUG("Setting clock to %lld", (long long)epochsecs);

    if (cam->cam == NULL) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Controls not available when camera is closed"));
        goto cleanup;
    }

    if (cam->widgets == NULL) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Controls not available for this camera"));
        goto cleanup;
    }

    widget = entangle_camera_find_widget(cam, "/main/settings/datetime");
    if (!widget) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Time setting not available with this camera"));
        goto cleanup;
    }

    if ((err = gp_widget_get_type(widget, &type)) != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to fetch widget type"));
        goto cleanup;
    }

    if (type != GP_WIDGET_DATE) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Time setting was not a date widget"));
        goto cleanup;
    }

    value = epochsecs;
    if ((err = gp_widget_set_value(widget, &value)) != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Failed to set time state: %s %d"),
                    gp_port_result_as_string(err), err);
        goto cleanup;
    }

    if ((err = gp_camera_set_config(cam->cam, cam->widgets, cam->ctx)) !=
        GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to save camera control configuration: %s"),
                    cam->lastError);
        goto cleanup;
    }

    ret = TRUE;

cleanup:
    entangle_camera_end_job(cam);
    g_mutex_unlock(&cam->lock);
    return ret;
}

static void
entangle_camera_set_clock_helper(GTask *task,
                                 gpointer object,
                                 gpointer task_data,
                                 GCancellable *cancellable G_GNUC_UNUSED)
{
    GError *error = NULL;
    guint64 *epochsecsptr = task_data;

    if (!entangle_camera_set_clock(ENTANGLE_CAMERA(object), *epochsecsptr,
                                   &error)) {
        g_task_return_error(task, error);
    } else {
        g_task_return_boolean(task, TRUE);
    }
}

/**
 * entangle_camera_set_clock_async:
 * @cam: (transfer none): the camera
 * @epochsecs: new time in seconds since the epoch
 * @cancellable: (nullable): optional GCancellable object, NULL to ignore.
 * @callback: (scope async): a GAsyncReadyCallback to call when the request is
 * satisfied.
 * @user_data: the data to pass to callback function.
 *
 * Update the camera clock to be @epochsecs seconds since
 * the epoch.
 *
 * This can only be invoked when the camera is opened.
 *
 * This will execute in the background, and invoke @callback
 * when complete, whereupon entangle_camera_set_clock_finish
 * can be used to check the status
 */
void
entangle_camera_set_clock_async(EntangleCamera *cam,
                                gint64 epochsecs,
                                GCancellable *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer user_data)
{
    guint64 *epochsecsptr;
    g_return_if_fail(ENTANGLE_IS_CAMERA(cam));

    GTask *task = g_task_new(cam, cancellable, callback, user_data);

    epochsecsptr = g_new0(guint64, 1);
    *epochsecsptr = epochsecs;

    g_task_set_task_data(task, epochsecsptr, g_free);

    g_task_run_in_thread(task, entangle_camera_set_clock_helper);
    g_object_unref(task);
}

/**
 * entangle_camera_set_clock_finish:
 * @cam: (transfer none): the camera
 * @result: a GAsyncResult
 * @error: a GError, or NULL
 *
 * Check the completion status of a previous call to
 * entangle_camera_set_clock_async.
 *
 * Returns: TRUE if the clock was changed, FALSE on error
 */
gboolean
entangle_camera_set_clock_finish(EntangleCamera *cam,
                                 GAsyncResult *result,
                                 GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    return g_task_propagate_boolean(G_TASK(result), error);
}

/**
 * entangle_camera_set_capture_target:
 * @cam: (transfer none): the camera
 * @target: the capture target
 * @error: a GError, or NULL
 *
 * Set the destination for storing captured images
 * to @target.
 *
 * This can only be invoked when the camera is opened.
 *
 * This block execution of the caller until completion.
 *
 * Returns: TRUE if the capture target was changed, FALSE on error
 */
gboolean
entangle_camera_set_capture_target(EntangleCamera *cam,
                                   EntangleCameraCaptureTarget target,
                                   GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    gboolean ret = FALSE;
    CameraWidget *widget;
    CameraWidgetType type;
    const gchar *value;
    GEnumValue *evalue;
    GEnumClass *eclass = NULL;
    int err;

    g_mutex_lock(&cam->lock);
    entangle_camera_begin_job(cam);

    ENTANGLE_DEBUG("Setting clock to %d", target);

    if (cam->cam == NULL) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Controls not available when camera is closed"));
        goto cleanup;
    }

    if (cam->widgets == NULL) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Controls not available for this camera"));
        goto cleanup;
    }

    widget = entangle_camera_find_widget(cam, "/main/settings/capturetarget");
    if (!widget) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Capture target setting not available with this camera"));
        goto cleanup;
    }

    if ((err = gp_widget_get_type(widget, &type)) != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to fetch widget type"));
        goto cleanup;
    }

    if (type != GP_WIDGET_MENU) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Time setting was not a choice widget"));
        goto cleanup;
    }

    eclass = g_type_class_ref(ENTANGLE_TYPE_CAMERA_CAPTURE_TARGET);
    evalue = g_enum_get_value(eclass, target);
    value = evalue->value_nick;
    if ((err = gp_widget_set_value(widget, value)) != GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Failed to set capture target: %s %d"),
                    gp_port_result_as_string(err), err);
        goto cleanup;
    }

    if ((err = gp_camera_set_config(cam->cam, cam->widgets, cam->ctx)) !=
        GP_OK) {
        g_set_error(error, ENTANGLE_CAMERA_ERROR, 0,
                    _("Unable to save camera control configuration: %s"),
                    cam->lastError);
        goto cleanup;
    }

    ret = TRUE;

cleanup:
    if (eclass)
        g_type_class_unref(eclass);
    entangle_camera_end_job(cam);
    g_mutex_unlock(&cam->lock);
    return ret;
}

static void
entangle_camera_set_capture_target_helper(GTask *task,
                                          gpointer object,
                                          gpointer task_data,
                                          GCancellable *cancellable
                                              G_GNUC_UNUSED)
{
    GError *error = NULL;
    EntangleCameraCaptureTarget *targetptr = task_data;

    if (!entangle_camera_set_capture_target(ENTANGLE_CAMERA(object), *targetptr,
                                            &error)) {
        g_task_return_error(task, error);
    } else {
        g_task_return_boolean(task, TRUE);
    }
}

/**
 * entangle_camera_set_capture_target_async:
 * @cam: (transfer none): the camera
 * @target: the capture target
 * @cancellable: (nullable): optional GCancellable object, NULL to ignore.
 * @callback: (scope async): a GAsyncReadyCallback to call when the request is
 * satisfied.
 * @user_data: the data to pass to callback function.
 *
 * Set the destination for storing captured images
 * to @target.
 *
 * This can only be invoked when the camera is opened.
 *
 * This will execute in the background, and invoke @callback
 * when complete, whereupon entangle_camera_set_clock_finish
 * can be used to check the status
 */
void
entangle_camera_set_capture_target_async(EntangleCamera *cam,
                                         EntangleCameraCaptureTarget target,
                                         GCancellable *cancellable,
                                         GAsyncReadyCallback callback,
                                         gpointer user_data)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA(cam));

    EntangleCameraCaptureTarget *targetptr;
    GTask *task = g_task_new(cam, cancellable, callback, user_data);

    targetptr = g_new0(EntangleCameraCaptureTarget, 1);
    *targetptr = target;

    g_task_set_task_data(task, targetptr, g_free);

    g_task_run_in_thread(task, entangle_camera_set_capture_target_helper);
    g_object_unref(task);
}

/**
 * entangle_camera_capture_target_finish:
 * @cam: (transfer none): the camera
 * @result: a GAsyncResult
 * @error: a GError, or NULL
 *
 * Check the completion status of a previous call to
 * entangle_camera_capture_target_async.
 *
 * Returns: TRUE if the capture target was changed, FALSE on error
 */
gboolean
entangle_camera_set_capture_target_finish(EntangleCamera *cam,
                                          GAsyncResult *result,
                                          GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    return g_task_propagate_boolean(G_TASK(result), error);
}

/**
 * entangle_camera_get_has_capture:
 * @cam: (transfer none): the camera
 *
 * Check if the camera supports images capture
 *
 * Returns: TRUE if the camera supports image capture, FALSE otherwise
 */
gboolean
entangle_camera_get_has_capture(EntangleCamera *cam)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    gboolean ret;

    g_mutex_lock(&cam->lock);
    ret = cam->hasCapture;
    g_mutex_unlock(&cam->lock);

    return ret;
}

/**
 * entangle_camera_get_has_preview:
 * @cam: (transfer none): the camera
 *
 * Check if the camera supports images preview
 *
 * Returns: TRUE if the camera supports image preview, FALSE otherwise
 */
gboolean
entangle_camera_get_has_preview(EntangleCamera *cam)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    gboolean ret;

    g_mutex_lock(&cam->lock);
    ret = cam->hasPreview;
    g_mutex_unlock(&cam->lock);

    return ret;
}

/**
 * entangle_camera_get_has_settings:
 * @cam: (transfer none): the camera
 *
 * Check if the camera supports configuration settings
 *
 * Returns: TRUE if the camera supports configuration settings, FALSE otherwise
 */
gboolean
entangle_camera_get_has_settings(EntangleCamera *cam)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    gboolean ret;

    g_mutex_lock(&cam->lock);
    ret = cam->hasSettings;
    g_mutex_unlock(&cam->lock);

    return ret;
}

/**
 * entangle_camera_get_has_viewfinder:
 * @cam: (transfer none): the camera
 *
 * Check if the camera supports view finder control
 *
 * Returns: TRUE if the camera supports view finder control, FALSE otherwise
 */
gboolean
entangle_camera_get_has_viewfinder(EntangleCamera *cam)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    gboolean ret;

    g_mutex_lock(&cam->lock);
    ret = cam->hasViewfinder;
    g_mutex_unlock(&cam->lock);

    return ret;
}

/**
 * entangle_camera_get_has_autofocus:
 * @cam: (transfer none): the camera
 *
 * Check if the camera supports autofocus control
 *
 * Returns: TRUE if the camera supports autofocus control, FALSE otherwise
 */
gboolean
entangle_camera_get_has_autofocus(EntangleCamera *cam)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    gboolean ret;

    g_mutex_lock(&cam->lock);
    ret = cam->hasAutofocus;
    g_mutex_unlock(&cam->lock);

    return ret;
}

/**
 * entangle_camera_get_has_focus_area:
 * @cam: (transfer none): the camera
 *
 * Check if the camera supports selecting a focus area position
 *
 * Returns: TRUE if the camera supports a focus area position, FALSE otherwise
 */
gboolean
entangle_camera_get_has_focus_area(EntangleCamera *cam)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    gboolean ret;

    g_mutex_lock(&cam->lock);
    ret = cam->hasFocusArea;
    g_mutex_unlock(&cam->lock);

    return ret;
}

/**
 * entangle_camera_get_has_manual_focus:
 * @cam: (transfer none): the camera
 *
 * Check if the camera supports manual focus adjustment
 *
 * Returns: TRUE if the camera supports manual focus adjustment,
 *          FALSE otherwise
 */
gboolean
entangle_camera_get_has_manual_focus(EntangleCamera *cam)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    gboolean ret;

    g_mutex_lock(&cam->lock);
    ret = cam->hasManualFocus;
    g_mutex_unlock(&cam->lock);

    return ret;
}

/**
 * entangle_camera_set_progress:
 * @cam: (transfer none): the camera
 * @prog: (transfer none)(allow-none): the progress instance
 *
 * Set the object instance to receive operation progress
 * notifications
 */
void
entangle_camera_set_progress(EntangleCamera *cam, EntangleProgress *prog)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA(cam));

    g_mutex_lock(&cam->lock);
    if (cam->progress)
        g_object_unref(cam->progress);
    cam->progress = prog;
    if (cam->progress)
        g_object_ref(cam->progress);
    g_mutex_unlock(&cam->lock);
}

/**
 * entangle_camera_get_progress:
 * @cam: (transfer none): the camera
 *
 * Get the object instance that is receiving operation
 * progress notifications
 *
 * Returns: (transfer full): the progress object
 */
EntangleProgress *
entangle_camera_get_progress(EntangleCamera *cam)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), NULL);

    EntangleProgress *ret;

    g_mutex_unlock(&cam->lock);
    ret = cam->progress;
    g_object_ref(ret);
    g_mutex_unlock(&cam->lock);

    return ret;
}

static GMount *
entangle_device_manager_find_mount(EntangleCamera *cam, GVolumeMonitor *monitor)
{
    gchar *uri1;
    gchar *uri2;
    gchar *uri3 = NULL;
    GList *mounts;
    GList *tmp;
    GMount *ret = NULL;
    gchar *host, *eschost;

    g_mutex_lock(&cam->lock);
    uri1 = g_strdup_printf("gphoto2://[%s]/", cam->port);
    host = g_strdup_printf("[%s]", cam->port);
    eschost = g_uri_escape_string(host, ",", TRUE);
    uri2 = g_strdup_printf("gphoto2://%s/", eschost);
    if (cam->serial)
        uri3 = g_strdup_printf("gphoto2://%s/", cam->serial);
    g_mutex_unlock(&cam->lock);

    ENTANGLE_DEBUG("Look for URI 1 '%s'", uri1);
    ENTANGLE_DEBUG("Look for URI 2 '%s'", uri2);
    if (uri3)
        ENTANGLE_DEBUG("Look for URI 3 '%s'", uri3);
    mounts = g_volume_monitor_get_mounts(monitor);
    tmp = mounts;
    while (tmp) {
        GMount *mount = tmp->data;
        GFile *root = g_mount_get_root(mount);
        gchar *thisuri = g_file_get_uri(root);

        ENTANGLE_DEBUG("Check against URI '%s'", thisuri);
        if (g_str_equal(uri1, thisuri) || g_str_equal(uri2, thisuri) ||
            (uri3 && g_str_equal(uri3, thisuri)))
            ret = mount;
        else
            g_object_unref(mount);

        g_free(thisuri);
        g_object_unref(root);
        tmp = tmp->next;
    }
    g_list_free(mounts);

    g_free(uri1);
    g_free(uri2);
    g_free(uri3);
    g_free(host);
    g_free(eschost);

    return ret;
}

/**
 * entangle_camera_is_mounted:
 * @cam: (transfer none): the camera
 *
 * Check whether the camera is mounted as a virtual filesystem,
 * which would prevent opening it.
 *
 * Returns: TRUE if the camera is mounted, FALSE otherwise
 */
gboolean
entangle_camera_is_mounted(EntangleCamera *cam)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);
    /*
     * For some reason if we ever unref monitor, we get a 100%
     * CPU burn loop in a bg thread from DBus messages. So we
     * have made this static for now
     */
    static GVolumeMonitor *monitor;

    GMount *mount;
    gboolean ret;
    if (!monitor)
        monitor = g_volume_monitor_get();
    mount = entangle_device_manager_find_mount(cam, monitor);

    if (mount) {
        g_object_unref(mount);
        ret = TRUE;
    } else {
        ret = FALSE;
    }

    return ret;
}

struct UnmountData
{
    GVolumeMonitor *monitor;
    GMount *mount;
    GAsyncReadyCallback callback;
    gpointer user_data;
};

static void
entangle_camera_unmount_cleanup(GObject *object,
                                GAsyncResult *result,
                                gpointer user_data)
{
    struct UnmountData *data = user_data;

    data->callback(object, result, data->user_data);

    g_object_unref(data->monitor);
    if (data->mount)
        g_object_unref(data->mount);
    g_free(data);
}

static void
entangle_camera_unmount_complete(GObject *object,
                                 GAsyncResult *result,
                                 gpointer user_data)
{
    GTask *task = G_TASK(user_data);
    GError *err = NULL;
    GMount *mount = G_MOUNT(object);

    g_mount_unmount_with_operation_finish(mount, result, &err);

    if (err)
        g_task_return_error(task, err);
    else
        g_task_return_boolean(task, TRUE);
    g_object_unref(task);
}

/**
 * entangle_camera_unmount_async:
 * @cam: (transfer none): the camera
 * @cancellable: (nullable): optional GCancellable object, NULL to ignore.
 * @callback: (scope async): a GAsyncReadyCallback to call when the request is
 * satisfied.
 * @user_data: the data to pass to callback function.
 *
 * Unmount the camera virtual filesystem, allowing it to be
 * opened.
 *
 * This will execute in the background, and invoke @callback
 * when complete, whereupon entangle_camera_unmount_finish
 * can be used to check the status
 */
void
entangle_camera_unmount_async(EntangleCamera *cam,
                              GCancellable *cancellable,
                              GAsyncReadyCallback callback,
                              gpointer user_data)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA(cam));

    struct UnmountData *data = g_new0(struct UnmountData, 1);

    data->monitor = g_volume_monitor_get();
    data->mount = entangle_device_manager_find_mount(cam, data->monitor);
    data->callback = callback;
    data->user_data = user_data;

    GTask *task =
        g_task_new(cam, cancellable, entangle_camera_unmount_cleanup, data);

    if (data->mount) {
        g_mount_unmount_with_operation(data->mount, 0, NULL, cancellable,
                                       entangle_camera_unmount_complete, task);
    } else {
        g_task_return_boolean(task, TRUE);
        g_object_unref(task);
    }
}

/**
 * entangle_camera_unmount_finish:
 * @cam: (transfer none): the camera
 * @result: a GAsyncResult
 * @error: a GError, or NULL
 *
 * Check the completion status of a previous call to
 * entangle_camera_unmount_async.
 *
 * Returns: TRUE if the camera was unmounted, FALSE on error
 */
gboolean
entangle_camera_unmount_finish(EntangleCamera *cam,
                               GAsyncResult *result,
                               GError **error)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    return g_task_propagate_boolean(G_TASK(result), error);
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
