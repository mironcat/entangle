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
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gdk/gdkx.h>
#include <gio/gunixoutputstream.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include "entangle-camera-manager.h"

#include "entangle-camera-automata.h"
#include "entangle-camera-list.h"
#include "entangle-camera-picker.h"
#include "entangle-colour-profile.h"
#include "entangle-control-panel.h"
#include "entangle-debug.h"
#include "entangle-dpms.h"
#include "entangle-image-display.h"
#include "entangle-image-histogram.h"
#include "entangle-image-loader.h"
#include "entangle-media-popup.h"
#include "entangle-media-statusbar.h"
#include "entangle-progress.h"
#include "entangle-script-config.h"
#include "entangle-session-browser.h"
#include "entangle-session.h"
#include "entangle-thumbnail-loader.h"

struct _EntangleCameraManager
{
    GtkApplicationWindow parent;

    EntangleCameraAutomata *automata;
    EntangleCamera *camera;
    EntangleCameraPreferences *cameraPrefs;
    gboolean cameraReady;
    gboolean cameraChanged;
    EntangleSession *session;
    EntangleScriptConfig *scriptConfig;

    EntanglePreferences *prefs;
    EntangleCameraPicker *picker;

    EntangleImageLoader *imageLoader;
    EntangleThumbnailLoader *thumbLoader;
    EntangleColourProfileTransform *colourTransform;
    GtkScrolledWindow *imageScroll;
    EntangleImageDisplay *imageDisplay;
    EntangleMediaStatusbar *mediaStatusbar;
    GtkRevealer *imageDrawer;
    gulong imageDrawerTimer;
    EntangleSessionBrowser *sessionBrowser;
    GtkMenu *sessionBrowserMenu;
    EntangleMedia *sessionBrowserMedia;
    EntangleControlPanel *controlPanel;
    EntangleImageHistogram *imageHistogram;
    GtkWidget *scriptConfigExpander;

    GtkBox *winBox;
    GtkHeaderBar *toolbarBox;
    GtkButton *toolbarButtonCapture;
    GtkToggleButton *toolbarButtonPreview;
    GtkButton *toolbarButtonCancel;
    GtkMenuButton *toolbarButtonAction;
    GtkMenuButton *toolbarButtonView;
    GtkMenuButton *toolbarButtonZoom;
    GtkSpinner *toolbarStatus;
    GtkPaned *winMainpane;
    GtkScrolledWindow *settingsScroll;
    GtkViewport *settingsViewport;
    GtkPaned *displayPanel;
    GtkHeaderBar *titlebarBox;
    GtkButton *titlebarButtonCapture;
    GtkToggleButton *titlebarButtonPreview;
    GtkButton *titlebarButtonCancel;
    GtkMenuButton *titlebarButtonAction;
    GtkMenuButton *titlebarButtonView;
    GtkMenuButton *titlebarButtonZoom;
    GtkSpinner *titlebarStatus;

    EntangleMedia *currentMedia;

    EntangleMediaPopup *mediaPresentation;
    gint presentationMonitor;
    GHashTable *popups;

    gdouble imageScrollVOffset;
    gdouble imageScrollHOffset;
    gboolean imageScrollRestored;

    int zoomLevel;

    gulong sigFilePreview;
    gulong sigChanged;
    gulong sigPrefsNotify;

    GCancellable *monitorCancel;
    GCancellable *taskCancel;
    GCancellable *taskConfirm;
    gboolean taskCapture;
    gboolean taskPreview;
    gboolean taskActive;
    gboolean taskProcessEvents;
    char *deleteImageDup;
    gboolean syncCapture;
    gboolean mediaContextMenu;
};

static void
entangle_camera_progress_interface_init(gpointer g_iface, gpointer iface_data);
static void
do_select_media(EntangleCameraManager *manager, EntangleMedia *media);

G_DEFINE_TYPE_EXTENDED(
    EntangleCameraManager,
    entangle_camera_manager,
    GTK_TYPE_APPLICATION_WINDOW,
    0,
    G_IMPLEMENT_INTERFACE(ENTANGLE_TYPE_PROGRESS,
                          entangle_camera_progress_interface_init));

enum
{
    PROP_O,
    PROP_CAMERA,
};

gboolean
do_manager_key_release(GtkWidget *widget G_GNUC_UNUSED,
                       GdkEventKey *ev,
                       gpointer data);
void
do_menu_session_open_activate(GtkMenuItem *item G_GNUC_UNUSED,
                              EntangleCameraManager *manager);
void
do_menu_session_delete_activate(GtkMenuItem *item G_GNUC_UNUSED,
                                EntangleCameraManager *manager);

static void
do_camera_sync_clock_finish(GObject *src, GAsyncResult *res, gpointer opaque);

static EntanglePreferences *
entangle_camera_manager_get_preferences(EntangleCameraManager *manager)
{
    GtkApplication *gapp = gtk_window_get_application(GTK_WINDOW(manager));
    EntangleApplication *app = ENTANGLE_APPLICATION(gapp);
    return entangle_application_get_preferences(app);
}

static int
entangle_camera_manager_get_monitor_num(GdkMonitor *monitor)
{
    GdkDisplay *display;
    int n_monitors, i;

    display = gdk_monitor_get_display(monitor);
    n_monitors = gdk_display_get_n_monitors(display);
    for (i = 0; i < n_monitors; i++) {
        if (gdk_display_get_monitor(display, i) == monitor)
            return i;
    }
    return -1;
}

static EntangleColourProfile *
entangle_camera_manager_monitor_profile(GtkWindow *window)
{
    GdkDisplay *display;
    GdkMonitor *monitor;
    GdkScreen *screen;
    GByteArray *profileData;
    gchar *atom;
    int monitorIdx = 0;
    GdkAtom type = GDK_NONE;
    gint format = 0;
    gint nitems = 0;
    guint8 *data = NULL;
    EntangleColourProfile *profile = NULL;

    gtk_widget_realize(GTK_WIDGET(window));

    screen = gtk_widget_get_screen(GTK_WIDGET(window));
    display = gtk_widget_get_display(GTK_WIDGET(window));
    monitor = gdk_display_get_monitor_at_window(
        display, gtk_widget_get_window(GTK_WIDGET(window)));
    monitorIdx = entangle_camera_manager_get_monitor_num(monitor);

    if (monitorIdx == 0)
        atom = g_strdup("_ICC_PROFILE");
    else
        atom = g_strdup_printf("_ICC_PROFILE_%d", monitorIdx);

    if (!gdk_property_get(gdk_screen_get_root_window(screen),
                          gdk_atom_intern(atom, FALSE), GDK_NONE, 0,
                          64 * 1024 * 1024, FALSE, &type, &format, &nitems,
                          &data) ||
        nitems <= 0)
        goto cleanup;

    profileData = g_byte_array_new();
    g_byte_array_append(profileData, data, nitems);

    profile = entangle_colour_profile_new_data(profileData);
    g_byte_array_unref(profileData);

cleanup:
    g_free(data);
    g_free(atom);

    return profile;
}

static EntangleColourProfileTransform *
entangle_camera_manager_colour_transform(EntangleCameraManager *manager)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager), NULL);

    EntangleColourProfileTransform *transform = NULL;
    EntanglePreferences *prefs =
        entangle_camera_manager_get_preferences(manager);

    if (entangle_preferences_cms_get_enabled(prefs)) {
        EntangleColourProfile *rgbProfile;
        EntangleColourProfile *monitorProfile;
        EntangleColourProfileIntent intent;

        rgbProfile = entangle_preferences_cms_get_rgb_profile(prefs);
        intent = entangle_preferences_cms_get_rendering_intent(prefs);
        if (entangle_preferences_cms_get_detect_system_profile(prefs)) {
            monitorProfile =
                entangle_camera_manager_monitor_profile(GTK_WINDOW(manager));
        } else {
            monitorProfile =
                entangle_preferences_cms_get_monitor_profile(prefs);
        }

        if (monitorProfile) {
            transform = entangle_colour_profile_transform_new(
                rgbProfile, monitorProfile, intent);
            g_object_unref(monitorProfile);
        }
        g_object_unref(rgbProfile);
    }

    return transform;
}

static void
entangle_camera_manager_update_colour_transform(EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    if (manager->colourTransform)
        g_object_unref(manager->colourTransform);

    manager->colourTransform =
        entangle_camera_manager_colour_transform(manager);
    if (manager->imageLoader)
        entangle_pixbuf_loader_set_colour_transform(
            ENTANGLE_PIXBUF_LOADER(manager->imageLoader),
            manager->colourTransform);
    if (manager->thumbLoader)
        entangle_pixbuf_loader_set_colour_transform(
            ENTANGLE_PIXBUF_LOADER(manager->thumbLoader),
            manager->colourTransform);
}

static void
entangle_camera_manager_update_aspect_ratio(EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    EntanglePreferences *prefs =
        entangle_camera_manager_get_preferences(manager);
    gchar *aspect = entangle_preferences_img_get_aspect_ratio(prefs);

    if (!aspect) {
        entangle_image_display_set_aspect_ratio(manager->imageDisplay, 1.33);
    } else {
        gdouble d;
        gchar *end;
        d = g_ascii_strtod(aspect, &end);
        if (end == aspect || (errno == ERANGE))
            entangle_image_display_set_aspect_ratio(manager->imageDisplay,
                                                    1.33);
        else
            entangle_image_display_set_aspect_ratio(manager->imageDisplay, d);
    }
    g_free(aspect);
}

static void
entangle_camera_manager_update_mask_opacity(EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    EntanglePreferences *prefs =
        entangle_camera_manager_get_preferences(manager);
    gint opacity = entangle_preferences_img_get_mask_opacity(prefs);

    entangle_image_display_set_mask_opacity(manager->imageDisplay,
                                            opacity / 100.0);
}

static void
entangle_camera_manager_update_mask_enabled(EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    EntanglePreferences *prefs =
        entangle_camera_manager_get_preferences(manager);
    gboolean enabled = entangle_preferences_img_get_mask_enabled(prefs);

    entangle_image_display_set_mask_enabled(manager->imageDisplay, enabled);
}

static void
entangle_camera_manager_update_overexposure_highlighting(
    EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    EntanglePreferences *prefs =
        entangle_camera_manager_get_preferences(manager);
    gboolean enabled =
        entangle_preferences_img_get_overexposure_highlighting(prefs);

    entangle_image_display_set_overexposure_highlighting(manager->imageDisplay,
                                                         enabled);
}

static void
entangle_camera_manager_update_flip_vertically(EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    EntanglePreferences *prefs =
        entangle_camera_manager_get_preferences(manager);
    gboolean enabled = entangle_preferences_img_get_flip_vertically(prefs);

    entangle_image_display_set_flip_vertically(manager->imageDisplay, enabled);
}

static void
entangle_camera_manager_update_flip_horizontally(EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    EntanglePreferences *prefs =
        entangle_camera_manager_get_preferences(manager);
    gboolean enabled = entangle_preferences_img_get_flip_horizontally(prefs);

    entangle_image_display_set_flip_horizontally(manager->imageDisplay,
                                                 enabled);
}

static void
entangle_camera_manager_update_histogram_linear(EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    EntanglePreferences *prefs =
        entangle_camera_manager_get_preferences(manager);
    gboolean histogram_linear =
        entangle_preferences_interface_get_histogram_linear(prefs);

    entangle_image_histogram_set_histogram_linear(manager->imageHistogram,
                                                  histogram_linear);
}

static void
entangle_camera_manager_update_background_highlight(
    EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    EntanglePreferences *prefs =
        entangle_camera_manager_get_preferences(manager);
    GHashTableIter iter;
    void *key;
    void *value;
    gchar *bg = entangle_preferences_img_get_background(prefs);
    gchar *hl = entangle_preferences_img_get_highlight(prefs);

    memset(&iter, 0, sizeof(iter));

    entangle_image_display_set_background(manager->imageDisplay, bg);
    entangle_session_browser_set_background(manager->sessionBrowser, bg);
    entangle_session_browser_set_highlight(manager->sessionBrowser, hl);
    if (manager->mediaPresentation)
        entangle_media_popup_set_background(manager->mediaPresentation, bg);

    g_hash_table_iter_init(&iter, manager->popups);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        EntangleMediaPopup *pop = value;
        entangle_media_popup_set_background(pop, bg);
    }
    g_free(bg);
    g_free(hl);
}

