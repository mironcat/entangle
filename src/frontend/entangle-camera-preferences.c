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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>

#include "entangle-camera-preferences.h"

#include "entangle-debug.h"

struct _EntangleCameraPreferences
{
    GObject parent;
    EntangleCamera *camera;
    GSettings *settings;
};

G_DEFINE_TYPE(EntangleCameraPreferences,
              entangle_camera_preferences,
              G_TYPE_OBJECT);

enum
{
    PROP_O,
    PROP_CAMERA,
};

static void
entangle_camera_preferences_get_property(GObject *object,
                                         guint prop_id,
                                         GValue *value,
                                         GParamSpec *pspec)
{
    EntangleCameraPreferences *prefs = ENTANGLE_CAMERA_PREFERENCES(object);

    switch (prop_id) {
    case PROP_CAMERA:
        g_value_set_object(value, prefs->camera);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
entangle_camera_preferences_set_property(GObject *object,
                                         guint prop_id,
                                         const GValue *value,
                                         GParamSpec *pspec)
{
    EntangleCameraPreferences *prefs = ENTANGLE_CAMERA_PREFERENCES(object);

    ENTANGLE_DEBUG("Set prop on camera preferences %d", prop_id);

    switch (prop_id) {
    case PROP_CAMERA:
        entangle_camera_preferences_set_camera(prefs,
                                               g_value_get_object(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
entangle_camera_preferences_finalize(GObject *object)
{
    EntangleCameraPreferences *prefs = ENTANGLE_CAMERA_PREFERENCES(object);

    ENTANGLE_DEBUG("Finalize preferences %p", object);

    if (prefs->settings)
        g_object_unref(prefs->settings);

    G_OBJECT_CLASS(entangle_camera_preferences_parent_class)->finalize(object);
}

static void
entangle_camera_preferences_class_init(EntangleCameraPreferencesClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = entangle_camera_preferences_finalize;
    object_class->get_property = entangle_camera_preferences_get_property;
    object_class->set_property = entangle_camera_preferences_set_property;

    g_object_class_install_property(
        object_class, PROP_CAMERA,
        g_param_spec_object("camera", "Camera", "Camera to managed",
                            ENTANGLE_TYPE_CAMERA,
                            G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
}

EntangleCameraPreferences *
entangle_camera_preferences_new(void)
{
    return ENTANGLE_CAMERA_PREFERENCES(
        g_object_new(ENTANGLE_TYPE_CAMERA_PREFERENCES, NULL));
}

static void
entangle_camera_preferences_init(EntangleCameraPreferences *prefs G_GNUC_UNUSED)
{
}

/**
 * entangle_camera_preferences_set_camera:
 * @preferences: the camera widget
 * @camera: (transfer none)(allow-none): the camera to display cameras for, or
 * NULL
 *
 * Set the camera to display cameras for
 */
void
entangle_camera_preferences_set_camera(EntangleCameraPreferences *prefs,
                                       EntangleCamera *camera)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_PREFERENCES(prefs));
    g_return_if_fail(!camera || ENTANGLE_IS_CAMERA(camera));

    if (prefs->camera) {
        g_object_unref(prefs->camera);
        g_object_unref(prefs->settings);
        prefs->camera = NULL;
        prefs->settings = NULL;
    }
    if (camera) {
        prefs->camera = g_object_ref(camera);

        gchar *suffix = g_strdup(entangle_camera_get_model(prefs->camera));
        gsize i;
        for (i = 0; suffix[i] != '\0'; i++) {
            if (g_ascii_isalnum(suffix[i]) || suffix[i] == '/' ||
                suffix[i] == '-')
                continue;
            suffix[i] = '-';
        }
        gchar *path =
            g_strdup_printf("/org/entangle-photo/manager/camera/%s/", suffix);
        prefs->settings =
            g_settings_new_with_path("org.entangle-photo.manager.camera", path);

        g_free(suffix);
        g_free(path);
    }
    g_object_notify(G_OBJECT(prefs), "camera");
}

/**
 * entangle_camera_preferences_get_camera:
 * @preferences: the camera widget
 *
 * Get the camera whose cameras are displayed
 *
 * Returns: (transfer none): the camera or NULL
 */
EntangleCamera *
entangle_camera_preferences_get_camera(EntangleCameraPreferences *prefs)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA_PREFERENCES(prefs), NULL);

    return prefs->camera;
}

/**
 * entangle_camera_preferences_get_controls:
 * @prefs: (transfer none): the camera preferences
 *
 * Returns: (transfer full): the list of controls
 */
gchar **
entangle_camera_preferences_get_controls(EntangleCameraPreferences *prefs)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA_PREFERENCES(prefs), NULL);

    if (!prefs->settings)
        return NULL;

    return g_settings_get_strv(prefs->settings, "controls");
}

/**
 * entangle_camera_preferences_set_controls:
 * @prefs: (transfer none): the camera preferences
 * @controls: (transfer none): the list of controls
 */
void
entangle_camera_preferences_set_controls(EntangleCameraPreferences *prefs,
                                         const gchar *const *controls)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_PREFERENCES(prefs));

    if (!prefs->settings)
        return;

    g_settings_set_strv(prefs->settings, "controls", controls);
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
