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

#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>

#include "entangle-application.h"

#include "entangle-camera-manager.h"
#include "entangle-camera-support.h"
#include "entangle-debug.h"
#include "entangle-help-about.h"
#include "entangle-preferences-display.h"
#include "entangle-preferences.h"

#if GIREPOSITORY_MAJOR_VERSION >= 2
#include <girepository/girepository.h>
#else
#include <girepository.h>
#endif

/**
 * SECTION:entangle-application
 * @Short_description: Global application state base class
 * @Title: EntangleApplication
 *
 * EntangleApplication maintains some global application state. At this time,
 * the list of currently attached cameras, the application preferences
 * and the plugin manager.
 */

struct _EntangleApplication
{
    GtkApplication parent;

    EntangleCameraList *activeCameras;
    EntangleCameraList *supportedCameras;

    EntanglePreferences *preferences;
    EntanglePreferencesDisplay *prefsDisplay;
    EntangleHelpAbout *about;
    EntangleCameraSupport *devices;

    GIRepository *giRepository;
    PeasEngine *pluginEngine;
    PeasExtensionSet *pluginExt;
};

G_DEFINE_TYPE(EntangleApplication, entangle_application, GTK_TYPE_APPLICATION);

enum
{
    PROP_0,
    PROP_ACTIVE_CAMERAS,
    PROP_SUPPORTED_CAMERAS,
    PROP_PREFERENCES,
};