static void
do_action_presentation_monitor(GSimpleAction *act G_GNUC_UNUSED,
                               GVariant *state,
                               gpointer opaque)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(opaque));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(opaque);

    manager->presentationMonitor = g_variant_get_int32(state);

    ENTANGLE_DEBUG("Set monitor %d", manager->presentationMonitor);

    if (manager->mediaPresentation)
        entangle_media_popup_move_to_monitor(manager->mediaPresentation,
                                             manager->presentationMonitor);
}

static GMenu *
entangle_camera_manager_monitor_menu(EntangleCameraManager *manager)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager), NULL);

    GdkDisplay *display = gtk_widget_get_display(GTK_WIDGET(manager));
    GMenu *menu = g_menu_new();
    GSimpleAction *action;

    action = G_SIMPLE_ACTION(g_action_map_lookup_action(
        G_ACTION_MAP(manager), "presentation-monitor"));

    for (int i = 0; i < gdk_display_get_n_monitors(display); i++) {
        GdkMonitor *monitor = gdk_display_get_monitor(display, i);
        const gchar *name = gdk_monitor_get_model(monitor);
        GMenuItem *submenu = g_menu_item_new(name, NULL);
        g_menu_item_set_action_and_target_value(
            submenu, "win.presentation-monitor", g_variant_new_int32(i));

        g_menu_append_item(menu, submenu);
        g_object_unref(submenu);
    }

    g_simple_action_set_state(action, g_variant_new_int32(0));

    manager->presentationMonitor = 0;

    return menu;
}

static void
entangle_camera_manager_update_viewfinder(EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    if (manager->taskPreview) {
        EntanglePreferences *prefs =
            entangle_camera_manager_get_preferences(manager);
        gint gridLines = entangle_preferences_img_get_grid_lines(prefs);
        gboolean focusPoint = entangle_preferences_img_get_focus_point(prefs);

        entangle_image_display_set_focus_point(manager->imageDisplay,
                                               focusPoint);
        entangle_image_display_set_grid_display(manager->imageDisplay,
                                                gridLines);
    } else {
        entangle_image_display_set_focus_point(manager->imageDisplay, FALSE);
        entangle_image_display_set_grid_display(
            manager->imageDisplay, ENTANGLE_IMAGE_DISPLAY_GRID_NONE);
    }
}

static void
entangle_camera_manager_update_image_loader(EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    EntanglePreferences *prefs =
        entangle_camera_manager_get_preferences(manager);
    gboolean embeddedPreview =
        entangle_preferences_img_get_embedded_preview(prefs);
    entangle_image_loader_set_embedded_preview(manager->imageLoader,
                                               embeddedPreview);
}

static void
entangle_camera_manager_update_automata(EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    EntanglePreferences *prefs =
        entangle_camera_manager_get_preferences(manager);
    gboolean deleteFile = entangle_preferences_capture_get_delete_file(prefs);

    entangle_camera_automata_set_delete_file(manager->automata, deleteFile);
}

static void
entangle_camera_manager_prefs_changed(GObject *object G_GNUC_UNUSED,
                                      GParamSpec *spec,
                                      gpointer data)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(data));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(data);

    if (g_str_equal(spec->name, "interface-histogram-linear")) {
        entangle_camera_manager_update_histogram_linear(manager);
    } else if (g_str_equal(spec->name, "cms-enabled") ||
               g_str_equal(spec->name, "cms-rgb-profile") ||
               g_str_equal(spec->name, "cms-monitor-profile") ||
               g_str_equal(spec->name, "cms-detect-system-profile") ||
               g_str_equal(spec->name, "cms-rendering-intent")) {
        entangle_camera_manager_update_colour_transform(manager);
    } else if (g_str_equal(spec->name, "img-aspect-ratio")) {
        entangle_camera_manager_update_aspect_ratio(manager);
    } else if (g_str_equal(spec->name, "img-mask-opacity")) {
        entangle_camera_manager_update_mask_opacity(manager);
    } else if (g_str_equal(spec->name, "img-mask-enabled")) {
        entangle_camera_manager_update_mask_enabled(manager);
    } else if (g_str_equal(spec->name, "img-overexposure-highlighting")) {
        entangle_camera_manager_update_overexposure_highlighting(manager);
    } else if (g_str_equal(spec->name, "img-flip-vertically")) {
        entangle_camera_manager_update_flip_vertically(manager);
    } else if (g_str_equal(spec->name, "img-flip-horizontally")) {
        entangle_camera_manager_update_flip_horizontally(manager);
    } else if (g_str_equal(spec->name, "img-focus-point") ||
               g_str_equal(spec->name, "img-grid-lines")) {
        entangle_camera_manager_update_viewfinder(manager);
    } else if (g_str_equal(spec->name, "img-embedded-preview")) {
        entangle_camera_manager_update_image_loader(manager);
    } else if (g_str_equal(spec->name, "capture-delete-file")) {
        entangle_camera_manager_update_automata(manager);
    } else if (g_str_equal(spec->name, "img-onion-skin") ||
               g_str_equal(spec->name, "img-onion-layers")) {
        do_select_media(manager, manager->currentMedia);
    } else if (g_str_equal(spec->name, "img-background") ||
               g_str_equal(spec->name, "img-highlight")) {
        do_select_media(manager, manager->currentMedia);

        entangle_camera_manager_update_background_highlight(manager);
    }
}

static void
do_capture_widget_sensitivity(EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    GAction *actionCapture;
    GAction *actionPreview;
    GAction *actionCancel;

    GAction *actionSession;
    GAction *actionConnect;
    GAction *actionDisconnect;

    actionCapture =
        g_action_map_lookup_action(G_ACTION_MAP(manager), "session");
    actionPreview =
        g_action_map_lookup_action(G_ACTION_MAP(manager), "preview");
    actionCancel = g_action_map_lookup_action(G_ACTION_MAP(manager), "cancel");

    actionSession =
        g_action_map_lookup_action(G_ACTION_MAP(manager), "session");
    actionConnect =
        g_action_map_lookup_action(G_ACTION_MAP(manager), "connect");
    actionDisconnect =
        g_action_map_lookup_action(G_ACTION_MAP(manager), "disconnect");

    gtk_widget_set_sensitive(
        GTK_WIDGET(manager->titlebarButtonCapture),
        manager->cameraReady && !manager->taskCapture && manager->camera &&
                entangle_camera_get_has_capture(manager->camera)
            ? TRUE
            : FALSE);
    gtk_widget_set_sensitive(
        GTK_WIDGET(manager->titlebarButtonPreview),
        manager->cameraReady && !manager->taskCapture && manager->camera &&
                entangle_camera_get_has_capture(manager->camera) &&
                entangle_camera_get_has_preview(manager->camera) &&
                !manager->taskCapture
            ? TRUE
            : FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(manager->titlebarButtonCancel),
                             manager->taskCapture);

    gtk_widget_set_sensitive(
        GTK_WIDGET(manager->toolbarButtonCapture),
        gtk_widget_get_sensitive(GTK_WIDGET(manager->titlebarButtonCapture)));
    gtk_widget_set_sensitive(
        GTK_WIDGET(manager->toolbarButtonPreview),
        gtk_widget_get_sensitive(GTK_WIDGET(manager->titlebarButtonPreview)));
    gtk_widget_set_sensitive(
        GTK_WIDGET(manager->toolbarButtonCancel),
        gtk_widget_get_sensitive(GTK_WIDGET(manager->titlebarButtonCancel)));

    g_simple_action_set_enabled(
        G_SIMPLE_ACTION(actionCapture),
        gtk_widget_get_sensitive(GTK_WIDGET(manager->titlebarButtonCapture)));
    g_simple_action_set_enabled(
        G_SIMPLE_ACTION(actionPreview),
        gtk_widget_get_sensitive(GTK_WIDGET(manager->titlebarButtonPreview)));
    g_simple_action_set_enabled(
        G_SIMPLE_ACTION(actionCancel),
        gtk_widget_get_sensitive(GTK_WIDGET(manager->titlebarButtonCancel)));

    g_simple_action_set_enabled(G_SIMPLE_ACTION(actionSession),
                                !manager->taskActive);
    g_simple_action_set_enabled(G_SIMPLE_ACTION(actionConnect),
                                manager->camera ? FALSE : TRUE);
    g_simple_action_set_enabled(
        G_SIMPLE_ACTION(actionDisconnect),
        manager->camera && manager->cameraReady ? TRUE : FALSE);

    gtk_widget_set_tooltip_text(GTK_WIDGET(manager->titlebarButtonCapture),
                                _("Capture an image"));
    gtk_widget_set_tooltip_text(GTK_WIDGET(manager->titlebarButtonPreview),
                                _("Continuous capture preview"));
    gtk_widget_set_tooltip_text(GTK_WIDGET(manager->toolbarButtonCapture),
                                _("Capture an image"));
    gtk_widget_set_tooltip_text(GTK_WIDGET(manager->toolbarButtonPreview),
                                _("Continuous capture preview"));

    if (manager->camera) {
        if (!entangle_camera_get_has_capture(manager->camera)) {
            gtk_widget_set_tooltip_text(
                GTK_WIDGET(manager->titlebarButtonCapture),
                _("This camera does not support image capture"));
            gtk_widget_set_tooltip_text(
                GTK_WIDGET(manager->toolbarButtonCapture),
                _("This camera does not support image capture"));
        }
        if (!entangle_camera_get_has_capture(manager->camera) ||
            !entangle_camera_get_has_preview(manager->camera)) {
            gtk_widget_set_tooltip_text(
                GTK_WIDGET(manager->titlebarButtonPreview),
                _("This camera does not support image preview"));
            gtk_widget_set_tooltip_text(
                GTK_WIDGET(manager->toolbarButtonPreview),
                _("This camera does not support image preview"));
        }
    }

    entangle_camera_manager_update_viewfinder(manager);
}

static void
do_restore_scroll(GtkWidget *widget,
                  GdkRectangle *allocation G_GNUC_UNUSED,
                  EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));
    GtkAdjustment *hadjust;
    GtkAdjustment *vadjust;

    if (!entangle_image_display_get_loaded(ENTANGLE_IMAGE_DISPLAY(widget)))
        return;

    hadjust = gtk_scrolled_window_get_hadjustment(
        GTK_SCROLLED_WINDOW(manager->imageScroll));
    vadjust = gtk_scrolled_window_get_vadjustment(
        GTK_SCROLLED_WINDOW(manager->imageScroll));

    gtk_adjustment_set_value(hadjust, manager->imageScrollHOffset);
    gtk_adjustment_set_value(vadjust, manager->imageScrollVOffset);
    manager->imageScrollRestored = TRUE;
}

static void
do_select_image(EntangleCameraManager *manager, EntangleImage *image)
{
    GList *newimages = NULL;
    GList *oldimages;
    GList *tmp;
    GtkAdjustment *hadjust;
    GtkAdjustment *vadjust;
    EntanglePreferences *prefs =
        entangle_camera_manager_get_preferences(manager);

    if (image) {
        if (entangle_preferences_img_get_onion_skin(prefs)) {
            newimages = entangle_session_browser_get_earlier_images(
                manager->sessionBrowser,
                entangle_media_get_filename(ENTANGLE_MEDIA(image)) == NULL,
                entangle_preferences_img_get_onion_layers(prefs));
        }
        newimages = g_list_prepend(newimages, g_object_ref(image));
    }

    /* Load all new media first */
    tmp = newimages;
    while (tmp) {
        EntangleMedia *thismedia = ENTANGLE_MEDIA(tmp->data);

        ENTANGLE_DEBUG("New media %p %s", thismedia,
                       entangle_media_get_filename(thismedia));

        if (entangle_media_get_filename(thismedia))
            entangle_pixbuf_loader_load(
                ENTANGLE_PIXBUF_LOADER(manager->imageLoader),
                ENTANGLE_IMAGE(thismedia));

        tmp = tmp->next;
    }

    /* Now unload old images */
    tmp = oldimages =
        entangle_image_display_get_image_list(manager->imageDisplay);
    while (tmp) {
        EntangleMedia *thismedia = ENTANGLE_MEDIA(tmp->data);

        ENTANGLE_DEBUG("Old %p %s", thismedia,
                       entangle_media_get_filename(thismedia));

        if (entangle_media_get_filename(thismedia))
            entangle_pixbuf_loader_unload(
                ENTANGLE_PIXBUF_LOADER(manager->imageLoader),
                ENTANGLE_IMAGE(thismedia));

        tmp = tmp->next;
    }

    hadjust = gtk_scrolled_window_get_hadjustment(
        GTK_SCROLLED_WINDOW(manager->imageScroll));
    vadjust = gtk_scrolled_window_get_vadjustment(
        GTK_SCROLLED_WINDOW(manager->imageScroll));

    if (manager->imageScrollRestored) {
        manager->imageScrollHOffset = gtk_adjustment_get_value(hadjust);
        manager->imageScrollVOffset = gtk_adjustment_get_value(vadjust);
        manager->imageScrollRestored = FALSE;
    }

    entangle_image_display_set_image_list(manager->imageDisplay, newimages);
    entangle_image_histogram_set_image(manager->imageHistogram, image);

    g_list_foreach(oldimages, (GFunc)g_object_unref, NULL);
    g_list_free(oldimages);
    g_list_foreach(newimages, (GFunc)g_object_unref, NULL);
    g_list_free(newimages);
}

static void
do_select_media(EntangleCameraManager *manager, EntangleMedia *media)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));
    g_return_if_fail(!media || ENTANGLE_IS_MEDIA(media));

    ENTANGLE_DEBUG("Selected media %p %s", media,
                   media ? entangle_media_get_filename(media) : "<none>");

    if (ENTANGLE_IS_IMAGE(media)) {
        do_select_image(manager, ENTANGLE_IMAGE(media));
    } else {
        entangle_image_histogram_set_image(manager->imageHistogram, NULL);
        entangle_image_display_set_image(manager->imageDisplay, NULL);
    }

    entangle_media_statusbar_set_media(manager->mediaStatusbar, media);
    if (manager->mediaPresentation)
        entangle_media_popup_set_media(manager->mediaPresentation, media);

    if (manager->currentMedia)
        g_object_unref(manager->currentMedia);
    manager->currentMedia = media;
    if (manager->currentMedia)
        g_object_ref(manager->currentMedia);
}

static void
do_camera_task_error(EntangleCameraManager *manager,
                     const char *label,
                     GError *error)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    GtkWidget *msg =
        gtk_message_dialog_new(GTK_WINDOW(manager), 0, GTK_MESSAGE_ERROR,
                               GTK_BUTTONS_OK, _("Operation: %s"), label);
    gtk_window_set_title(GTK_WINDOW(msg), _("Entangle: Operation failed"));
    if (error)
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(msg), "%s",
                                                 error->message);
    g_signal_connect_swapped(msg, "response", G_CALLBACK(gtk_widget_destroy),
                             msg);
    gtk_widget_show_all(msg);
}

static void
do_camera_process_events(EntangleCameraManager *manager);

static void
do_camera_load_controls_refresh_finish(GObject *source,
                                       GAsyncResult *result,
                                       gpointer data)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(data));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(data);
    EntangleCamera *camera = ENTANGLE_CAMERA(source);
    GError *error = NULL;

    if (!entangle_camera_load_controls_finish(camera, result, &error)) {
        GtkWidget *msg = gtk_message_dialog_new(
            GTK_WINDOW(manager), 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
            _("Camera load controls failed"));
        gtk_window_set_title(GTK_WINDOW(msg),
                             _("Entangle: Camera load controls failed"));
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(msg), "%s",
                                                 error->message);
        g_signal_connect_swapped(msg, "response",
                                 G_CALLBACK(gtk_widget_destroy), msg);
        gtk_widget_show_all(msg);

        g_error_free(error);
    }

    g_cancellable_reset(manager->monitorCancel);

    do_camera_process_events(manager);
}

static void
do_camera_process_events_finish(GObject *src,
                                GAsyncResult *result,
                                gpointer data)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(data));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(data);
    EntangleCamera *camera = ENTANGLE_CAMERA(src);
    GError *error = NULL;

    manager->taskProcessEvents = FALSE;

    if (!entangle_camera_process_events_finish(camera, result, &error)) {
        if (g_cancellable_is_cancelled(manager->monitorCancel)) {
            g_cancellable_reset(manager->monitorCancel);
        } else {
            do_camera_task_error(manager, _("Monitor"), error);
        }
        g_error_free(error);
        goto cleanup;
    }

    if (!manager->cameraReady) {
        manager->cameraReady = TRUE;
        entangle_camera_automata_set_camera(manager->automata, manager->camera);
        do_capture_widget_sensitivity(manager);
    }

    if (!manager->camera)
        goto cleanup;

    if (manager->cameraChanged) {
        manager->cameraChanged = FALSE;
        entangle_camera_load_controls_async(
            manager->camera, NULL, do_camera_load_controls_refresh_finish,
            manager);
    } else {
        do_camera_process_events(manager);
    }

cleanup:
    g_object_unref(manager);
}

static void
do_camera_process_events(EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    if (!manager->camera)
        return;

    if (manager->taskProcessEvents)
        return;

    g_object_ref(manager);
    entangle_camera_process_events_async(
        manager->camera, 500, manager->monitorCancel,
        do_camera_process_events_finish, manager);

    manager->taskProcessEvents = TRUE;
}

static gboolean
do_camera_task_begin(EntangleCameraManager *manager)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager), FALSE);

    if (manager->taskActive)
        return FALSE;

    g_cancellable_cancel(manager->monitorCancel);
    g_cancellable_reset(manager->taskConfirm);
    g_cancellable_reset(manager->taskCancel);
    manager->taskActive = TRUE;

    return TRUE;
}

static void
do_camera_task_complete(EntangleCameraManager *manager)
{
    manager->taskActive = manager->taskPreview = manager->taskCapture = FALSE;

    do_capture_widget_sensitivity(manager);

    g_cancellable_reset(manager->taskConfirm);
    g_cancellable_reset(manager->taskCancel);
    g_cancellable_reset(manager->monitorCancel);

    do_camera_process_events(manager);
}

static void
do_camera_file_preview(EntangleCamera *cam G_GNUC_UNUSED,
                       EntangleCameraFile *file,
                       void *data)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(data));
    g_return_if_fail(ENTANGLE_IS_CAMERA_FILE(file));

    EntangleCameraManager *manager = data;
    GdkPixbuf *pixbuf;
    GByteArray *bytes;
    GInputStream *is;
    EntangleImage *image = NULL;

    if (manager->taskPreview && manager->taskCancel &&
        !g_cancellable_is_cancelled(manager->taskCancel)) {
        ENTANGLE_DEBUG("File preview %p %p %p", cam, file, data);

        bytes = entangle_camera_file_get_data(file);
        is = g_memory_input_stream_new_from_data(bytes->data, bytes->len, NULL);

        pixbuf = gdk_pixbuf_new_from_stream(is, NULL, NULL);

        if (manager->taskCapture) {
            char *localpath =
                entangle_session_next_filename(manager->session, file);

            if (!localpath ||
                !entangle_camera_file_save_path(file, localpath, NULL)) {
                ENTANGLE_DEBUG("Failed save path");
            } else {
                ENTANGLE_DEBUG("Saved to %s", localpath);
                image = entangle_image_new_file(localpath);

                entangle_session_add_media(manager->session,
                                           ENTANGLE_MEDIA(image));
            }
            g_free(localpath);
            manager->taskCapture = FALSE;
        }
        if (!image)
            image = entangle_image_new_pixbuf(pixbuf);

        do_select_media(manager, ENTANGLE_MEDIA(image));

        g_object_unref(pixbuf);
        g_object_unref(is);
        g_object_unref(image);
    }
}

static void
do_entangle_camera_progress_start(EntangleProgress *iface,
                                  float target G_GNUC_UNUSED,
                                  const char *msg)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(iface));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(iface);

    if (manager->taskPreview &&
        !g_cancellable_is_cancelled(manager->taskConfirm))
        return;

    gtk_widget_set_tooltip_text(GTK_WIDGET(manager->toolbarStatus), msg);
    gtk_spinner_start(GTK_SPINNER(manager->toolbarStatus));
}

static void
do_entangle_camera_progress_update(EntangleProgress *iface G_GNUC_UNUSED,
                                   float current G_GNUC_UNUSED)
{
}

static void
do_entangle_camera_progress_stop(EntangleProgress *iface)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(iface));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(iface);

    if (manager->taskPreview &&
        !g_cancellable_is_cancelled(manager->taskConfirm))
        return;

    gtk_widget_set_tooltip_text(GTK_WIDGET(manager->toolbarStatus), "");
    gtk_spinner_stop(GTK_SPINNER(manager->toolbarStatus));
}

static void
entangle_camera_progress_interface_init(gpointer g_iface,
                                        gpointer iface_data G_GNUC_UNUSED)
{
    EntangleProgressInterface *iface = g_iface;
    iface->start = do_entangle_camera_progress_start;
    iface->update = do_entangle_camera_progress_update;
    iface->stop = do_entangle_camera_progress_stop;
}

static void
do_remove_camera(EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    g_cancellable_cancel(manager->monitorCancel);
    g_cancellable_cancel(manager->taskCancel);

    gtk_window_set_title(GTK_WINDOW(manager), _("No camera connected"));
    gtk_header_bar_set_title(manager->toolbarBox, _("No camera connected"));

    entangle_camera_preferences_set_camera(manager->cameraPrefs, NULL);
    entangle_camera_set_progress(manager->camera, NULL);

    g_signal_handler_disconnect(manager->camera, manager->sigFilePreview);

    entangle_camera_automata_set_camera(manager->automata, NULL);

    if (manager->mediaPresentation) {
        gtk_widget_hide(GTK_WIDGET(manager->mediaPresentation));
        g_object_unref(manager->mediaPresentation);
        manager->mediaPresentation = NULL;
    }
}

static void
do_camera_load_controls_finish(GObject *source,
                               GAsyncResult *result,
                               gpointer data)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(data));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(data);
    gint64 epochsecs = g_get_real_time() / (1000 * 1000);
    EntanglePreferences *prefs =
        entangle_camera_manager_get_preferences(manager);
    EntangleCamera *cam = ENTANGLE_CAMERA(source);
    GError *error = NULL;

    if (entangle_camera_load_controls_finish(cam, result, &error)) {
        do_capture_widget_sensitivity(manager);
        entangle_camera_preferences_set_camera(manager->cameraPrefs,
                                               manager->camera);
    } else {
        GtkWidget *msg = gtk_message_dialog_new(
            GTK_WINDOW(manager), 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
            _("Camera load controls failed"));
        gtk_window_set_title(GTK_WINDOW(msg),
                             _("Entangle: Camera load controls failed"));
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(msg), "%s",
                                                 error->message);
        g_signal_connect_swapped(msg, "response",
                                 G_CALLBACK(gtk_widget_destroy), msg);
        gtk_widget_show_all(msg);

        g_error_free(error);
    }

    g_cancellable_reset(manager->monitorCancel);

    if (manager->camera && 0 &&
        entangle_preferences_capture_get_sync_clock(prefs))
        entangle_camera_set_clock_async(manager->camera, epochsecs, NULL,
                                        do_camera_sync_clock_finish, manager);

    do_camera_process_events(manager);
}

static void
do_camera_open_finish(GObject *source, GAsyncResult *result, gpointer data)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(data));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(data);
    EntangleCamera *cam = ENTANGLE_CAMERA(source);
    GError *error = NULL;

    if (entangle_camera_open_finish(cam, result, &error)) {
        entangle_camera_load_controls_async(
            manager->camera, NULL, do_camera_load_controls_finish, manager);
    } else {
        int response;
        GtkWidget *msg = gtk_message_dialog_new(
            GTK_WINDOW(manager), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
            GTK_BUTTONS_NONE, _("Unable to connect to camera: %s"),
            error->message);

        gtk_message_dialog_format_secondary_markup(
            GTK_MESSAGE_DIALOG(msg), "%s",
            _("Check that the camera is not\n\n"
              " - opened by another photo <b>application</b>\n"
              " - mounted as a <b>filesystem</b> on the desktop\n"
              " - in <b>sleep mode</b> to save battery power\n"));

        gtk_dialog_add_button(GTK_DIALOG(msg), _("Cancel"),
                              GTK_RESPONSE_CANCEL);
        gtk_dialog_add_button(GTK_DIALOG(msg), _("Retry"), GTK_RESPONSE_ACCEPT);
        gtk_dialog_set_default_response(GTK_DIALOG(msg), GTK_RESPONSE_ACCEPT);
        response = gtk_dialog_run(GTK_DIALOG(msg));

        gtk_widget_destroy(msg);

        if (response == GTK_RESPONSE_CANCEL) {
            entangle_camera_manager_set_camera(manager, NULL);
        } else {
            entangle_camera_open_async(cam, NULL, do_camera_open_finish,
                                       manager);
        }
        g_error_free(error);
    }
}