static void
entangle_application_get_property(GObject *object,
                                  guint prop_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
    EntangleApplication *app = ENTANGLE_APPLICATION(object);

    switch (prop_id) {
    case PROP_ACTIVE_CAMERAS:
        g_value_set_object(value, app->activeCameras);
        break;

    case PROP_SUPPORTED_CAMERAS:
        g_value_set_object(value, app->supportedCameras);
        break;

    case PROP_PREFERENCES:
        g_value_set_object(value, app->preferences);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
entangle_application_set_property(GObject *object,
                                  guint prop_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
    EntangleApplication *app = ENTANGLE_APPLICATION(object);

    switch (prop_id) {
    case PROP_ACTIVE_CAMERAS:
        if (app->activeCameras)
            g_object_unref(app->activeCameras);
        app->activeCameras = g_value_get_object(value);
        g_object_ref(app->activeCameras);
        break;

    case PROP_SUPPORTED_CAMERAS:
        if (app->supportedCameras)
            g_object_unref(app->supportedCameras);
        app->supportedCameras = g_value_get_object(value);
        g_object_ref(app->supportedCameras);
        break;

    case PROP_PREFERENCES:
        if (app->preferences)
            g_object_unref(app->preferences);
        app->preferences = g_value_get_object(value);
        g_object_ref(app->preferences);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
entangle_application_finalize(GObject *object)
{
    EntangleApplication *app = ENTANGLE_APPLICATION(object);

    ENTANGLE_DEBUG("Finalize application %p", object);

    if (app->prefsDisplay)
        g_object_unref(app->prefsDisplay);
    if (app->activeCameras)
        g_object_unref(app->activeCameras);
    if (app->supportedCameras)
        g_object_unref(app->supportedCameras);
    if (app->preferences)
        g_object_unref(app->preferences);
    if (app->pluginEngine)
        g_object_unref(app->pluginEngine);
    if (app->pluginExt)
        g_object_unref(app->pluginExt);
    g_clear_object(&app->giRepository);

    G_OBJECT_CLASS(entangle_application_parent_class)->finalize(object);
}

static void
entangle_application_activate(GApplication *gapp)
{
    GList *wins = gtk_application_get_windows(GTK_APPLICATION(gapp));
    while (wins) {
        GtkWindow *win = wins->data;
        gtk_widget_show(GTK_WIDGET(win));
        gtk_window_present(win);
        wins = wins->next;
    }
}

static void
do_prefs_hide(GObject *media G_GNUC_UNUSED,
              GParamSpec *pspec G_GNUC_UNUSED,
              gpointer opaque)
{
    g_return_if_fail(ENTANGLE_IS_APPLICATION(opaque));

    EntangleApplication *app = ENTANGLE_APPLICATION(opaque);

    if (!gtk_widget_get_visible(GTK_WIDGET(app->prefsDisplay))) {
        gtk_widget_destroy(GTK_WIDGET(app->prefsDisplay));
        app->prefsDisplay = NULL;
    }
}

static void
do_action_preferences(GSimpleAction *act G_GNUC_UNUSED,
                      GVariant *param G_GNUC_UNUSED,
                      gpointer opaque)
{
    g_return_if_fail(ENTANGLE_IS_APPLICATION(opaque));

    EntangleApplication *app = ENTANGLE_APPLICATION(opaque);

    if (app->prefsDisplay == NULL) {
        GtkWindow *win =
            gtk_application_get_active_window(GTK_APPLICATION(app));
        app->prefsDisplay = entangle_preferences_display_new();
        gtk_application_add_window(GTK_APPLICATION(app),
                                   GTK_WINDOW(app->prefsDisplay));
        gtk_window_set_transient_for(GTK_WINDOW(app->prefsDisplay), win);
        g_signal_connect(app->prefsDisplay, "notify::visible",
                         G_CALLBACK(do_prefs_hide), app);
    }

    gtk_widget_show(GTK_WIDGET(app->prefsDisplay));
}

static void
do_action_devices(GSimpleAction *act G_GNUC_UNUSED,
                  GVariant *param G_GNUC_UNUSED,
                  gpointer opaque)
{
    g_return_if_fail(ENTANGLE_IS_APPLICATION(opaque));

    EntangleApplication *app = ENTANGLE_APPLICATION(opaque);

    if (!app->devices) {
        app->devices = entangle_camera_support_new();
        entangle_camera_support_set_camera_list(
            app->devices, entangle_application_get_supported_cameras(app));
        gtk_window_set_transient_for(
            GTK_WINDOW(app->devices),
            gtk_application_get_active_window(GTK_APPLICATION(app)));
    }

    gtk_widget_show(GTK_WIDGET(app->devices));
}

static void
do_action_quit(GSimpleAction *act G_GNUC_UNUSED,
               GVariant *param G_GNUC_UNUSED,
               gpointer opaque)
{
    g_return_if_fail(ENTANGLE_IS_APPLICATION(opaque));

    EntangleApplication *app = ENTANGLE_APPLICATION(opaque);
    GList *windows = gtk_application_get_windows(GTK_APPLICATION(app));
    GList *tmp = g_list_copy(windows);

    while (tmp) {
        GtkWidget *window = GTK_WIDGET(tmp->data);
        gtk_widget_destroy(window);
        tmp = tmp->next;
    }
    g_list_free(tmp);
}

static void
do_action_about(GSimpleAction *act G_GNUC_UNUSED,
                GVariant *param G_GNUC_UNUSED,
                gpointer opaque)
{
    g_return_if_fail(ENTANGLE_IS_APPLICATION(opaque));

    EntangleApplication *app = ENTANGLE_APPLICATION(opaque);

    if (!app->about) {
        app->about = entangle_help_about_new();
        gtk_window_set_transient_for(
            GTK_WINDOW(app->about),
            gtk_application_get_active_window(GTK_APPLICATION(app)));
    }

    gtk_widget_show(GTK_WIDGET(app->about));
}

static void
do_action_manual(GSimpleAction *act G_GNUC_UNUSED,
                 GVariant *param G_GNUC_UNUSED,
                 gpointer opaque)
{
    g_return_if_fail(ENTANGLE_IS_APPLICATION(opaque));

    EntangleApplication *app = ENTANGLE_APPLICATION(opaque);
    gtk_show_uri_on_window(
        gtk_application_get_active_window(GTK_APPLICATION(app)),
        "help:entangle", GDK_CURRENT_TIME, NULL);
}

static void
do_action_new_window(GSimpleAction *act G_GNUC_UNUSED,
                     GVariant *param G_GNUC_UNUSED,
                     gpointer opaque)
{
    g_return_if_fail(ENTANGLE_IS_APPLICATION(opaque));

    EntangleApplication *app = ENTANGLE_APPLICATION(opaque);
    EntangleCameraManager *newmanager = entangle_camera_manager_new(app);

    gtk_widget_show(GTK_WIDGET(newmanager));
    gtk_window_present(GTK_WINDOW(newmanager));
}

static GActionEntry actions[] = {
    { .name = "about", .activate = do_action_about },
    { .name = "manual", .activate = do_action_manual },
    { .name = "devices", .activate = do_action_devices },
    { .name = "quit", .activate = do_action_quit },
    { .name = "new-window", .activate = do_action_new_window },
    { .name = "preferences", .activate = do_action_preferences },
};

struct
{
    const char *name;
    const char *accels[3];
} actionAccels[] = {
    { .name = "app.new-window", .accels = { "<Primary>n", NULL } },
    { .name = "app.quit", .accels = { "<Primary>q", NULL } },
    { .name = "app.manual", .accels = { "F1", NULL } },
    { .name = "win.capture", .accels = { "s", "space", NULL } },
    { .name = "win.preview", .accels = { "p", NULL } },
    { .name = "win.rotate-cw", .accels = { "<Primary>r", NULL } },
    { .name = "win.rotate-ccw", .accels = { "<Primary><Shift>r", NULL } },
    { .name = "win.rotate-reset", .accels = { "<Primary><Shift>0", NULL } },
    { .name = "win.focus-in", .accels = { "<Primary>bracketright", NULL } },
    { .name = "win.focus-out", .accels = { "<Primary>bracketleft", NULL } },
    { .name = "win.zoom-in",
      .accels = { "<Primary>plus", "<Primary>equal", NULL } },
    { .name = "win.zoom-out", .accels = { "<Primary>minus", NULL } },
    { .name = "win.zoom-normal", .accels = { "<Primary>0", NULL } },
    { .name = "win.zoom-best", .accels = { "<Primary>i", NULL } },
    { .name = "win.fullscreen", .accels = { "F11", NULL } },
    { .name = "win.presentation", .accels = { "F5", NULL } },

};

static void
entangle_application_startup(GApplication *gapp)
{
    g_return_if_fail(ENTANGLE_IS_APPLICATION(gapp));

    EntangleApplication *app = ENTANGLE_APPLICATION(gapp);
    GList *cameras = NULL, *tmp = NULL;
    gboolean gotcamera = FALSE;
    gsize i;

    (*G_APPLICATION_CLASS(entangle_application_parent_class)->startup)(gapp);

    g_action_map_add_action_entries(G_ACTION_MAP(gapp), actions,
                                    G_N_ELEMENTS(actions), app);

    for (i = 0; i < G_N_ELEMENTS(actionAccels); i++)
        gtk_application_set_accels_for_action(GTK_APPLICATION(gapp),
                                              actionAccels[i].name,
                                              actionAccels[i].accels);

    gtk_window_set_default_icon_name("org.entangle_photo.Manager");

    if (entangle_preferences_interface_get_auto_connect(app->preferences))
        cameras = tmp = entangle_camera_list_get_cameras(app->activeCameras);

    while (tmp) {
        EntangleCamera *cam = ENTANGLE_CAMERA(tmp->data);

        ENTANGLE_DEBUG("Opening window for %s", entangle_camera_get_port(cam));

        if (entangle_camera_get_has_capture(cam)) {
            EntangleCameraManager *manager = entangle_camera_manager_new(app);
            gtk_widget_show(GTK_WIDGET(manager));
            entangle_camera_manager_set_camera(manager, cam);
            gotcamera = TRUE;
        }

        tmp = tmp->next;
    }
    g_list_free(cameras);

    if (!gotcamera) {
        EntangleCameraManager *manager = entangle_camera_manager_new(app);
        gtk_widget_show(GTK_WIDGET(manager));
    }
}

static void
entangle_application_class_init(EntangleApplicationClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GApplicationClass *app_class = G_APPLICATION_CLASS(klass);

    object_class->finalize = entangle_application_finalize;
    object_class->get_property = entangle_application_get_property;
    object_class->set_property = entangle_application_set_property;

    app_class->activate = entangle_application_activate;
    app_class->startup = entangle_application_startup;

    g_object_class_install_property(
        object_class, PROP_ACTIVE_CAMERAS,
        g_param_spec_object("active-cameras", "Active cameras",
                            "List of active cameras", ENTANGLE_TYPE_CAMERA_LIST,
                            G_PARAM_READABLE | G_PARAM_STATIC_NAME |
                                G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    g_object_class_install_property(
        object_class, PROP_SUPPORTED_CAMERAS,
        g_param_spec_object("supported-cameras", "Supported cameras",
                            "List of supported cameras",
                            ENTANGLE_TYPE_CAMERA_LIST,
                            G_PARAM_READABLE | G_PARAM_STATIC_NAME |
                                G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    g_object_class_install_property(
        object_class, PROP_PREFERENCES,
        g_param_spec_object("preferences", "Preferences",
                            "Application preferences",
                            ENTANGLE_TYPE_PREFERENCES,
                            G_PARAM_READABLE | G_PARAM_STATIC_NAME |
                                G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
}

EntangleApplication *
entangle_application_new(void)
{
    return ENTANGLE_APPLICATION(
        g_object_new(ENTANGLE_TYPE_APPLICATION, "application-id",
                     "org.entangle_photo.Manager", NULL));
}

static void
on_extension_added(PeasExtensionSet *set G_GNUC_UNUSED,
                   PeasPluginInfo *info G_GNUC_UNUSED,
                   PeasExtension *exten,
                   gpointer opaque G_GNUC_UNUSED)
{
    peas_activatable_activate(PEAS_ACTIVATABLE(exten));
}

static void
on_extension_removed(PeasExtensionSet *set G_GNUC_UNUSED,
                     PeasPluginInfo *info G_GNUC_UNUSED,
                     PeasExtension *exten,
                     gpointer opaque G_GNUC_UNUSED)
{
    peas_activatable_deactivate(PEAS_ACTIVATABLE(exten));
}

static void
on_plugin_load(PeasEngine *engine G_GNUC_UNUSED,
               PeasPluginInfo *info,
               gpointer data)
{
    g_return_if_fail(ENTANGLE_IS_APPLICATION(data));

    EntangleApplication *app = data;

    entangle_preferences_interface_add_plugin(
        app->preferences, peas_plugin_info_get_module_name(info));
}

static void
on_plugin_unload(PeasEngine *engine G_GNUC_UNUSED,
                 PeasPluginInfo *info,
                 gpointer data)
{
    g_return_if_fail(ENTANGLE_IS_APPLICATION(data));

    EntangleApplication *app = data;

    entangle_preferences_interface_remove_plugin(
        app->preferences, peas_plugin_info_get_module_name(info));
}

static void
entangle_application_init(EntangleApplication *app)
{
    int i;
    gchar *userdir;
    gchar **plugins;
    GtkSettings *gtk_settings;

    gtk_settings = gtk_settings_get_default();
    g_object_set(G_OBJECT(gtk_settings), "gtk-application-prefer-dark-theme",
                 TRUE, NULL);

    app->preferences = entangle_preferences_new();
    app->activeCameras = entangle_camera_list_new_active();
    app->supportedCameras = entangle_camera_list_new_supported();

#if GIREPOSITORY_MAJOR_VERSION >= 2
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    app->giRepository = gi_repository_dup_default();
#pragma GCC diagnostic pop
    gi_repository_require(app->giRepository, "Peas", "1.0", 0, NULL);
#else
    app->giRepository = g_object_ref(g_irepository_get_default());
    g_irepository_require(app->giRepository, "Peas", "1.0", 0, NULL);
#endif

    userdir =
        g_build_filename(g_get_user_config_dir(), "entangle/plugins", NULL);
    g_mkdir_with_parents(userdir, 0777);

    app->pluginEngine = peas_engine_get_default();
    peas_engine_enable_loader(app->pluginEngine, "python3");

    peas_engine_add_search_path(app->pluginEngine, userdir, userdir);
    g_free(userdir);
    peas_engine_add_search_path(app->pluginEngine, LIBDIR "/entangle/plugins",
                                DATADIR "/entangle/plugins");
    peas_engine_rescan_plugins(app->pluginEngine);

    app->pluginExt = peas_extension_set_new(
        app->pluginEngine, PEAS_TYPE_ACTIVATABLE, "object", app, NULL);

    peas_extension_set_foreach(
        app->pluginExt, (PeasExtensionSetForeachFunc)on_extension_added, NULL);

    g_signal_connect(app->pluginExt, "extension-added",
                     G_CALLBACK(on_extension_added), NULL);
    g_signal_connect(app->pluginExt, "extension-removed",
                     G_CALLBACK(on_extension_removed), NULL);

    g_signal_connect(app->pluginEngine, "load-plugin",
                     G_CALLBACK(on_plugin_load), app);
    g_signal_connect(app->pluginEngine, "unload-plugin",
                     G_CALLBACK(on_plugin_unload), app);

    plugins = entangle_preferences_interface_get_plugins(app->preferences);
    for (i = 0; plugins[i] != NULL; i++) {
        PeasPluginInfo *plugin =
            peas_engine_get_plugin_info(app->pluginEngine, plugins[i]);
        if (plugin) {
            ENTANGLE_DEBUG("Plugin %p %s", plugin, plugins[i]);
            peas_engine_load_plugin(app->pluginEngine, plugin);
        }
    }
    g_strfreev(plugins);
}

/**
 * entangle_application_get_active_cameras:
 * @app: the application
 *
 * Retrieve the active camera list
 *
 * Returns: (transfer none): the camera list
 */
EntangleCameraList *
entangle_application_get_active_cameras(EntangleApplication *app)
{
    g_return_val_if_fail(ENTANGLE_IS_APPLICATION(app), NULL);

    return app->activeCameras;
}

/**
 * entangle_application_get_supported_cameras:
 * @app: the application
 *
 * Retrieve the supported camera list
 *
 * Returns: (transfer none): the camera list
 */
EntangleCameraList *
entangle_application_get_supported_cameras(EntangleApplication *app)
{
    g_return_val_if_fail(ENTANGLE_IS_APPLICATION(app), NULL);

    return app->supportedCameras;
}

/**
 * entangle_application_get_preferences:
 * @app: the application
 *
 * Retrieve the application preferences object
 *
 * Returns: (transfer none): the application preferences
 */
EntanglePreferences *
entangle_application_get_preferences(EntangleApplication *app)
{
    g_return_val_if_fail(ENTANGLE_IS_APPLICATION(app), NULL);

    return app->preferences;
}

/**
 * entangle_application_get_plugin_engine:
 * @app: the application
 *
 * Retrieve the plugin manager
 *
 * Returns: (transfer none): the plugin engine
 */
PeasEngine *
entangle_application_get_plugin_engine(EntangleApplication *app)
{
    g_return_val_if_fail(ENTANGLE_IS_APPLICATION(app), NULL);

    return app->pluginEngine;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