static gboolean
need_camera_unmount(EntangleCameraManager *manager, EntangleCamera *cam)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA(cam), FALSE);

    if (entangle_camera_is_mounted(cam)) {
        int response;
        GtkWidget *msg = gtk_message_dialog_new(
            GTK_WINDOW(manager), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
            GTK_BUTTONS_NONE, "%s", _("Camera is in use"));

        gtk_message_dialog_format_secondary_markup(
            GTK_MESSAGE_DIALOG(msg), "%s",
            _("The camera cannot be opened because it is mounted as a "
              "filesystem. Do you wish to umount it now?"));

        gtk_dialog_add_button(GTK_DIALOG(msg), _("No"), GTK_RESPONSE_NO);
        gtk_dialog_add_button(GTK_DIALOG(msg), _("Yes"), GTK_RESPONSE_YES);
        gtk_dialog_set_default_response(GTK_DIALOG(msg), GTK_RESPONSE_YES);

        response = gtk_dialog_run(GTK_DIALOG(msg));

        gtk_widget_destroy(msg);

        if (response == GTK_RESPONSE_YES)
            return TRUE;
    }
    return FALSE;
}

static void
do_camera_unmount_finish(GObject *source, GAsyncResult *result, gpointer data)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(data));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(data);
    EntangleCamera *cam = ENTANGLE_CAMERA(source);
    GError *error = NULL;

    if (entangle_camera_unmount_finish(cam, result, &error)) {
        entangle_camera_open_async(manager->camera, NULL, do_camera_open_finish,
                                   manager);
    } else {
        GtkWidget *msg =
            gtk_message_dialog_new(GTK_WINDOW(manager), 0, GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_OK, _("Camera connect failed"));
        gtk_window_set_title(GTK_WINDOW(msg),
                             _("Entangle: Camera connect failed"));
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(msg), "%s",
                                                 error->message);
        g_signal_connect_swapped(msg, "response",
                                 G_CALLBACK(gtk_widget_destroy), msg);
        gtk_widget_show_all(msg);
        entangle_camera_manager_set_camera(manager, NULL);

        g_error_free(error);
    }
}

static void
do_camera_control_changed(EntangleCamera *cam G_GNUC_UNUSED, gpointer data)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(data));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(data);

    manager->cameraChanged = TRUE;
}

static void
do_add_camera(EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    gtk_window_set_title(GTK_WINDOW(manager),
                         entangle_camera_get_model(manager->camera));
    gtk_header_bar_set_title(manager->toolbarBox,
                             entangle_camera_get_model(manager->camera));

    manager->sigFilePreview =
        g_signal_connect(manager->camera, "camera-file-previewed",
                         G_CALLBACK(do_camera_file_preview), manager);
    manager->sigChanged =
        g_signal_connect(manager->camera, "camera-controls-changed",
                         G_CALLBACK(do_camera_control_changed), manager);

    entangle_camera_set_progress(manager->camera, ENTANGLE_PROGRESS(manager));

    if (need_camera_unmount(manager, manager->camera)) {
        entangle_camera_unmount_async(manager->camera, NULL,
                                      do_camera_unmount_finish, manager);
    } else {
        entangle_camera_open_async(manager->camera, NULL, do_camera_open_finish,
                                   manager);
    }
}

static void
entangle_camera_manager_get_property(GObject *object,
                                     guint prop_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(object);

    switch (prop_id) {
    case PROP_CAMERA:
        g_value_set_object(value, manager->camera);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
do_camera_removed(EntangleCameraList *cameras G_GNUC_UNUSED,
                  EntangleCamera *camera,
                  gpointer data)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(data));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(data);

    if (manager->camera == camera)
        entangle_camera_manager_set_camera(manager, NULL);
}

static void
do_camera_manager_set_session(EntangleCameraManager *manager,
                              EntangleSession *session)
{
    if (manager->session) {
        entangle_camera_automata_set_session(manager->automata,
                                             manager->session);
        g_object_unref(manager->session);
        manager->session = NULL;
    }
    if (session) {
        EntanglePreferences *prefs =
            entangle_camera_manager_get_preferences(manager);
        manager->session = g_object_ref(session);
        entangle_session_load(manager->session);

        entangle_preferences_capture_set_last_session(
            prefs, entangle_session_directory(manager->session));
        entangle_camera_automata_set_session(manager->automata,
                                             manager->session);
    }
    entangle_session_browser_set_session(manager->sessionBrowser,
                                         manager->session);
}

static void
do_entangle_camera_manager_set_app(GObject *object,
                                   GParamSpec *spec G_GNUC_UNUSED)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(object));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(object);
    EntangleCameraList *cameras;
    gchar *directory;
    gchar *pattern;
    EntangleApplication *app;
    EntangleSession *session;

    app = ENTANGLE_APPLICATION(gtk_window_get_application(GTK_WINDOW(manager)));

    manager->prefs = g_object_ref(entangle_application_get_preferences(app));
    manager->sigPrefsNotify = g_signal_connect(
        manager->prefs, "notify",
        G_CALLBACK(entangle_camera_manager_prefs_changed), manager);
    cameras = entangle_application_get_active_cameras(app);
    g_signal_connect(cameras, "camera-removed", G_CALLBACK(do_camera_removed),
                     manager);
    directory = entangle_preferences_capture_get_last_session(manager->prefs);
    pattern = entangle_preferences_capture_get_filename_pattern(manager->prefs);

    entangle_camera_manager_update_histogram_linear(manager);
    entangle_camera_manager_update_colour_transform(manager);
    entangle_camera_manager_update_aspect_ratio(manager);
    entangle_camera_manager_update_mask_opacity(manager);
    entangle_camera_manager_update_mask_enabled(manager);
    entangle_camera_manager_update_overexposure_highlighting(manager);
    entangle_camera_manager_update_flip_vertically(manager);
    entangle_camera_manager_update_flip_horizontally(manager);
    entangle_camera_manager_update_image_loader(manager);
    entangle_camera_manager_update_background_highlight(manager);
    entangle_camera_manager_update_automata(manager);

    session = entangle_session_new(directory, pattern);
    do_camera_manager_set_session(manager, session);
    g_object_unref(session);

    g_free(directory);
    g_free(pattern);
}

static void
entangle_camera_manager_set_property(GObject *object,
                                     guint prop_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(object);

    ENTANGLE_DEBUG("Set prop %d", prop_id);

    switch (prop_id) {
    case PROP_CAMERA:
        entangle_camera_manager_set_camera(manager, g_value_get_object(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
entangle_camera_manager_finalize(GObject *object)
{
    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(object);

    ENTANGLE_DEBUG("Finalize manager");

    g_free(manager->deleteImageDup);

    if (manager->sessionBrowserMenu) {
        gtk_widget_destroy(GTK_WIDGET(manager->sessionBrowserMenu));
        manager->sessionBrowserMenu = NULL;
    }

    g_object_unref(manager->monitorCancel);
    g_object_unref(manager->taskCancel);
    g_object_unref(manager->taskConfirm);

    if (manager->sigPrefsNotify)
        g_signal_handler_disconnect(manager->prefs, manager->sigPrefsNotify);
    g_object_unref(manager->prefs);
    if (manager->imageDrawerTimer)
        g_source_remove(manager->imageDrawerTimer);
    if (manager->imageLoader)
        g_object_unref(manager->imageLoader);
    if (manager->thumbLoader)
        g_object_unref(manager->thumbLoader);
    if (manager->colourTransform)
        g_object_unref(manager->colourTransform);
    if (manager->camera)
        g_object_unref(manager->camera);
    if (manager->picker)
        gtk_widget_destroy(GTK_WIDGET(manager->picker));
    if (manager->mediaPresentation)
        gtk_widget_destroy(GTK_WIDGET(manager->mediaPresentation));

    g_hash_table_destroy(manager->popups);

    g_object_unref(manager->automata);

    G_OBJECT_CLASS(entangle_camera_manager_parent_class)->finalize(object);
}

static void
entangle_camera_manager_class_init(EntangleCameraManagerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = entangle_camera_manager_finalize;
    object_class->get_property = entangle_camera_manager_get_property;
    object_class->set_property = entangle_camera_manager_set_property;

    g_signal_new("closed", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST, 0,
                 NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

    g_object_class_install_property(
        object_class, PROP_CAMERA,
        g_param_spec_object("camera", "Camera", "Camera to be managed",
                            ENTANGLE_TYPE_CAMERA,
                            G_PARAM_READWRITE | G_PARAM_STATIC_NAME |
                                G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

    gtk_widget_class_set_template_from_resource(
        GTK_WIDGET_CLASS(klass),
        "/org/entangle_photo/Manager/frontend/entangle-camera-manager.ui");

    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass),
                                         EntangleCameraManager, winBox);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass),
                                         EntangleCameraManager, displayPanel);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass),
                                         EntangleCameraManager, winMainpane);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass),
                                         EntangleCameraManager, settingsScroll);
    gtk_widget_class_bind_template_child(
        GTK_WIDGET_CLASS(klass), EntangleCameraManager, settingsViewport);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass),
                                         EntangleCameraManager, toolbarBox);
    gtk_widget_class_bind_template_child(
        GTK_WIDGET_CLASS(klass), EntangleCameraManager, toolbarButtonCapture);
    gtk_widget_class_bind_template_child(
        GTK_WIDGET_CLASS(klass), EntangleCameraManager, toolbarButtonPreview);
    gtk_widget_class_bind_template_child(
        GTK_WIDGET_CLASS(klass), EntangleCameraManager, toolbarButtonCancel);
    gtk_widget_class_bind_template_child(
        GTK_WIDGET_CLASS(klass), EntangleCameraManager, toolbarButtonAction);
    gtk_widget_class_bind_template_child(
        GTK_WIDGET_CLASS(klass), EntangleCameraManager, toolbarButtonZoom);
    gtk_widget_class_bind_template_child(
        GTK_WIDGET_CLASS(klass), EntangleCameraManager, toolbarButtonView);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass),
                                         EntangleCameraManager, toolbarStatus);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass),
                                         EntangleCameraManager, titlebarBox);
    gtk_widget_class_bind_template_child(
        GTK_WIDGET_CLASS(klass), EntangleCameraManager, titlebarButtonCapture);
    gtk_widget_class_bind_template_child(
        GTK_WIDGET_CLASS(klass), EntangleCameraManager, titlebarButtonPreview);
    gtk_widget_class_bind_template_child(
        GTK_WIDGET_CLASS(klass), EntangleCameraManager, titlebarButtonCancel);
    gtk_widget_class_bind_template_child(
        GTK_WIDGET_CLASS(klass), EntangleCameraManager, titlebarButtonAction);
    gtk_widget_class_bind_template_child(
        GTK_WIDGET_CLASS(klass), EntangleCameraManager, titlebarButtonZoom);
    gtk_widget_class_bind_template_child(
        GTK_WIDGET_CLASS(klass), EntangleCameraManager, titlebarButtonView);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass),
                                         EntangleCameraManager, titlebarStatus);
}

static void
do_action_session(GSimpleAction *act G_GNUC_UNUSED,
                  GVariant *param G_GNUC_UNUSED,
                  gpointer opaque)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(opaque));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(opaque);
    GtkWidget *chooser;
    gchar *dir;
    EntanglePreferences *prefs =
        entangle_camera_manager_get_preferences(manager);

    chooser = gtk_file_chooser_dialog_new(
        _("Select a folder"), GTK_WINDOW(manager),
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, _("_Cancel"),
        GTK_RESPONSE_REJECT, _("_Open"), GTK_RESPONSE_ACCEPT, NULL);
    gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(chooser), TRUE);
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(chooser), FALSE);

    dir = entangle_preferences_capture_get_last_session(prefs);
    g_mkdir_with_parents(dir, 0777);

    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), dir);
    g_free(dir);

    gtk_widget_hide(chooser);

    if (gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
        EntangleSession *session;
        gchar *pattern;
        do_select_media(manager, NULL);
        dir = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
        pattern = entangle_preferences_capture_get_filename_pattern(prefs);
        session = entangle_session_new(dir, pattern);
        g_free(dir);
        g_free(pattern);
        g_hash_table_remove_all(manager->popups);
        do_camera_manager_set_session(manager, session);
        g_object_unref(session);
    }

    gtk_widget_destroy(chooser);
}

static void
do_action_settings(GSimpleAction *act, GVariant *state, gpointer opaque)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(opaque));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(opaque);
    gboolean visible;

    visible = g_variant_get_boolean(state);
    g_simple_action_set_state(act, g_variant_new_boolean(visible));

    if (visible)
        gtk_widget_show(GTK_WIDGET(manager->settingsScroll));
    else
        gtk_widget_hide(GTK_WIDGET(manager->settingsScroll));
}

static void
do_action_cancel(GSimpleAction *act G_GNUC_UNUSED,
                 GVariant *param G_GNUC_UNUSED,
                 gpointer opaque)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(opaque));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(opaque);

    if (manager->taskCancel)
        g_cancellable_cancel(manager->taskCancel);
}

static void
do_camera_manager_capture(EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    EntangleApplication *app =
        ENTANGLE_APPLICATION(gtk_window_get_application(GTK_WINDOW(manager)));

    if (manager->syncCapture) {
        GList *windows = gtk_application_get_windows(GTK_APPLICATION(app));

        while (windows) {
            GtkWindow *window = windows->data;

            if (ENTANGLE_IS_CAMERA_MANAGER(window))
                entangle_camera_manager_capture(
                    ENTANGLE_CAMERA_MANAGER(window));

            windows = windows->next;
        }
    } else {
        entangle_camera_manager_capture(manager);
    }
}

static void
do_camera_manager_preview_begin(EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    EntangleApplication *app =
        ENTANGLE_APPLICATION(gtk_window_get_application(GTK_WINDOW(manager)));

    if (manager->syncCapture) {
        GList *windows = gtk_application_get_windows(GTK_APPLICATION(app));

        while (windows) {
            GtkWindow *window = windows->data;
            if (ENTANGLE_IS_CAMERA_MANAGER(window))
                entangle_camera_manager_preview_begin(
                    ENTANGLE_CAMERA_MANAGER(window));

            windows = windows->next;
        }
    } else {
        entangle_camera_manager_preview_begin(manager);
    }
}

static void
do_camera_manager_preview_cancel(EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    EntangleApplication *app =
        ENTANGLE_APPLICATION(gtk_window_get_application(GTK_WINDOW(manager)));

    if (manager->syncCapture) {
        GList *windows = gtk_application_get_windows(GTK_APPLICATION(app));

        while (windows) {
            GtkWindow *window = windows->data;
            if (ENTANGLE_IS_CAMERA_MANAGER(window))
                entangle_camera_manager_preview_cancel(
                    ENTANGLE_CAMERA_MANAGER(window));

            windows = windows->next;
        }
    } else {
        entangle_camera_manager_preview_cancel(manager);
    }
}

static void
do_entangle_camera_manager_capture_finish(GObject *src,
                                          GAsyncResult *res,
                                          gpointer opaque)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_AUTOMATA(src));
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(opaque));

    EntangleCameraAutomata *automata = ENTANGLE_CAMERA_AUTOMATA(src);
    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(opaque);
    GError *error = NULL;

    if (!entangle_camera_automata_capture_finish(automata, res, &error)) {
        do_camera_task_error(manager, _("Capture"), error);
        g_error_free(error);
    }

    do_camera_task_complete(manager);
}

static void
do_entangle_camera_manager_script_finish(GObject *src,
                                         GAsyncResult *res,
                                         gpointer opaque)
{
    g_return_if_fail(ENTANGLE_IS_SCRIPT(src));
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(opaque));
    EntangleScript *script = ENTANGLE_SCRIPT(src);
    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(opaque);
    GError *error = NULL;

    if (!entangle_script_execute_finish(script, res, &error)) {
        do_camera_task_error(manager, _("Script"), error);
        if (error)
            g_error_free(error);
    }

    do_camera_task_complete(manager);
    g_object_unref(script);
}

void
entangle_camera_manager_capture(EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    EntanglePreferences *prefs =
        entangle_camera_manager_get_preferences(manager);
    ENTANGLE_DEBUG("starting capture operation");

    if (!manager->camera)
        return;

    if (manager->taskPreview) {
        if (entangle_preferences_capture_get_continuous_preview(prefs) &&
            entangle_preferences_capture_get_electronic_shutter(prefs))
            manager->taskCapture = TRUE;
        else
            g_cancellable_cancel(manager->taskConfirm);
    } else {
        EntangleScript *script;
        if (!do_camera_task_begin(manager))
            return;

        manager->taskCapture = TRUE;
        do_capture_widget_sensitivity(manager);

        script = entangle_script_config_get_selected(manager->scriptConfig);
        if (script)
            entangle_script_execute_async(
                script, manager->automata, manager->taskCancel,
                do_entangle_camera_manager_script_finish, manager);
        else
            entangle_camera_automata_capture_async(
                manager->automata, manager->taskCancel,
                do_entangle_camera_manager_capture_finish, manager);
    }
}

static void
do_entangle_camera_manager_preview_finish(GObject *src,
                                          GAsyncResult *res,
                                          gpointer opaque)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_AUTOMATA(src));
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(opaque));

    EntangleCameraAutomata *automata = ENTANGLE_CAMERA_AUTOMATA(src);
    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(opaque);
    EntanglePreferences *prefs =
        entangle_camera_manager_get_preferences(manager);
    GError *error = NULL;

    if (!entangle_camera_automata_preview_finish(automata, res, &error)) {
        do_camera_task_error(manager, _("Preview"), error);
    }

    if (!error && manager->camera &&
        !g_cancellable_is_cancelled(manager->taskCancel) &&
        entangle_preferences_capture_get_continuous_preview(prefs)) {
        entangle_camera_automata_preview_async(
            manager->automata, manager->taskCancel, manager->taskConfirm,
            do_entangle_camera_manager_preview_finish, manager);
    } else {
        gtk_toggle_button_set_active(manager->titlebarButtonPreview, FALSE);
        gtk_toggle_button_set_active(manager->toolbarButtonPreview, FALSE);

        do_camera_task_complete(manager);
        if (error)
            g_error_free(error);
    }
}

void
entangle_camera_manager_preview_begin(EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    if (!manager->camera)
        return;

    ENTANGLE_DEBUG("starting preview operation");
    if (!do_camera_task_begin(manager))
        return;

    manager->taskPreview = TRUE;
    do_capture_widget_sensitivity(manager);

    entangle_camera_automata_preview_async(
        manager->automata, manager->taskCancel, manager->taskConfirm,
        do_entangle_camera_manager_preview_finish, manager);
}

void
entangle_camera_manager_preview_cancel(EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    EntangleMedia *media;

    if (!manager->camera)
        return;

    if (manager->taskPreview) {
        ENTANGLE_DEBUG("Cancelling capture operation");
        g_cancellable_cancel(manager->taskCancel);

        media = entangle_session_browser_get_selected_media(
            manager->sessionBrowser);
        do_select_media(manager, media);
        if (media) {
            g_object_unref(media);
        }
    }
}

static void
do_action_capture(GSimpleAction *act G_GNUC_UNUSED,
                  GVariant *param G_GNUC_UNUSED,
                  gpointer opaque)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(opaque));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(opaque);

    do_camera_manager_capture(manager);
}

static void
do_action_preview(GSimpleAction *act G_GNUC_UNUSED,
                  GVariant *param G_GNUC_UNUSED,
                  gpointer opaque)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(opaque));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(opaque);

    if (!manager->taskPreview) {
        gtk_toggle_button_set_active(manager->titlebarButtonPreview, True);
        gtk_toggle_button_set_active(manager->toolbarButtonPreview, True);
        do_camera_manager_preview_begin(manager);
    } else {
        gtk_toggle_button_set_active(manager->titlebarButtonPreview, False);
        gtk_toggle_button_set_active(manager->toolbarButtonPreview, False);
        do_camera_manager_preview_cancel(manager);
    }
}

static void
do_camera_sync_clock_finish(GObject *src, GAsyncResult *res, gpointer opaque)
{
    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(opaque);
    EntangleCamera *camera = ENTANGLE_CAMERA(src);
    GError *error = NULL;

    if (!entangle_camera_set_clock_finish(camera, res, &error)) {
        do_camera_task_error(manager, _("Set clock"), error);
        g_error_free(error);
        return;
    }
}

static void
do_action_sync_clock(GSimpleAction *act G_GNUC_UNUSED,
                     GVariant *param G_GNUC_UNUSED,
                     gpointer opaque)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(opaque));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(opaque);
    gint64 epochsecs = g_get_real_time() / (1000 * 1000);

    ENTANGLE_DEBUG("starting sync clock operation");

    if (!manager->camera)
        return;

    entangle_camera_set_clock_async(manager->camera, epochsecs, NULL,
                                    do_camera_sync_clock_finish, manager);
}

static void
do_camera_autofocus_finish(GObject *source,
                           GAsyncResult *result,
                           gpointer opaque)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(opaque));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(opaque);
    EntangleCamera *camera = ENTANGLE_CAMERA(source);
    GError *error = NULL;

    if (!entangle_camera_autofocus_finish(camera, result, &error)) {
        GtkWidget *msg =
            gtk_message_dialog_new(GTK_WINDOW(manager), 0, GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_OK, _("Autofocus failed"));
        gtk_window_set_title(GTK_WINDOW(msg),
                             _("Entangle: Camera autofocus failed"));
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(msg), "%s",
                                                 error->message);
        g_signal_connect_swapped(msg, "response",
                                 G_CALLBACK(gtk_widget_destroy), msg);
        gtk_widget_show_all(msg);

        g_error_free(error);
    }
}

static void
do_camera_manualfocus_finish(GObject *source,
                             GAsyncResult *result,
                             gpointer opaque)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(opaque));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(opaque);
    EntangleCamera *camera = ENTANGLE_CAMERA(source);
    GError *error = NULL;

    if (!entangle_camera_manualfocus_finish(camera, result, &error)) {
        GtkWidget *msg =
            gtk_message_dialog_new(GTK_WINDOW(manager), 0, GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_OK, _("Manual focus failed"));
        gtk_window_set_title(GTK_WINDOW(msg),
                             _("Entangle: Camera manual focus failed"));
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(msg), "%s",
                                                 error->message);
        g_signal_connect_swapped(msg, "response",
                                 G_CALLBACK(gtk_widget_destroy), msg);
        gtk_widget_show_all(msg);

        g_error_free(error);
    }
}

gboolean
do_manager_key_release(GtkWidget *widget G_GNUC_UNUSED,
                       GdkEventKey *ev,
                       gpointer data)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA_MANAGER(data), FALSE);

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(data);

    switch (ev->keyval) {
    case GDK_KEY_m: {
        EntanglePreferences *prefs =
            entangle_camera_manager_get_preferences(manager);
        gboolean enabled = entangle_preferences_img_get_mask_enabled(prefs);
        entangle_preferences_img_set_mask_enabled(prefs, !enabled);
    } break;

    case GDK_KEY_o: {
        EntanglePreferences *prefs =
            entangle_camera_manager_get_preferences(manager);
        gboolean enabled =
            entangle_preferences_img_get_overexposure_highlighting(prefs);
        entangle_preferences_img_set_overexposure_highlighting(prefs, !enabled);
    } break;

    case GDK_KEY_h: {
        EntanglePreferences *prefs =
            entangle_camera_manager_get_preferences(manager);
        gboolean linear =
            entangle_preferences_interface_get_histogram_linear(prefs);
        entangle_preferences_interface_set_histogram_linear(prefs, !linear);
    } break;

    case GDK_KEY_a: {
        if (manager->taskPreview) {
            entangle_camera_autofocus_async(
                manager->camera, NULL, do_camera_autofocus_finish, manager);
        }
    } break;

    case GDK_KEY_comma: {
        if (manager->taskPreview) {
            entangle_camera_manualfocus_async(
                manager->camera, ENTANGLE_CAMERA_MANUAL_FOCUS_STEP_OUT_MEDIUM,
                NULL, do_camera_manualfocus_finish, manager);
        }
    } break;

    case GDK_KEY_period: {
        if (manager->taskPreview) {
            entangle_camera_manualfocus_async(
                manager->camera, ENTANGLE_CAMERA_MANUAL_FOCUS_STEP_IN_MEDIUM,
                NULL, do_camera_manualfocus_finish, manager);
        }
    } break;

    case GDK_KEY_less: {
        if (manager->taskPreview) {
            entangle_camera_manualfocus_async(
                manager->camera, ENTANGLE_CAMERA_MANUAL_FOCUS_STEP_OUT_COARSE,
                NULL, do_camera_manualfocus_finish, manager);
        }
    } break;

    case GDK_KEY_greater: {
        if (manager->taskPreview) {
            entangle_camera_manualfocus_async(
                manager->camera, ENTANGLE_CAMERA_MANUAL_FOCUS_STEP_IN_COARSE,
                NULL, do_camera_manualfocus_finish, manager);
        }
    } break;

    default:
        break;
    }

    return FALSE;
}

static void
do_zoom_widget_sensitivity(EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    GAction *actionzoomnormal;
    GAction *actionzoombest;
    GAction *actionzoomin;
    GAction *actionzoomout;
    gboolean autoscale;

    actionzoomnormal =
        g_action_map_lookup_action(G_ACTION_MAP(manager), "zoom-normal");
    actionzoombest =
        g_action_map_lookup_action(G_ACTION_MAP(manager), "zoom-best");
    actionzoomin = g_action_map_lookup_action(G_ACTION_MAP(manager), "zoom-in");
    actionzoomout =
        g_action_map_lookup_action(G_ACTION_MAP(manager), "zoom-out");

    autoscale = entangle_image_display_get_autoscale(manager->imageDisplay);

    if (autoscale) {
        g_simple_action_set_enabled(G_SIMPLE_ACTION(actionzoombest), FALSE);
        g_simple_action_set_enabled(G_SIMPLE_ACTION(actionzoomnormal), TRUE);
        g_simple_action_set_enabled(G_SIMPLE_ACTION(actionzoomin), FALSE);
        g_simple_action_set_enabled(G_SIMPLE_ACTION(actionzoomout), FALSE);
    } else {
        g_simple_action_set_enabled(G_SIMPLE_ACTION(actionzoombest), TRUE);
        g_simple_action_set_enabled(G_SIMPLE_ACTION(actionzoomnormal),
                                    manager->zoomLevel == 0 ? FALSE : TRUE);
        g_simple_action_set_enabled(G_SIMPLE_ACTION(actionzoomin),
                                    manager->zoomLevel == 10 ? FALSE : TRUE);
        g_simple_action_set_enabled(G_SIMPLE_ACTION(actionzoomout),
                                    manager->zoomLevel == -10 ? FALSE : TRUE);
    }
}

static void
entangle_camera_manager_update_zoom(EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    if (manager->zoomLevel > 0)
        entangle_image_display_set_scale(manager->imageDisplay,
                                         1.0 + manager->zoomLevel);
    else if (manager->zoomLevel < 0)
        entangle_image_display_set_scale(manager->imageDisplay,
                                         1.0 / pow(1.5, -manager->zoomLevel));
    else
        entangle_image_display_set_scale(manager->imageDisplay, 0.0);
    do_zoom_widget_sensitivity(manager);
}

static void
entangle_camera_manager_zoom_in(EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    if (manager->zoomLevel < 10)
        manager->zoomLevel += 1;

    entangle_camera_manager_update_zoom(manager);
}

static void
entangle_camera_manager_zoom_out(EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    if (manager->zoomLevel > -10)
        manager->zoomLevel -= 1;

    entangle_camera_manager_update_zoom(manager);
}

static void
entangle_camera_manager_zoom_normal(EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    manager->zoomLevel = 0;
    entangle_image_display_set_scale(manager->imageDisplay, 0.0);
    entangle_image_display_set_autoscale(manager->imageDisplay, FALSE);
    do_zoom_widget_sensitivity(manager);
}

static void
entangle_camera_manager_zoom_best(EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    manager->zoomLevel = 0;
    entangle_image_display_set_autoscale(manager->imageDisplay, TRUE);
    do_zoom_widget_sensitivity(manager);
}

static void
do_action_zoom_in(GSimpleAction *act G_GNUC_UNUSED,
                  GVariant *param G_GNUC_UNUSED,
                  gpointer opaque)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(opaque));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(opaque);

    entangle_camera_manager_zoom_in(manager);
}

static void
do_action_zoom_out(GSimpleAction *act G_GNUC_UNUSED,
                   GVariant *param G_GNUC_UNUSED,
                   gpointer opaque)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(opaque));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(opaque);

    entangle_camera_manager_zoom_out(manager);
}

static void
do_action_zoom_normal(GSimpleAction *act G_GNUC_UNUSED,
                      GVariant *param G_GNUC_UNUSED,
                      gpointer opaque)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(opaque));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(opaque);

    entangle_camera_manager_zoom_normal(manager);
}

static void
do_action_zoom_best(GSimpleAction *act G_GNUC_UNUSED,
                    GVariant *param G_GNUC_UNUSED,
                    gpointer opaque)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(opaque));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(opaque);

    entangle_camera_manager_zoom_best(manager);
}

static void
do_action_fullscreen(GSimpleAction *act, GVariant *state, gpointer opaque)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(opaque));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(opaque);
    gboolean fullscreen = g_variant_get_boolean(state);

    g_simple_action_set_state(act, g_variant_new_boolean(fullscreen));

    if (fullscreen) {
        gtk_widget_show(GTK_WIDGET(manager->toolbarBox));
        gtk_window_fullscreen(GTK_WINDOW(manager));
    } else {
        gtk_widget_hide(GTK_WIDGET(manager->toolbarBox));
        gtk_window_unfullscreen(GTK_WINDOW(manager));
    }
}

static void
do_presentation_end(EntangleMediaPopup *popup G_GNUC_UNUSED,
                    EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    GAction *actionPresentation;

    actionPresentation =
        g_action_map_lookup_action(G_ACTION_MAP(manager), "presentation");
    g_simple_action_set_state(G_SIMPLE_ACTION(actionPresentation),
                              g_variant_new_boolean(FALSE));
}

static void
do_action_presentation(GSimpleAction *act, GVariant *state, gpointer opaque)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(opaque));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(opaque);
    gboolean presentation;

    if (!manager->mediaPresentation) {
        EntanglePreferences *prefs =
            entangle_camera_manager_get_preferences(manager);
        gchar *bg = entangle_preferences_img_get_background(prefs);
        manager->mediaPresentation = entangle_media_popup_new();
        entangle_media_popup_set_background(manager->mediaPresentation, bg);
        g_signal_connect(manager->mediaPresentation, "popup-close",
                         G_CALLBACK(do_presentation_end), manager);
        g_free(bg);
    }

    presentation = g_variant_get_boolean(state);
    g_simple_action_set_state(act, g_variant_new_boolean(FALSE));
    if (presentation) {
        entangle_media_popup_set_media(manager->mediaPresentation,
                                       manager->currentMedia);
        entangle_media_popup_show_on_monitor(manager->mediaPresentation,
                                             manager->presentationMonitor);
    } else {
        gtk_widget_hide(GTK_WIDGET(manager->mediaPresentation));
    }
}

static void
do_action_close(GSimpleAction *act G_GNUC_UNUSED,
                GVariant *param G_GNUC_UNUSED,
                gpointer opaque)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(opaque));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(opaque);

    gtk_widget_destroy(GTK_WIDGET(manager));
}

static gboolean
do_manager_delete(GtkWidget *src G_GNUC_UNUSED,
                  GdkEvent *ev G_GNUC_UNUSED,
                  EntangleCameraManager *manager)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager), TRUE);

    gtk_widget_destroy(GTK_WIDGET(manager));
    return TRUE;
}

static void
do_picker_refresh(EntangleCameraPicker *picker G_GNUC_UNUSED,
                  EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    EntangleApplication *app =
        ENTANGLE_APPLICATION(gtk_window_get_application(GTK_WINDOW(manager)));
    EntangleCameraList *list = entangle_application_get_active_cameras(app);
    entangle_camera_list_refresh(list, NULL);
}

static void
do_picker_connect(EntangleCameraPicker *picker G_GNUC_UNUSED,
                  EntangleCamera *cam,
                  gpointer *data)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(data));
    g_return_if_fail(ENTANGLE_IS_CAMERA(cam));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(data);
#if 0
    GError *error = NULL;
    ENTANGLE_DEBUG("emit connect %p %s", cam, entangle_camera_get_model(cam));
    if (need_camera_unmount(manager, cam))
        do_camera_unmount(cam);

    while (!entangle_camera_open(cam, &error)) {
        int response;
        GtkWidget *msg = gtk_message_dialog_new(GTK_WINDOW(manager),
                                                GTK_DIALOG_MODAL,
                                                GTK_MESSAGE_ERROR,
                                                GTK_BUTTONS_NONE,
                                                _("Unable to connect to camera: %s"),
                                                error->message);
        g_error_free(error);

        gtk_message_dialog_format_secondary_markup(GTK_MESSAGE_DIALOG(msg),
                                                   "%s",
                                                   _("Check that the camera is not\n\n"
                                                     " - opened by another photo <b>application</b>\n"
                                                     " - mounted as a <b>filesystem</b> on the desktop\n"
                                                     " - in <b>sleep mode</b> to save battery power\n"));

        gtk_dialog_add_button(GTK_DIALOG(msg), _("Cancel"), GTK_RESPONSE_CANCEL);
        gtk_dialog_add_button(GTK_DIALOG(msg), _("Retry"), GTK_RESPONSE_ACCEPT);
        gtk_dialog_set_default_response(GTK_DIALOG(msg), GTK_RESPONSE_ACCEPT);

        response = gtk_dialog_run(GTK_DIALOG(msg));

        gtk_widget_destroy(msg);

        if (response == GTK_RESPONSE_CANCEL)
            return;
    }
#endif
    entangle_camera_manager_set_camera(manager, cam);
    gtk_widget_hide(GTK_WIDGET(manager->picker));
}

static void
do_camera_open(EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    EntangleApplication *app =
        ENTANGLE_APPLICATION(gtk_window_get_application(GTK_WINDOW(manager)));
    EntangleCameraList *cameras = entangle_application_get_active_cameras(app);

    if (!manager->picker) {
        manager->picker = entangle_camera_picker_new();
        entangle_camera_picker_set_camera_list(manager->picker, cameras);
        gtk_window_set_transient_for(GTK_WINDOW(manager->picker),
                                     GTK_WINDOW(manager));
        g_signal_connect(manager->picker, "picker-refresh",
                         G_CALLBACK(do_picker_refresh), manager);
        g_signal_connect(manager->picker, "picker-connect",
                         G_CALLBACK(do_picker_connect), manager);
    }

    gtk_widget_show(GTK_WIDGET(manager->picker));
}

static void
do_action_connect(GSimpleAction *act G_GNUC_UNUSED,
                  GVariant *param G_GNUC_UNUSED,
                  gpointer opaque)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(opaque));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(opaque);

    do_camera_open(manager);
}

static void
do_action_disconnect(GSimpleAction *act G_GNUC_UNUSED,
                     GVariant *param G_GNUC_UNUSED,
                     gpointer opaque)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(opaque));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(opaque);

    entangle_camera_manager_set_camera(manager, NULL);
}

static void
do_session_media_selected(GtkIconView *view G_GNUC_UNUSED, gpointer data)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(data));

    EntangleCameraManager *manager = data;
    EntangleMedia *media =
        entangle_session_browser_get_selected_media(manager->sessionBrowser);

    ENTANGLE_DEBUG("Media selection changed");
    do_select_media(manager, media);
    if (media)
        g_object_unref(media);
}

static void
do_popup_close(EntangleMediaPopup *popup, EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_MEDIA_POPUP(popup));
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    EntangleMedia *media = entangle_media_popup_get_media(popup);
    const char *filename = entangle_media_get_filename(media);

    g_hash_table_remove(manager->popups, filename);

    if (ENTANGLE_IS_IMAGE(media))
        entangle_pixbuf_loader_unload(
            ENTANGLE_PIXBUF_LOADER(manager->imageLoader),
            ENTANGLE_IMAGE(media));
}

static void
do_menu_session_tearoff_activate(GtkMenuItem *item,
                                 EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    GdkSeat *seat =
        gdk_display_get_default_seat(gtk_widget_get_display(GTK_WIDGET(item)));
    GdkDevice *dev = gdk_seat_get_pointer(seat);
    GdkScreen *screen;
    int x, y;
    const gchar *filename;
    EntangleMediaPopup *pol;
    EntanglePreferences *prefs;

    gdk_device_get_position(dev, &screen, &x, &y);

    if (!manager->sessionBrowserMedia)
        return;

    filename = entangle_media_get_filename(manager->sessionBrowserMedia);
    prefs = entangle_camera_manager_get_preferences(manager);
    if (!(pol = g_hash_table_lookup(manager->popups, filename))) {
        gchar *bg = entangle_preferences_img_get_background(prefs);
        pol = entangle_media_popup_new();
        entangle_media_popup_set_media(pol, manager->sessionBrowserMedia);
        entangle_media_popup_set_background(pol, bg);
        g_signal_connect(pol, "hide", G_CALLBACK(do_popup_close), manager);
        g_hash_table_insert(manager->popups, g_strdup(filename), pol);
        if (ENTANGLE_IS_IMAGE(manager->sessionBrowserMedia))
            entangle_pixbuf_loader_load(
                ENTANGLE_PIXBUF_LOADER(manager->imageLoader),
                ENTANGLE_IMAGE(manager->sessionBrowserMedia));
        g_free(bg);
    }
    ENTANGLE_DEBUG("Popup %p for %s", pol, filename);
    entangle_media_popup_show(pol, GTK_WINDOW(manager), x, y);
}

void
do_menu_session_open_activate(GtkMenuItem *item G_GNUC_UNUSED,
                              EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    gchar *ctype = NULL;
    GList *files = NULL;
    GAppInfo *info = NULL;
    const gchar *filename;

    if (!manager->sessionBrowserMedia)
        return;

    filename = entangle_media_get_filename(manager->sessionBrowserMedia);
    ctype = g_content_type_guess(filename, NULL, 0, NULL);
    if (!ctype)
        return;

    info = g_app_info_get_default_for_type(ctype, FALSE);
    g_free(ctype);
    if (!info)
        return;

    files = g_list_append(files, g_file_new_for_path(filename));

    g_app_info_launch(info, files, NULL, NULL);

    g_list_foreach(files, (GFunc)g_object_unref, NULL);
    g_list_free(files);
}

void
do_menu_session_delete_activate(GtkMenuItem *item G_GNUC_UNUSED,
                                EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    GError *error = NULL;

    if (!manager->sessionBrowserMedia)
        return;

    if (!entangle_media_delete(manager->sessionBrowserMedia, &error)) {
        do_camera_task_error(manager, _("Delete"), error);
        return;
    }
    entangle_session_remove_media(manager->session,
                                  manager->sessionBrowserMedia);
}

static void
do_session_browser_open_with_app(GtkMenuItem *src,
                                 EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    GAppInfo *info = g_object_get_data(G_OBJECT(src), "appinfo");
    GList *files = NULL;
    const gchar *filename;

    if (!info)
        return;

    if (!manager->sessionBrowserMedia)
        return;

    filename = entangle_media_get_filename(manager->sessionBrowserMedia);
    files = g_list_append(files, g_file_new_for_path(filename));

    g_app_info_launch(info, files, NULL, NULL);

    g_list_foreach(files, (GFunc)g_object_unref, NULL);
    g_list_free(files);
}

static void
do_session_browser_open_with_select(GtkMenuItem *src G_GNUC_UNUSED,
                                    EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    GtkDialog *dialog;
    GFile *file;
    GAppInfo *info;
    GList *files = NULL;

    if (!manager->sessionBrowserMedia)
        return;

    file = g_file_new_for_path(
        entangle_media_get_filename(manager->sessionBrowserMedia));
    dialog = GTK_DIALOG(gtk_app_chooser_dialog_new(
        GTK_WINDOW(manager), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        file));

    gtk_dialog_run(dialog);

    info = gtk_app_chooser_get_app_info(GTK_APP_CHOOSER(dialog));

    files = g_list_append(files, file);

    if (info)
        g_app_info_launch(info, files, NULL, NULL);

    g_list_foreach(files, (GFunc)g_object_unref, NULL);
    g_list_free(files);
    gtk_widget_destroy(GTK_WIDGET(dialog));
}

static gboolean
do_session_browser_popup(EntangleSessionBrowser *browser,
                         GdkEventButton *event,
                         EntangleCameraManager *manager)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager), FALSE);

    GtkMenuItem *tearoff;
    GtkMenuItem *open;
    GtkMenuItem *openWith;
    GtkSeparatorMenuItem *sep1;
    GtkSeparatorMenuItem *sep2;
    GtkMenuItem *delete;
    GtkMenu *appList;
    GtkMenuItem *child;
    gchar *ctype = NULL;
    GList *appInfoList = NULL;
    const gchar *filename;

    if (manager->sessionBrowserMenu) {
        gtk_widget_destroy(GTK_WIDGET(manager->sessionBrowserMenu));
        manager->sessionBrowserMenu = NULL;
    }

    if (event->type != GDK_BUTTON_PRESS)
        return FALSE;
    if (event->button != 3)
        return FALSE;

    if (!manager->mediaContextMenu)
        return FALSE;

    manager->sessionBrowserMedia = entangle_session_browser_get_media_at_coords(
        browser, event->x, event->y);

    if (!manager->sessionBrowserMedia)
        return FALSE;

    filename = entangle_media_get_filename(manager->sessionBrowserMedia);

    ctype = g_content_type_guess(filename, NULL, 0, NULL);
    if (!ctype)
        return FALSE;

    manager->sessionBrowserMenu = GTK_MENU(gtk_menu_new());

    tearoff = GTK_MENU_ITEM(gtk_menu_item_new_with_label(_("Tear off")));
    gtk_container_add(GTK_CONTAINER(manager->sessionBrowserMenu),
                      GTK_WIDGET(tearoff));
    g_signal_connect(tearoff, "activate",
                     G_CALLBACK(do_menu_session_tearoff_activate), manager);
    sep1 = GTK_SEPARATOR_MENU_ITEM(gtk_separator_menu_item_new());
    gtk_container_add(GTK_CONTAINER(manager->sessionBrowserMenu),
                      GTK_WIDGET(sep1));
    open = GTK_MENU_ITEM(gtk_menu_item_new_with_label(_("Open")));
    gtk_container_add(GTK_CONTAINER(manager->sessionBrowserMenu),
                      GTK_WIDGET(open));
    g_signal_connect(open, "activate",
                     G_CALLBACK(do_menu_session_open_activate), manager);
    openWith = GTK_MENU_ITEM(gtk_menu_item_new_with_label(_("Open with")));
    gtk_container_add(GTK_CONTAINER(manager->sessionBrowserMenu),
                      GTK_WIDGET(openWith));
    sep2 = GTK_SEPARATOR_MENU_ITEM(gtk_separator_menu_item_new());
    gtk_container_add(GTK_CONTAINER(manager->sessionBrowserMenu),
                      GTK_WIDGET(sep2));
    delete = GTK_MENU_ITEM(gtk_menu_item_new_with_label(_("Delete")));
    gtk_container_add(GTK_CONTAINER(manager->sessionBrowserMenu),
                      GTK_WIDGET(delete));
    g_signal_connect(delete, "activate",
                     G_CALLBACK(do_menu_session_delete_activate), manager);

    appInfoList = g_app_info_get_all_for_type(ctype);

    appList = GTK_MENU(gtk_menu_new());

    if (appInfoList) {
        GList *tmp = appInfoList;
        while (tmp) {
            GAppInfo *appInfo = tmp->data;

            child = GTK_MENU_ITEM(gtk_menu_item_new_with_label(
                g_app_info_get_display_name(appInfo)));
            g_signal_connect(child, "activate",
                             G_CALLBACK(do_session_browser_open_with_app),
                             manager);
            g_object_set_data_full(G_OBJECT(child), "appinfo", appInfo,
                                   (GDestroyNotify)g_object_unref);
            gtk_container_add(GTK_CONTAINER(appList), GTK_WIDGET(child));

            tmp = tmp->next;
        }
        g_list_free(appInfoList);

        child = GTK_MENU_ITEM(gtk_separator_menu_item_new());
        gtk_container_add(GTK_CONTAINER(appList), GTK_WIDGET(child));
    }

    child =
        GTK_MENU_ITEM(gtk_menu_item_new_with_label(_("Select application...")));
    g_signal_connect(child, "activate",
                     G_CALLBACK(do_session_browser_open_with_select), manager);
    gtk_container_add(GTK_CONTAINER(appList), GTK_WIDGET(child));

    gtk_widget_show_all(GTK_WIDGET(appList));
    gtk_widget_show_all(GTK_WIDGET(manager->sessionBrowserMenu));

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(openWith), GTK_WIDGET(appList));

    gtk_menu_popup_at_pointer(manager->sessionBrowserMenu, (GdkEvent *)event);

    gtk_widget_set_sensitive(GTK_WIDGET(open),
                             ctype && appInfoList ? TRUE : FALSE);
    return TRUE;
}

static void
do_pixbuf_loaded(EntanglePixbufLoader *loader, EntangleImage *image)
{
    g_return_if_fail(ENTANGLE_IS_IMAGE(image));

    GdkPixbuf *pixbuf = entangle_pixbuf_loader_get_pixbuf(loader, image);

    entangle_image_set_pixbuf(image, pixbuf);
}

static void
do_metadata_loaded(EntanglePixbufLoader *loader, EntangleImage *image)
{
    g_return_if_fail(ENTANGLE_IS_IMAGE(image));

    GExiv2Metadata *metadata =
        entangle_pixbuf_loader_get_metadata(loader, image);

    entangle_media_set_metadata(ENTANGLE_MEDIA(image), metadata);
}

static void
do_pixbuf_unloaded(EntanglePixbufLoader *loader G_GNUC_UNUSED,
                   EntangleImage *image)
{
    g_return_if_fail(ENTANGLE_IS_IMAGE(image));

    entangle_image_set_pixbuf(image, NULL);
}

static void
do_metadata_unloaded(EntanglePixbufLoader *loader G_GNUC_UNUSED,
                     EntangleImage *image)
{
    g_return_if_fail(ENTANGLE_IS_IMAGE(image));

    entangle_media_set_metadata(ENTANGLE_MEDIA(image), NULL);
}

static gboolean
do_image_status_hide(gpointer data)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA_MANAGER(data), FALSE);

    EntangleCameraManager *manager = data;

    gtk_revealer_set_reveal_child(manager->imageDrawer, FALSE);
    manager->imageDrawerTimer = 0;
    return FALSE;
}

static gboolean
do_image_status_show(GtkWidget *widget G_GNUC_UNUSED,
                     GdkEvent *event G_GNUC_UNUSED,
                     gpointer data)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA_MANAGER(data), FALSE);

    EntangleCameraManager *manager = data;

    gtk_revealer_set_reveal_child(manager->imageDrawer, TRUE);
    if (manager->imageDrawerTimer)
        g_source_remove(manager->imageDrawerTimer);
    manager->imageDrawerTimer =
        g_timeout_add_seconds(3, do_image_status_hide, manager);

    return FALSE;
}

static void
do_entangle_camera_manager_hide(EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    entangle_camera_manager_set_camera(manager, NULL);

    ENTANGLE_DEBUG("Removing all popups");
    g_hash_table_remove_all(manager->popups);
}

static void
do_camera_capture_begin(EntangleCameraAutomata *automata,
                        EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_AUTOMATA(automata));
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    EntanglePreferences *prefs =
        entangle_camera_manager_get_preferences(manager);

    if (entangle_preferences_interface_get_screen_blank(prefs))
        entangle_dpms_set_blanking(TRUE, NULL);
}

static void
do_camera_capture_end(EntangleCameraAutomata *automata,
                      EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_AUTOMATA(automata));
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    EntanglePreferences *prefs =
        entangle_camera_manager_get_preferences(manager);

    if (entangle_preferences_interface_get_screen_blank(prefs))
        entangle_dpms_set_blanking(FALSE, NULL);
}

static void
do_image_realize(GtkWidget *widget, EntangleCameraManager *manager)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    GdkWindow *imgWin = gtk_widget_get_window(GTK_WIDGET(widget));

    gdk_window_set_events(imgWin, gdk_window_get_events(imgWin) |
                                      GDK_POINTER_MOTION_MASK);
    g_signal_connect(widget, "motion-notify-event",
                     G_CALLBACK(do_image_status_show), manager);
}

static void
do_action_sync_capture(GSimpleAction *act G_GNUC_UNUSED,
                       GVariant *state,
                       gpointer opaque)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(opaque));

    EntangleCameraManager *manager = ENTANGLE_CAMERA_MANAGER(opaque);

    manager->syncCapture = g_variant_get_boolean(state);
    g_simple_action_set_state(act, g_variant_new_boolean(manager->syncCapture));
}

static GActionEntry actions[] = {
    { .name = "session", .activate = do_action_session },
    { .name = "close", .activate = do_action_close },
    { .name = "connect", .activate = do_action_connect },
    { .name = "disconnect", .activate = do_action_disconnect },
    { .name = "capture", .activate = do_action_capture },
    { .name = "preview", .activate = do_action_preview },
    { .name = "cancel", .activate = do_action_cancel },
    { .name = "sync-clock", .activate = do_action_sync_clock },
    { .name = "zoom-best", .activate = do_action_zoom_best },
    { .name = "zoom-out", .activate = do_action_zoom_out },
    { .name = "zoom-normal", .activate = do_action_zoom_normal },
    { .name = "zoom-in", .activate = do_action_zoom_in },
    { .name = "settings", .state = "true", .change_state = do_action_settings },
    { .name = "fullscreen",
      .state = "false",
      .change_state = do_action_fullscreen },
    { .name = "presentation",
      .state = "false",
      .change_state = do_action_presentation },
    { .name = "presentation-monitor",
      .parameter_type = "i",
      .state = "0",
      .change_state = do_action_presentation_monitor },
    { .name = "sync-capture",
      .state = "false",
      .change_state = do_action_sync_capture },
};

static void
entangle_camera_manager_init(EntangleCameraManager *manager)
{
    GtkWidget *iconScroll;
    GtkWidget *settingsBox;
    GMenuModel *viewMenu;
    GMenuModel *presentSection;
    GMenu *monitorMenu;
    GtkWidget *imageViewport;
    GtkWidget *imageHistogramExpander;
    GtkWidget *imageOverlay;
    GtkBuilder *menuBuilder;

    gtk_widget_init_template(GTK_WIDGET(manager));

    manager->imageScrollHOffset = 0;
    manager->imageScrollVOffset = 0;

    manager->automata = entangle_camera_automata_new();
    manager->cameraPrefs = entangle_camera_preferences_new();

    g_action_map_add_action_entries(G_ACTION_MAP(manager), actions,
                                    G_N_ELEMENTS(actions), manager);

    g_signal_connect(manager->automata, "camera-capture-begin",
                     G_CALLBACK(do_camera_capture_begin), manager);
    g_signal_connect(manager->automata, "camera-capture-end",
                     G_CALLBACK(do_camera_capture_end), manager);

    g_signal_connect(manager, "notify::application",
                     G_CALLBACK(do_entangle_camera_manager_set_app), NULL);

    menuBuilder =
        gtk_builder_new_from_resource("/org/entangle_photo/Manager/frontend/"
                                      "entangle-camera-manager-menus.ui");

    gtk_menu_button_set_menu_model(
        manager->titlebarButtonView,
        G_MENU_MODEL(gtk_builder_get_object(menuBuilder, "view-menu")));
    gtk_menu_button_set_menu_model(
        manager->titlebarButtonAction,
        G_MENU_MODEL(gtk_builder_get_object(menuBuilder, "action-menu")));
    gtk_menu_button_set_menu_model(
        manager->titlebarButtonZoom,
        G_MENU_MODEL(gtk_builder_get_object(menuBuilder, "zoom-menu")));
    gtk_menu_button_set_menu_model(
        manager->toolbarButtonView,
        G_MENU_MODEL(gtk_builder_get_object(menuBuilder, "view-menu")));
    gtk_menu_button_set_menu_model(
        manager->toolbarButtonAction,
        G_MENU_MODEL(gtk_builder_get_object(menuBuilder, "action-menu")));
    gtk_menu_button_set_menu_model(
        manager->toolbarButtonZoom,
        G_MENU_MODEL(gtk_builder_get_object(menuBuilder, "zoom-menu")));

    viewMenu = G_MENU_MODEL(gtk_builder_get_object(menuBuilder, "view-menu"));
    presentSection =
        g_menu_model_get_item_link(viewMenu, 0, G_MENU_LINK_SECTION);
    monitorMenu = entangle_camera_manager_monitor_menu(manager);
    g_menu_append_section(G_MENU(presentSection), "Present on",
                          G_MENU_MODEL(monitorMenu));

    imageViewport = gtk_viewport_new(NULL, NULL);
    manager->imageScroll = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(
        gtk_scrollable_get_hadjustment(GTK_SCROLLABLE(imageViewport)),
        gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(imageViewport))));

    gtk_container_add(GTK_CONTAINER(manager->imageScroll), imageViewport);

    manager->imageLoader = entangle_image_loader_new();
    manager->thumbLoader = entangle_thumbnail_loader_new(256, 256);

    g_signal_connect(manager->imageLoader, "pixbuf-loaded",
                     G_CALLBACK(do_pixbuf_loaded), NULL);
    g_signal_connect(manager->imageLoader, "metadata-loaded",
                     G_CALLBACK(do_metadata_loaded), NULL);
    g_signal_connect(manager->imageLoader, "pixbuf-unloaded",
                     G_CALLBACK(do_pixbuf_unloaded), NULL);
    g_signal_connect(manager->imageLoader, "metadata-unloaded",
                     G_CALLBACK(do_metadata_unloaded), NULL);

    manager->imageDisplay = entangle_image_display_new();
    manager->mediaStatusbar = entangle_media_statusbar_new();
    manager->imageDrawer = GTK_REVEALER(gtk_revealer_new());
    manager->sessionBrowser = entangle_session_browser_new();
    manager->controlPanel = entangle_control_panel_new(manager->cameraPrefs);
    manager->imageHistogram = entangle_image_histogram_new();
    gtk_widget_show(GTK_WIDGET(manager->imageHistogram));
    manager->scriptConfig = entangle_script_config_new();
    gtk_widget_show(GTK_WIDGET(manager->scriptConfig));

    g_object_set(manager->sessionBrowser, "thumbnail-loader",
                 manager->thumbLoader, NULL);

    g_signal_connect(manager->imageDisplay, "size-allocate",
                     G_CALLBACK(do_restore_scroll), manager);

    g_signal_connect(manager->sessionBrowser, "selection-changed",
                     G_CALLBACK(do_session_media_selected), manager);
    g_signal_connect(manager->sessionBrowser, "button-press-event",
                     G_CALLBACK(do_session_browser_popup), manager);

    settingsBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    imageHistogramExpander = gtk_expander_new(_("Image histogram"));
    gtk_expander_set_expanded(GTK_EXPANDER(imageHistogramExpander), TRUE);
    manager->scriptConfigExpander = gtk_expander_new(_("Automation"));
    gtk_expander_set_expanded(GTK_EXPANDER(manager->scriptConfigExpander),
                              TRUE);

    iconScroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(iconScroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);

    gtk_widget_set_size_request(GTK_WIDGET(manager->settingsViewport), 300,
                                100);
    gtk_widget_set_size_request(settingsBox, 300, 100);
    gtk_widget_set_size_request(iconScroll, 140, 170);

    manager->popups = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                            (GDestroyNotify)gtk_widget_destroy);

    gtk_container_add(GTK_CONTAINER(imageViewport),
                      GTK_WIDGET(manager->imageDisplay));

    imageOverlay = gtk_overlay_new();
    gtk_container_add(GTK_CONTAINER(imageOverlay),
                      GTK_WIDGET(manager->imageScroll));
    gtk_overlay_add_overlay(GTK_OVERLAY(imageOverlay),
                            GTK_WIDGET(manager->imageDrawer));
    gtk_overlay_set_overlay_pass_through(
        GTK_OVERLAY(imageOverlay), GTK_WIDGET(manager->imageDrawer), TRUE);
    gtk_container_add(GTK_CONTAINER(manager->imageDrawer),
                      GTK_WIDGET(manager->mediaStatusbar));
    g_object_set(G_OBJECT(manager->imageDrawer), "valign", GTK_ALIGN_START,
                 NULL);

    gtk_revealer_set_transition_duration(manager->imageDrawer, 1500);
    gtk_revealer_set_transition_type(manager->imageDrawer,
                                     GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
    gtk_widget_show(GTK_WIDGET(manager->imageDrawer));
    gtk_widget_show(GTK_WIDGET(manager->mediaStatusbar));

    ENTANGLE_DEBUG("Adding %p to %p", manager->imageDisplay, imageViewport);
    gtk_paned_pack1(manager->displayPanel, imageOverlay, TRUE, TRUE);
    gtk_paned_pack2(manager->displayPanel, GTK_WIDGET(iconScroll), FALSE, TRUE);
    gtk_widget_show_all(GTK_WIDGET(manager->displayPanel));

    gtk_container_add(GTK_CONTAINER(iconScroll),
                      GTK_WIDGET(manager->sessionBrowser));
    gtk_container_add(GTK_CONTAINER(manager->settingsViewport), settingsBox);
    gtk_box_pack_start(GTK_BOX(settingsBox), GTK_WIDGET(manager->controlPanel),
                       FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(settingsBox),
                       GTK_WIDGET(manager->scriptConfigExpander), FALSE, TRUE,
                       0);
    gtk_container_add(GTK_CONTAINER(manager->scriptConfigExpander),
                      GTK_WIDGET(manager->scriptConfig));
    gtk_box_pack_start(GTK_BOX(settingsBox), GTK_WIDGET(imageHistogramExpander),
                       FALSE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(imageHistogramExpander),
                      GTK_WIDGET(manager->imageHistogram));
    gtk_widget_show(GTK_WIDGET(manager->settingsViewport));
    gtk_widget_show(settingsBox);
    gtk_widget_show(imageHistogramExpander);

    gtk_widget_set_margin_end(manager->scriptConfigExpander, 6);
    gtk_widget_set_margin_end(imageHistogramExpander, 6);

    manager->monitorCancel = g_cancellable_new();
    manager->taskCancel = g_cancellable_new();
    manager->taskConfirm = g_cancellable_new();

    do_zoom_widget_sensitivity(manager);
    do_capture_widget_sensitivity(manager);

    gtk_widget_show(GTK_WIDGET(manager->controlPanel));
    gtk_widget_show(GTK_WIDGET(manager->imageDisplay));
    gtk_widget_show(GTK_WIDGET(manager->sessionBrowser));

    g_signal_connect(manager, "realize", G_CALLBACK(do_image_realize), manager);
    g_signal_connect(manager, "hide",
                     G_CALLBACK(do_entangle_camera_manager_hide), NULL);
    g_signal_connect(manager, "delete-event", G_CALLBACK(do_manager_delete),
                     manager);

    g_object_unref(menuBuilder);

    manager->mediaContextMenu = TRUE;
}

EntangleCameraManager *
entangle_camera_manager_new(EntangleApplication *app)
{
    return ENTANGLE_CAMERA_MANAGER(
        g_object_new(ENTANGLE_TYPE_CAMERA_MANAGER, "application", app, NULL));
}

void
entangle_camera_manager_add_script(EntangleCameraManager *manager,
                                   EntangleScript *script)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));
    g_return_if_fail(ENTANGLE_IS_SCRIPT(script));

    entangle_script_config_add_script(manager->scriptConfig, script);
    gtk_widget_show(manager->scriptConfigExpander);
}

void
entangle_camera_manager_remove_script(EntangleCameraManager *manager,
                                      EntangleScript *script)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));
    g_return_if_fail(ENTANGLE_IS_SCRIPT(script));

    entangle_script_config_remove_script(manager->scriptConfig, script);

    if (!entangle_script_config_has_scripts(manager->scriptConfig))
        gtk_widget_hide(manager->scriptConfigExpander);
}

static void
do_camera_close_finish(GObject *source,
                       GAsyncResult *result,
                       gpointer opaque G_GNUC_UNUSED)
{
    EntangleCamera *cam = ENTANGLE_CAMERA(source);
    GError *error = NULL;

    if (entangle_camera_close_finish(cam, result, &error)) {
        ENTANGLE_DEBUG("Unable to disconnect from camera");
    }
}

void
entangle_camera_manager_set_camera(EntangleCameraManager *manager,
                                   EntangleCamera *cam)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager));

    if (manager->camera) {
        do_remove_camera(manager);
        entangle_camera_close_async(manager->camera, NULL,
                                    do_camera_close_finish, manager);
        g_object_unref(manager->camera);
    }
    manager->camera = cam;
    manager->cameraReady = FALSE;
    if (manager->camera) {
        g_object_ref(manager->camera);
        do_add_camera(manager);
    }

    do_capture_widget_sensitivity(manager);
}

/**
 * entangle_camera_manager_get_camera:
 * @manager: the camera manager window
 *
 * Get the camera currently being used
 *
 * Returns: (transfer none): the camera or NULL
 */
EntangleCamera *
entangle_camera_manager_get_camera(EntangleCameraManager *manager)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA_MANAGER(manager), NULL);

    return manager->camera;
}

void
entangle_camera_manager_set_media_context_menu(EntangleCameraManager *manager,
                                               gboolean enabled)
{
    manager->mediaContextMenu = enabled;
}

gboolean
entangle_camera_manager_get_media_context_menu(EntangleCameraManager *manager)
{
    return manager->mediaContextMenu;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
