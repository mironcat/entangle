/*
 *  Entangle: Tethered Camera Control & Capture
 *
 *  Copyright (C) 2009-2017 Daniel P. Berrange
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
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <unistd.h>
#include <math.h>

#include <libpeas-gtk/peas-gtk.h>

#include "entangle-debug.h"
#include "entangle-preferences-display.h"
#include "entangle-camera-picker.h"
#include "entangle-camera-manager.h"
#include "entangle-image-display.h"

static void entangle_preferences_display_refresh(EntanglePreferencesDisplay *preferences);

typedef struct _EntanglePreferencesDisplayPrivate EntanglePreferencesDisplayPrivate;
struct _EntanglePreferencesDisplayPrivate {
    PeasGtkPluginManager *pluginManager;
    EntanglePreferences *prefs;
    gulong prefsID;

    GtkTreeView *preferencesSwitch;
    GtkNotebook *preferencesNotebook;

    GtkEventBox *captureBox;
    GtkBox *captureHBox;
    GtkLabel *captureLabel;
    GtkImage *captureImage;

    GtkEventBox *cmsBox;
    GtkBox *cmsHBox;
    GtkLabel *cmsLabel;
    GtkImage *cmsImage;

    GtkEventBox *pluginsBox;
    GtkBox *pluginsHBox;
    GtkLabel *pluginsLabel;
    GtkImage *pluginsImage;

    GtkEventBox *interfaceBox;
    GtkBox *interfaceHBox;
    GtkLabel *interfaceLabel;
    GtkImage *interfaceImage;

    GtkEventBox *imgBox;
    GtkBox *imgHBox;
    GtkLabel *imgLabel;
    GtkImage *imgImage;

    GtkEntry *captureFilenamePattern;
    GtkToggleButton *captureContinuousPreview;
    GtkToggleButton *captureElectronicShutter;
    GtkToggleButton *captureDeleteFile;
    GtkToggleButton *captureSyncClock;

    GtkToggleButton *cmsEnabled;
    GtkFileChooser *cmsRgbProfile;
    GtkFileChooser *cmsMonitorProfile;
    GtkToggleButton *cmsDetectSystemProfile;
    GtkComboBox *cmsRenderingIntent;

    GtkContainer *pluginsPanel;

    GtkToggleButton *interfaceAutoConnect;
    GtkToggleButton *interfaceScreenBlank;
    GtkToggleButton *interfaceHistogramLinear;

    GtkToggleButton *imgMaskEnabled;
    GtkLabel *imgAspectRatioLabel;
    GtkLabel *imgMaskOpacityLabel;
    GtkComboBox *imgAspectRatio;
    GtkSpinButton *imgMaskOpacity;
    GtkToggleButton *imgFocusPoint;
    GtkComboBox *imgGridLines;
    GtkToggleButton *imgEmbeddedPreview;
    GtkToggleButton *imgOnionSkin;
    GtkLabel *imgOnionLayersLabel;
    GtkSpinButton *imgOnionLayers;
    GtkLabel *imgBackgroundLabel;
    GtkColorChooser *imgBackground;
    GtkLabel *imgHighlightLabel;
    GtkColorChooser *imgHighlight;
};

G_DEFINE_TYPE_WITH_PRIVATE(EntanglePreferencesDisplay, entangle_preferences_display, GTK_TYPE_DIALOG);

enum {
    PROP_0,
};

void do_preferences_close(GtkButton *src, EntanglePreferencesDisplay *preferences);
gboolean do_preferences_delete(GtkWidget *src,
                               GdkEvent *ev,
                               EntanglePreferencesDisplay *preferences);
void do_page_changed(GtkTreeSelection *selection,
                     EntanglePreferencesDisplay *preferences);
void do_cms_enabled_toggled(GtkToggleButton *src, EntanglePreferencesDisplay *display);
void do_cms_rgb_profile_file_set(GtkFileChooserButton *src, EntanglePreferencesDisplay *display);
void do_cms_monitor_profile_file_set(GtkFileChooserButton *src, EntanglePreferencesDisplay *display);
void do_cms_detect_system_profile_toggled(GtkToggleButton *src, EntanglePreferencesDisplay *display);
void do_cms_rendering_intent_changed(GtkComboBox *src, EntanglePreferencesDisplay *display);

void do_interface_auto_connect_toggled(GtkToggleButton *src, EntanglePreferencesDisplay *display);
void do_interface_screen_blank_toggled(GtkToggleButton *src, EntanglePreferencesDisplay *display);
void do_interface_histogram_linear_toggled(GtkToggleButton *src, EntanglePreferencesDisplay *display);

void do_capture_filename_pattern_changed(GtkEntry *src, EntanglePreferencesDisplay *display);
void do_capture_continuous_preview_toggled(GtkToggleButton *src, EntanglePreferencesDisplay *display);
void do_capture_electronic_shutter(GtkToggleButton *src, EntanglePreferencesDisplay *preferences);
void do_capture_delete_file_toggled(GtkToggleButton *src, EntanglePreferencesDisplay *display);
void do_capture_sync_clock_toggled(GtkToggleButton *src, EntanglePreferencesDisplay *display);

void do_img_mask_enabled_toggled(GtkToggleButton *src, EntanglePreferencesDisplay *display);
void do_img_aspect_ratio_changed(GtkComboBox *src, EntanglePreferencesDisplay *display);
void do_img_mask_opacity_changed(GtkSpinButton *src, EntanglePreferencesDisplay *display);
void do_img_focus_point_toggled(GtkToggleButton *src, EntanglePreferencesDisplay *display);
void do_img_grid_lines_changed(GtkComboBox *src, EntanglePreferencesDisplay *display);
void do_img_embedded_preview_toggled(GtkToggleButton *src, EntanglePreferencesDisplay *display);
void do_img_onion_skin_toggled(GtkToggleButton *src, EntanglePreferencesDisplay *display);
void do_img_onion_layers_changed(GtkSpinButton *src, EntanglePreferencesDisplay *display);
void do_img_background_changed(GtkColorButton *src, EntanglePreferencesDisplay *display);
void do_img_highlight_changed(GtkColorButton *src, EntanglePreferencesDisplay *display);

static void entangle_preferences_display_notify(GObject *object,
                                                GParamSpec *spec,
                                                gpointer data)
{
    g_return_if_fail(ENTANGLE_IS_PREFERENCES_DISPLAY(data));

    EntanglePreferencesDisplay *preferences = ENTANGLE_PREFERENCES_DISPLAY(data);
    EntanglePreferencesDisplayPrivate *priv = entangle_preferences_display_get_instance_private(preferences);

    ENTANGLE_DEBUG("Internal display Set %p %s", object, spec->name);
    if (strcmp(spec->name, "cms-enabled") == 0) {
        gboolean newvalue;
        gboolean oldvalue;

        g_object_get(object, spec->name, &newvalue, NULL);
        oldvalue = gtk_toggle_button_get_active(priv->cmsEnabled);

        if (newvalue != oldvalue)
            gtk_toggle_button_set_active(priv->cmsEnabled, newvalue);
    } else if (strcmp(spec->name, "cms-detect-system-profile") == 0) {
        gboolean newvalue;
        gboolean oldvalue;

        g_object_get(object, spec->name, &newvalue, NULL);
        oldvalue = gtk_toggle_button_get_active(priv->cmsDetectSystemProfile);

        if (newvalue != oldvalue)
            gtk_toggle_button_set_active(priv->cmsDetectSystemProfile, newvalue);
    } else if (strcmp(spec->name, "cms-rgb-profile") == 0) {
        EntangleColourProfile *profile;
        const gchar *oldvalue;
        const gchar *newvalue;

        g_object_get(object, spec->name, &profile, NULL);

        newvalue = profile ? entangle_colour_profile_filename(profile) : NULL;
        oldvalue = gtk_file_chooser_get_filename(priv->cmsRgbProfile);

        if ((newvalue && !oldvalue) ||
            (!newvalue && oldvalue) ||
            strcmp(newvalue, oldvalue) != 0)
            gtk_file_chooser_set_filename(priv->cmsRgbProfile, newvalue);

        if (profile)
            g_object_unref(profile);
    } else if (strcmp(spec->name, "cms-monitor-profile") == 0) {
        EntangleColourProfile *profile;
        const gchar *oldvalue;
        const gchar *newvalue;

        g_object_get(object, spec->name, &profile, NULL);

        newvalue = profile ? entangle_colour_profile_filename(profile) : NULL;
        oldvalue = gtk_file_chooser_get_filename(priv->cmsMonitorProfile);

        if ((newvalue && !oldvalue) ||
            (!newvalue && oldvalue) ||
            strcmp(newvalue, oldvalue) != 0)
            gtk_file_chooser_set_filename(priv->cmsMonitorProfile, newvalue);

        if (profile)
            g_object_unref(profile);
    } else if (strcmp(spec->name, "cms-rendering-intent") == 0) {
        int newvalue;
        int oldvalue;

        g_object_get(object, spec->name, &newvalue, NULL);
        oldvalue = gtk_combo_box_get_active(priv->cmsRenderingIntent);

        if (oldvalue != newvalue)
            gtk_combo_box_set_active(priv->cmsRenderingIntent, newvalue);
    } else if (strcmp(spec->name, "interface-auto-connect") == 0) {
        gboolean newvalue;
        gboolean oldvalue;

        g_object_get(object, spec->name, &newvalue, NULL);
        oldvalue = gtk_toggle_button_get_active(priv->interfaceAutoConnect);

        if (newvalue != oldvalue)
            gtk_toggle_button_set_active(priv->interfaceAutoConnect, newvalue);
    } else if (strcmp(spec->name, "interface-screen-blank") == 0) {
        gboolean newvalue;
        gboolean oldvalue;

        g_object_get(object, spec->name, &newvalue, NULL);
        oldvalue = gtk_toggle_button_get_active(priv->interfaceScreenBlank);

        if (newvalue != oldvalue)
            gtk_toggle_button_set_active(priv->interfaceScreenBlank, newvalue);
    } else if (strcmp(spec->name, "interface-histogram-linear") == 0) {
        gboolean newvalue;
        gboolean oldvalue;

        g_object_get(object, spec->name, &newvalue, NULL);
        oldvalue = gtk_toggle_button_get_active(priv->interfaceHistogramLinear);

        if (newvalue != oldvalue)
            gtk_toggle_button_set_active(priv->interfaceHistogramLinear, newvalue);
    } else if (strcmp(spec->name, "capture-filename-pattern") == 0) {
        gchar *newvalue;
        const gchar *oldvalue;

        g_object_get(object, spec->name, &newvalue, NULL);

        oldvalue = gtk_entry_get_text(priv->captureFilenamePattern);
        if ((newvalue && !oldvalue) ||
            (!newvalue && oldvalue) ||
            strcmp(newvalue, oldvalue) != 0)
            gtk_entry_set_text(priv->captureFilenamePattern, newvalue);

        g_free(newvalue);
    } else if (strcmp(spec->name, "capture-continuous-preview") == 0) {
        gboolean newvalue;
        gboolean oldvalue;

        g_object_get(object, spec->name, &newvalue, NULL);
        oldvalue = gtk_toggle_button_get_active(priv->captureContinuousPreview);

        if (newvalue != oldvalue)
            gtk_toggle_button_set_active(priv->captureContinuousPreview, newvalue);
    } else if (strcmp(spec->name, "capture-electronic-shutter") == 0) {
        gboolean newvalue;
        gboolean oldvalue;

        g_object_get(object, spec->name, &newvalue, NULL);
        oldvalue = gtk_toggle_button_get_active(priv->captureElectronicShutter);

        if (newvalue != oldvalue)
            gtk_toggle_button_set_active(priv->captureElectronicShutter, newvalue);
    } else if (strcmp(spec->name, "capture-delete-file") == 0) {
        gboolean newvalue;
        gboolean oldvalue;

        g_object_get(object, spec->name, &newvalue, NULL);
        oldvalue = gtk_toggle_button_get_active(priv->captureDeleteFile);

        if (newvalue != oldvalue)
            gtk_toggle_button_set_active(priv->captureDeleteFile, newvalue);
    } else if (strcmp(spec->name, "capture-sync-clock") == 0) {
        gboolean newvalue;
        gboolean oldvalue;

        g_object_get(object, spec->name, &newvalue, NULL);
        oldvalue = gtk_toggle_button_get_active(priv->captureSyncClock);

        if (newvalue != oldvalue)
            gtk_toggle_button_set_active(priv->captureSyncClock, newvalue);
    } else if (strcmp(spec->name, "img-mask-enabled") == 0) {
        gboolean newvalue;
        gboolean oldvalue;

        g_object_get(object, spec->name, &newvalue, NULL);
        oldvalue = gtk_toggle_button_get_active(priv->imgMaskEnabled);

        if (newvalue != oldvalue)
            gtk_toggle_button_set_active(priv->imgMaskEnabled, newvalue);
    } else if (strcmp(spec->name, "img-aspect-ratio") == 0) {
        gchar *newvalue;
        const gchar *oldvalue;

        g_object_get(object, spec->name, &newvalue, NULL);

        oldvalue = gtk_combo_box_get_active_id(priv->imgAspectRatio);
        if ((newvalue && !oldvalue) ||
            (!newvalue && oldvalue) ||
            strcmp(newvalue, oldvalue) != 0)
            gtk_combo_box_set_active_id(priv->imgAspectRatio, newvalue);

        g_free(newvalue);
    } else if (strcmp(spec->name, "img-mask-opacity") == 0) {
        GtkAdjustment *adjust = gtk_spin_button_get_adjustment(priv->imgMaskOpacity);
        gint newvalue;
        gfloat oldvalue;

        g_object_get(object, spec->name, &newvalue, NULL);
        oldvalue = gtk_adjustment_get_value(adjust);

        if (fabs(newvalue - oldvalue)  > 0.0005)
            gtk_adjustment_set_value(adjust, newvalue);
    } else if (strcmp(spec->name, "img-focus-point") == 0) {
        gboolean newvalue;
        gboolean oldvalue;

        g_object_get(object, spec->name, &newvalue, NULL);
        oldvalue = gtk_toggle_button_get_active(priv->imgFocusPoint);

        if (newvalue != oldvalue)
            gtk_toggle_button_set_active(priv->imgFocusPoint, newvalue);
    } else if (strcmp(spec->name, "img-grid-lines") == 0) {
        gint newvalue;
        gint oldvalue;
        const gchar *oldid;
        GEnumClass *enum_class;
        GEnumValue *enum_value;

        enum_class = g_type_class_ref(ENTANGLE_TYPE_IMAGE_DISPLAY_GRID);

        g_object_get(object, spec->name, &newvalue, NULL);
        oldid = gtk_combo_box_get_active_id(priv->imgGridLines);

        oldvalue = ENTANGLE_IMAGE_DISPLAY_GRID_NONE;
        if (oldid) {
            enum_value = g_enum_get_value_by_nick(enum_class, oldid);
            if (enum_value != NULL)
                oldvalue = enum_value->value;
        }

        if (newvalue != oldvalue) {
            enum_value = g_enum_get_value(enum_class, newvalue);
            if (enum_value != NULL)
                gtk_combo_box_set_active_id(priv->imgGridLines, enum_value->value_nick);
            else
                gtk_combo_box_set_active_id(priv->imgGridLines, "none");
        }

        g_type_class_unref(enum_class);
    } else if (strcmp(spec->name, "img-embedded-preview") == 0) {
        gboolean newvalue;
        gboolean oldvalue;

        g_object_get(object, spec->name, &newvalue, NULL);
        oldvalue = gtk_toggle_button_get_active(priv->imgEmbeddedPreview);

        if (newvalue != oldvalue)
            gtk_toggle_button_set_active(priv->imgEmbeddedPreview, newvalue);
    } else if (strcmp(spec->name, "img-onion-skin") == 0) {
        gboolean newvalue;
        gboolean oldvalue;

        g_object_get(object, spec->name, &newvalue, NULL);
        oldvalue = gtk_toggle_button_get_active(priv->imgOnionSkin);

        if (newvalue != oldvalue)
            gtk_toggle_button_set_active(priv->imgOnionSkin, newvalue);
    } else if (strcmp(spec->name, "img-onion-layers") == 0) {
        GtkAdjustment *adjust = gtk_spin_button_get_adjustment(priv->imgOnionLayers);
        gint newvalue;
        gfloat oldvalue;

        g_object_get(object, spec->name, &newvalue, NULL);
        oldvalue = gtk_adjustment_get_value(adjust);

        if (fabs(newvalue - oldvalue)  > 0.0005)
            gtk_adjustment_set_value(adjust, newvalue);
    }
}

static void do_entangle_preferences_display_set_app(GObject *object,
                                                    GParamSpec *spec G_GNUC_UNUSED)
{
    EntanglePreferencesDisplay *preferences = ENTANGLE_PREFERENCES_DISPLAY(object);
    EntanglePreferencesDisplayPrivate *priv = entangle_preferences_display_get_instance_private(preferences);
    PeasEngine *pluginEngine;
    EntangleApplication *app;

    app = ENTANGLE_APPLICATION(gtk_window_get_application(GTK_WINDOW(preferences)));
    priv->prefs = g_object_ref(entangle_application_get_preferences(app));
    entangle_preferences_display_refresh(preferences);
    priv->prefsID = g_signal_connect(priv->prefs,
                                     "notify",
                                     G_CALLBACK(entangle_preferences_display_notify),
                                     object);
    pluginEngine = entangle_application_get_plugin_engine(app);
    priv->pluginManager = PEAS_GTK_PLUGIN_MANAGER(peas_gtk_plugin_manager_new(pluginEngine));
    gtk_container_add_with_properties(priv->pluginsPanel, GTK_WIDGET(priv->pluginManager),
                                      "expand", TRUE, NULL);
    gtk_widget_show_all(GTK_WIDGET(priv->pluginManager));
}


static void entangle_preferences_display_refresh(EntanglePreferencesDisplay *preferences)
{
    g_return_if_fail(ENTANGLE_IS_PREFERENCES_DISPLAY(preferences));

    EntanglePreferencesDisplayPrivate *priv = entangle_preferences_display_get_instance_private(preferences);
    EntangleColourProfile *profile;
    const gchar *ratio;
    GtkAdjustment *adjust;
    gboolean hasContinuousPreview;
    gboolean hasRatio;
    gboolean hasOnion;

    gtk_toggle_button_set_active(priv->cmsEnabled, entangle_preferences_cms_get_enabled(priv->prefs));

    gtk_toggle_button_set_active(priv->cmsDetectSystemProfile, entangle_preferences_cms_get_detect_system_profile(priv->prefs));

    profile = entangle_preferences_cms_get_rgb_profile(priv->prefs);
    if (profile) {
        gtk_file_chooser_set_filename(priv->cmsRgbProfile, entangle_colour_profile_filename(profile));
        g_object_unref(profile);
    }

    profile = entangle_preferences_cms_get_monitor_profile(priv->prefs);
    if (profile) {
        gtk_file_chooser_set_filename(priv->cmsMonitorProfile, entangle_colour_profile_filename(profile));
        g_object_unref(profile);
    }

    gtk_combo_box_set_active(priv->cmsRenderingIntent, entangle_preferences_cms_get_rendering_intent(priv->prefs));

    gtk_toggle_button_set_active(priv->interfaceAutoConnect, entangle_preferences_interface_get_auto_connect(priv->prefs));
    gtk_toggle_button_set_active(priv->interfaceScreenBlank, entangle_preferences_interface_get_screen_blank(priv->prefs));
    gtk_toggle_button_set_active(priv->interfaceHistogramLinear,
                                 entangle_preferences_interface_get_histogram_linear(priv->prefs));

    gtk_entry_set_text(priv->captureFilenamePattern, entangle_preferences_capture_get_filename_pattern(priv->prefs));

    hasContinuousPreview = entangle_preferences_capture_get_continuous_preview(priv->prefs);
    gtk_toggle_button_set_active(priv->captureContinuousPreview, hasContinuousPreview);

    gtk_toggle_button_set_active(priv->captureElectronicShutter, entangle_preferences_capture_get_electronic_shutter(priv->prefs));
    gtk_widget_set_sensitive(GTK_WIDGET(priv->captureElectronicShutter), hasContinuousPreview);

    gtk_toggle_button_set_active(priv->captureDeleteFile, entangle_preferences_capture_get_delete_file(priv->prefs));

    gtk_toggle_button_set_active(priv->captureSyncClock, entangle_preferences_capture_get_sync_clock(priv->prefs));

    ratio = entangle_preferences_img_get_aspect_ratio(priv->prefs);
    hasRatio = entangle_preferences_img_get_mask_enabled(priv->prefs);

    gtk_toggle_button_set_active(priv->imgMaskEnabled, hasRatio);

    gtk_combo_box_set_active_id(priv->imgAspectRatio, ratio);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->imgAspectRatio), hasRatio);

    gtk_widget_set_sensitive(GTK_WIDGET(priv->imgAspectRatioLabel), hasRatio);

    gtk_widget_set_sensitive(GTK_WIDGET(priv->imgMaskOpacity), hasRatio);
    adjust = gtk_spin_button_get_adjustment(priv->imgMaskOpacity);
    gtk_adjustment_set_value(adjust,
                             entangle_preferences_img_get_mask_opacity(priv->prefs));

    gtk_widget_set_sensitive(GTK_WIDGET(priv->imgMaskOpacityLabel), hasRatio);

    gtk_toggle_button_set_active(priv->imgFocusPoint,
                                 entangle_preferences_img_get_focus_point(priv->prefs));

    GEnumClass *enum_class = g_type_class_ref(ENTANGLE_TYPE_IMAGE_DISPLAY_GRID);
    GEnumValue *enum_value = g_enum_get_value(enum_class,
                                              entangle_preferences_img_get_grid_lines(priv->prefs));
    g_type_class_unref(enum_class);

    if (enum_value != NULL)
        gtk_combo_box_set_active_id(priv->imgGridLines, enum_value->value_nick);
    else
        gtk_combo_box_set_active_id(priv->imgGridLines, NULL);

    gtk_toggle_button_set_active(priv->imgEmbeddedPreview,
                                 entangle_preferences_img_get_embedded_preview(priv->prefs));

    hasOnion = entangle_preferences_img_get_onion_skin(priv->prefs);
    gtk_toggle_button_set_active(priv->imgOnionSkin, hasOnion);

    gtk_widget_set_sensitive(GTK_WIDGET(priv->imgOnionLayers), hasOnion);
    adjust = gtk_spin_button_get_adjustment(priv->imgOnionLayers);
    gtk_adjustment_set_value(adjust,
                             entangle_preferences_img_get_onion_layers(priv->prefs));

    gtk_widget_set_sensitive(GTK_WIDGET(priv->imgOnionLayersLabel), hasOnion);

    GdkRGBA gbg;
    gchar *bg = entangle_preferences_img_get_background(priv->prefs);
    gdk_rgba_parse(&gbg, bg);
    gtk_color_chooser_set_rgba(priv->imgBackground, &gbg);
    g_free(bg);

    GdkRGBA ghl;
    gchar *hl = entangle_preferences_img_get_highlight(priv->prefs);
    gdk_rgba_parse(&ghl, hl);
    gtk_color_chooser_set_rgba(priv->imgHighlight, &ghl);
    g_free(hl);
}


static void entangle_preferences_display_finalize(GObject *object)
{
    EntanglePreferencesDisplay *preferences = ENTANGLE_PREFERENCES_DISPLAY(object);
    EntanglePreferencesDisplayPrivate *priv = entangle_preferences_display_get_instance_private(preferences);

    ENTANGLE_DEBUG("Finalize preferences");

    g_signal_handler_disconnect(priv->prefs, priv->prefsID);

    G_OBJECT_CLASS(entangle_preferences_display_parent_class)->finalize(object);
}


static void entangle_preferences_display_class_init(EntanglePreferencesDisplayClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = entangle_preferences_display_finalize;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass),
                                                "/org/entangle_photo/Manager/frontend/entangle-preferences-display.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, preferencesSwitch);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, preferencesNotebook);

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, captureBox);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, captureHBox);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, captureLabel);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, captureImage);

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, cmsBox);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, cmsHBox);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, cmsLabel);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, cmsImage);

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, pluginsBox);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, pluginsHBox);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, pluginsLabel);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, pluginsImage);

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, interfaceBox);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, interfaceHBox);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, interfaceLabel);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, interfaceImage);

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, imgBox);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, imgHBox);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, imgLabel);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, imgImage);

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, captureFilenamePattern);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, captureContinuousPreview);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, captureElectronicShutter);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, captureDeleteFile);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, captureSyncClock);

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, cmsEnabled);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, cmsRgbProfile);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, cmsMonitorProfile);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, cmsDetectSystemProfile);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, cmsRenderingIntent);

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, pluginsPanel);

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, interfaceAutoConnect);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, interfaceScreenBlank);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, interfaceHistogramLinear);

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, imgMaskEnabled);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, imgAspectRatioLabel);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, imgMaskOpacityLabel);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, imgAspectRatio);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, imgMaskOpacity);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, imgFocusPoint);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, imgGridLines);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, imgEmbeddedPreview);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, imgOnionSkin);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, imgOnionLayersLabel);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, imgOnionLayers);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, imgBackgroundLabel);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, imgBackground);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, imgHighlightLabel);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntanglePreferencesDisplay, imgHighlight);
}


EntanglePreferencesDisplay *entangle_preferences_display_new(void)
{
    return ENTANGLE_PREFERENCES_DISPLAY(g_object_new(ENTANGLE_TYPE_PREFERENCES_DISPLAY, NULL));
}


void do_preferences_close(GtkButton *src G_GNUC_UNUSED, EntanglePreferencesDisplay *preferences)
{
    g_return_if_fail(ENTANGLE_IS_PREFERENCES_DISPLAY(preferences));

    gtk_widget_hide(GTK_WIDGET(preferences));
}


gboolean do_preferences_delete(GtkWidget *src,
                               GdkEvent *ev G_GNUC_UNUSED,
                               EntanglePreferencesDisplay *preferences)
{
    g_return_val_if_fail(ENTANGLE_IS_PREFERENCES_DISPLAY(preferences), TRUE);

    ENTANGLE_DEBUG("preferences delete");
    gtk_widget_hide(src);
    return TRUE;
}

void do_page_changed(GtkTreeSelection *selection,
                     EntanglePreferencesDisplay *preferences)
{
    g_return_if_fail(ENTANGLE_IS_PREFERENCES_DISPLAY(preferences));

    EntanglePreferencesDisplayPrivate *priv = entangle_preferences_display_get_instance_private(preferences);
    GtkTreeIter iter;
    gboolean selected;
    GValue val;
    int page;

    ENTANGLE_DEBUG("select page");

    selected = gtk_tree_selection_get_selected(selection, NULL, &iter);
    if (!selected)
        return;

    memset(&val, 0, sizeof val);
    gtk_tree_model_get_value(gtk_tree_view_get_model(priv->preferencesSwitch),
                             &iter, 0, &val);

    page = g_value_get_int(&val);
    if (page >= 0)
        gtk_notebook_set_current_page(priv->preferencesNotebook, page);
}


void do_cms_enabled_toggled(GtkToggleButton *src, EntanglePreferencesDisplay *preferences)
{
    g_return_if_fail(ENTANGLE_IS_PREFERENCES_DISPLAY(preferences));

    EntanglePreferencesDisplayPrivate *priv = entangle_preferences_display_get_instance_private(preferences);
    gboolean enabled = gtk_toggle_button_get_active(src);

    entangle_preferences_cms_set_enabled(priv->prefs, enabled);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->cmsRgbProfile), enabled);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->cmsDetectSystemProfile), enabled);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->cmsRenderingIntent), enabled);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->cmsMonitorProfile), !gtk_toggle_button_get_active(priv->cmsDetectSystemProfile));
}

void do_cms_rgb_profile_file_set(GtkFileChooserButton *src, EntanglePreferencesDisplay *preferences)
{
    g_return_if_fail(ENTANGLE_IS_PREFERENCES_DISPLAY(preferences));

    EntanglePreferencesDisplayPrivate *priv = entangle_preferences_display_get_instance_private(preferences);
    char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(src));
    EntangleColourProfile *profile = entangle_colour_profile_new_file(filename);

    entangle_preferences_cms_set_rgb_profile(priv->prefs, profile);
    g_free(filename);
    g_object_unref(profile);
}

void do_cms_monitor_profile_file_set(GtkFileChooserButton *src, EntanglePreferencesDisplay *preferences)
{
    g_return_if_fail(ENTANGLE_IS_PREFERENCES_DISPLAY(preferences));

    EntanglePreferencesDisplayPrivate *priv = entangle_preferences_display_get_instance_private(preferences);
    char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(src));
    EntangleColourProfile *profile = entangle_colour_profile_new_file(filename);

    entangle_preferences_cms_set_monitor_profile(priv->prefs, profile);
    g_free(filename);
    g_object_unref(profile);
}

void do_cms_detect_system_profile_toggled(GtkToggleButton *src, EntanglePreferencesDisplay *preferences)
{
    g_return_if_fail(ENTANGLE_IS_PREFERENCES_DISPLAY(preferences));

    EntanglePreferencesDisplayPrivate *priv = entangle_preferences_display_get_instance_private(preferences);
    gboolean enabled = gtk_toggle_button_get_active(src);

    entangle_preferences_cms_set_detect_system_profile(priv->prefs, enabled);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->cmsMonitorProfile), !enabled);
}

void do_cms_rendering_intent_changed(GtkComboBox *src, EntanglePreferencesDisplay *preferences)
{
    g_return_if_fail(ENTANGLE_IS_PREFERENCES_DISPLAY(preferences));

    EntanglePreferencesDisplayPrivate *priv = entangle_preferences_display_get_instance_private(preferences);
    int option = gtk_combo_box_get_active(src);

    if (option < 0)
        option = ENTANGLE_COLOUR_PROFILE_INTENT_PERCEPTUAL;
    entangle_preferences_cms_set_rendering_intent(priv->prefs, option);
}


void do_capture_filename_pattern_changed(GtkEntry *src, EntanglePreferencesDisplay *preferences)
{
    g_return_if_fail(ENTANGLE_IS_PREFERENCES_DISPLAY(preferences));

    EntanglePreferencesDisplayPrivate *priv = entangle_preferences_display_get_instance_private(preferences);
    const char *text = gtk_entry_get_text(src);

    entangle_preferences_capture_set_filename_pattern(priv->prefs, text);
}


void do_capture_continuous_preview_toggled(GtkToggleButton *src, EntanglePreferencesDisplay *preferences)
{
    g_return_if_fail(ENTANGLE_IS_PREFERENCES_DISPLAY(preferences));

    EntanglePreferencesDisplayPrivate *priv = entangle_preferences_display_get_instance_private(preferences);
    gboolean enabled = gtk_toggle_button_get_active(src);

    gtk_widget_set_sensitive(GTK_WIDGET(priv->captureElectronicShutter), enabled);

    entangle_preferences_capture_set_continuous_preview(priv->prefs, enabled);
}


void do_capture_electronic_shutter(GtkToggleButton *src, EntanglePreferencesDisplay *preferences)
{
    g_return_if_fail(ENTANGLE_IS_PREFERENCES_DISPLAY(preferences));

    EntanglePreferencesDisplayPrivate *priv = entangle_preferences_display_get_instance_private(preferences);
    gboolean enabled = gtk_toggle_button_get_active(src);

    entangle_preferences_capture_set_electronic_shutter(priv->prefs, enabled);
}


void do_interface_auto_connect_toggled(GtkToggleButton *src, EntanglePreferencesDisplay *preferences)
{
    g_return_if_fail(ENTANGLE_IS_PREFERENCES_DISPLAY(preferences));

    EntanglePreferencesDisplayPrivate *priv = entangle_preferences_display_get_instance_private(preferences);
    gboolean enabled = gtk_toggle_button_get_active(src);

    entangle_preferences_interface_set_auto_connect(priv->prefs, enabled);
}


void do_interface_screen_blank_toggled(GtkToggleButton *src, EntanglePreferencesDisplay *preferences)
{
    g_return_if_fail(ENTANGLE_IS_PREFERENCES_DISPLAY(preferences));

    EntanglePreferencesDisplayPrivate *priv = entangle_preferences_display_get_instance_private(preferences);
    gboolean enabled = gtk_toggle_button_get_active(src);

    entangle_preferences_interface_set_screen_blank(priv->prefs, enabled);
}


void do_interface_histogram_linear_toggled(GtkToggleButton *src, EntanglePreferencesDisplay *preferences)
{
    g_return_if_fail(ENTANGLE_IS_PREFERENCES_DISPLAY(preferences));

    EntanglePreferencesDisplayPrivate *priv = entangle_preferences_display_get_instance_private(preferences);
    gboolean enabled = gtk_toggle_button_get_active(src);

    entangle_preferences_interface_set_histogram_linear(priv->prefs, enabled);
}


void do_capture_delete_file_toggled(GtkToggleButton *src, EntanglePreferencesDisplay *preferences)
{
    g_return_if_fail(ENTANGLE_IS_PREFERENCES_DISPLAY(preferences));

    EntanglePreferencesDisplayPrivate *priv = entangle_preferences_display_get_instance_private(preferences);
    gboolean enabled = gtk_toggle_button_get_active(src);

    entangle_preferences_capture_set_delete_file(priv->prefs, enabled);
}


void do_capture_sync_clock_toggled(GtkToggleButton *src, EntanglePreferencesDisplay *preferences)
{
    g_return_if_fail(ENTANGLE_IS_PREFERENCES_DISPLAY(preferences));

    EntanglePreferencesDisplayPrivate *priv = entangle_preferences_display_get_instance_private(preferences);
    gboolean enabled = gtk_toggle_button_get_active(src);

    entangle_preferences_capture_set_sync_clock(priv->prefs, enabled);
}


void do_img_mask_enabled_toggled(GtkToggleButton *src, EntanglePreferencesDisplay *preferences)
{
    g_return_if_fail(ENTANGLE_IS_PREFERENCES_DISPLAY(preferences));

    EntanglePreferencesDisplayPrivate *priv = entangle_preferences_display_get_instance_private(preferences);
    gboolean enabled = gtk_toggle_button_get_active(src);

    gtk_widget_set_sensitive(GTK_WIDGET(priv->imgAspectRatio), enabled);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->imgAspectRatioLabel), enabled);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->imgMaskOpacity), enabled);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->imgMaskOpacityLabel), enabled);

    entangle_preferences_img_set_mask_enabled(priv->prefs, enabled);
}


void do_img_aspect_ratio_changed(GtkComboBox *src, EntanglePreferencesDisplay *preferences)
{
    g_return_if_fail(ENTANGLE_IS_PREFERENCES_DISPLAY(preferences));

    EntanglePreferencesDisplayPrivate *priv = entangle_preferences_display_get_instance_private(preferences);
    const gchar *ratio = gtk_combo_box_get_active_id(src);

    if (ratio == NULL)
        ratio = "";

    entangle_preferences_img_set_aspect_ratio(priv->prefs, ratio);
}


void do_img_mask_opacity_changed(GtkSpinButton *src, EntanglePreferencesDisplay *preferences)
{
    g_return_if_fail(ENTANGLE_IS_PREFERENCES_DISPLAY(preferences));

    EntanglePreferencesDisplayPrivate *priv = entangle_preferences_display_get_instance_private(preferences);
    GtkAdjustment *adjust = gtk_spin_button_get_adjustment(src);

    entangle_preferences_img_set_mask_opacity(priv->prefs,
                                              gtk_adjustment_get_value(adjust));
}


void do_img_focus_point_toggled(GtkToggleButton *src, EntanglePreferencesDisplay *preferences)
{
    g_return_if_fail(ENTANGLE_IS_PREFERENCES_DISPLAY(preferences));

    EntanglePreferencesDisplayPrivate *priv = entangle_preferences_display_get_instance_private(preferences);
    gboolean enabled = gtk_toggle_button_get_active(src);

    entangle_preferences_img_set_focus_point(priv->prefs, enabled);
}


void do_img_grid_lines_changed(GtkComboBox *src, EntanglePreferencesDisplay *preferences)
{
    g_return_if_fail(ENTANGLE_IS_PREFERENCES_DISPLAY(preferences));

    EntanglePreferencesDisplayPrivate *priv = entangle_preferences_display_get_instance_private(preferences);
    const gchar *id = gtk_combo_box_get_active_id(src);
    EntangleImageDisplayGrid grid = ENTANGLE_IMAGE_DISPLAY_GRID_NONE;

    if (id) {
        GEnumClass *enum_class = g_type_class_ref(ENTANGLE_TYPE_IMAGE_DISPLAY_GRID);
        GEnumValue *enum_value = g_enum_get_value_by_nick(enum_class, id);
        g_type_class_unref(enum_class);

        if (enum_value != NULL)
            grid = enum_value->value;
    }

    entangle_preferences_img_set_grid_lines(priv->prefs, grid);
}


void do_img_embedded_preview_toggled(GtkToggleButton *src, EntanglePreferencesDisplay *preferences)
{
    g_return_if_fail(ENTANGLE_IS_PREFERENCES_DISPLAY(preferences));

    EntanglePreferencesDisplayPrivate *priv = entangle_preferences_display_get_instance_private(preferences);
    gboolean enabled = gtk_toggle_button_get_active(src);

    entangle_preferences_img_set_embedded_preview(priv->prefs, enabled);
}


void do_img_onion_skin_toggled(GtkToggleButton *src, EntanglePreferencesDisplay *preferences)
{
    g_return_if_fail(ENTANGLE_IS_PREFERENCES_DISPLAY(preferences));

    EntanglePreferencesDisplayPrivate *priv = entangle_preferences_display_get_instance_private(preferences);
    gboolean enabled = gtk_toggle_button_get_active(src);

    gtk_widget_set_sensitive(GTK_WIDGET(priv->imgOnionLayers), enabled);
    gtk_widget_set_sensitive(GTK_WIDGET(priv->imgOnionLayersLabel), enabled);

    entangle_preferences_img_set_onion_skin(priv->prefs, enabled);
}


void do_img_onion_layers_changed(GtkSpinButton *src, EntanglePreferencesDisplay *preferences)
{
    g_return_if_fail(ENTANGLE_IS_PREFERENCES_DISPLAY(preferences));

    EntanglePreferencesDisplayPrivate *priv = entangle_preferences_display_get_instance_private(preferences);
    GtkAdjustment *adjust = gtk_spin_button_get_adjustment(src);

    entangle_preferences_img_set_onion_layers(priv->prefs,
                                              gtk_adjustment_get_value(adjust));
}

void do_img_background_changed(GtkColorButton *src, EntanglePreferencesDisplay *preferences)
{
    g_return_if_fail(ENTANGLE_IS_PREFERENCES_DISPLAY(preferences));

    EntanglePreferencesDisplayPrivate *priv = entangle_preferences_display_get_instance_private(preferences);
    GdkRGBA gbg;
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(src), &gbg);
    gchar *bg = gdk_rgba_to_string(&gbg);
    entangle_preferences_img_set_background(priv->prefs, bg);
    g_free(bg);
}

void do_img_highlight_changed(GtkColorButton *src, EntanglePreferencesDisplay *preferences)
{
    g_return_if_fail(ENTANGLE_IS_PREFERENCES_DISPLAY(preferences));

    EntanglePreferencesDisplayPrivate *priv = entangle_preferences_display_get_instance_private(preferences);
    GdkRGBA ghl;
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(src), &ghl);
    gchar *hl = gdk_rgba_to_string(&ghl);
    entangle_preferences_img_set_highlight(priv->prefs, hl);
    g_free(hl);
}


static void entangle_preferences_display_init(EntanglePreferencesDisplay *preferences)
{
    EntanglePreferencesDisplayPrivate *priv = entangle_preferences_display_get_instance_private(preferences);
    GtkListStore *list;
    GtkTreeIter iter;
    GtkCellRenderer *cellImage;
    GtkCellRenderer *cellText;
    GtkTreeViewColumn *colImage;
    GtkTreeViewColumn *colText;
    GtkTreeSelection *selection;
    GtkFileFilter *allFilter;
    GtkFileFilter *iccFilter;
    GtkIconTheme *theme = gtk_icon_theme_get_default();

    gtk_widget_init_template(GTK_WIDGET(preferences));

    g_signal_connect(preferences, "delete-event",
                     G_CALLBACK(do_preferences_delete), preferences);

    gtk_notebook_set_show_tabs(priv->preferencesNotebook, FALSE);

    gtk_widget_set_state_flags(GTK_WIDGET(priv->interfaceBox), GTK_STATE_FLAG_SELECTED, TRUE);
    gtk_image_set_from_icon_name(priv->interfaceImage, "entangle-interface", GTK_ICON_SIZE_DIALOG);

    gtk_widget_set_state_flags(GTK_WIDGET(priv->cmsBox), GTK_STATE_FLAG_SELECTED, TRUE);
    gtk_image_set_from_icon_name(priv->cmsImage, "entangle-color-management", GTK_ICON_SIZE_DIALOG);

    gtk_widget_set_state_flags(GTK_WIDGET(priv->captureBox), GTK_STATE_FLAG_SELECTED, TRUE);
    gtk_image_set_from_icon_name(priv->captureImage, "entangle-capture", GTK_ICON_SIZE_DIALOG);

    gtk_widget_set_state_flags(GTK_WIDGET(priv->pluginsBox), GTK_STATE_FLAG_SELECTED, TRUE);
    gtk_image_set_from_icon_name(priv->pluginsImage, "entangle-plugins", GTK_ICON_SIZE_DIALOG);

    gtk_widget_set_state_flags(GTK_WIDGET(priv->imgBox), GTK_STATE_FLAG_SELECTED, TRUE);
    gtk_image_set_from_icon_name(priv->imgImage, "entangle-imageviewer", GTK_ICON_SIZE_DIALOG);

    list = gtk_list_store_new(3, G_TYPE_INT, G_TYPE_STRING, GDK_TYPE_PIXBUF, -1);

    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, 3,
                       1, _("Interface"),
                       2, gtk_icon_theme_load_icon(theme, "entangle-interface", 22, 0, NULL),
                       -1);

    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, 4,
                       1, _("Image Viewer"),
                       2, gtk_icon_theme_load_icon(theme, "entangle-imageviewer", 22, 0, NULL),
                       -1);

    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, 0,
                       1, _("Capture"),
                       2, gtk_icon_theme_load_icon(theme, "entangle-capture", 22, 0, NULL),
                       -1);

    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, 1,
                       1, _("Color Management"),
                       2, gtk_icon_theme_load_icon(theme, "entangle-color-management", 22, 0, NULL),
                       -1);

    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, 2,
                       1, _("Plugins"),
                       2, gtk_icon_theme_load_icon(theme, "entangle-plugins", 22, 0, NULL),
                       -1);

    cellText = gtk_cell_renderer_text_new();
    cellImage = gtk_cell_renderer_pixbuf_new();

    colText = gtk_tree_view_column_new_with_attributes("Label", cellText, "text", 1, NULL);
    colImage = gtk_tree_view_column_new_with_attributes("Icon", cellImage, "pixbuf", 2, NULL);

    g_object_set(colText, "expand", TRUE, NULL);
    g_object_set(colImage, "expand", FALSE, NULL);

    gtk_tree_view_set_model(priv->preferencesSwitch, GTK_TREE_MODEL(list));
    gtk_tree_view_append_column(priv->preferencesSwitch, colImage);
    gtk_tree_view_append_column(priv->preferencesSwitch, colText);

    selection = gtk_tree_view_get_selection(priv->preferencesSwitch);

    iccFilter = gtk_file_filter_new();
    gtk_file_filter_set_name(iccFilter, _("ICC profiles (*.icc, *.icm)"));
    gtk_file_filter_add_pattern(iccFilter, "*.[Ii][Cc][Cc]");
    gtk_file_filter_add_pattern(iccFilter, "*.[Ii][Cc][Mm]");

    allFilter = gtk_file_filter_new();
    gtk_file_filter_set_name(allFilter, _("All files (*.*)"));
    gtk_file_filter_add_pattern(allFilter, "*");

    g_object_ref(allFilter);
    gtk_file_chooser_add_filter(priv->cmsRgbProfile, allFilter);
    g_object_ref(iccFilter);
    gtk_file_chooser_add_filter(priv->cmsRgbProfile, iccFilter);
    gtk_file_chooser_set_filter(priv->cmsRgbProfile, iccFilter);

    g_object_ref(allFilter);
    gtk_file_chooser_add_filter(priv->cmsMonitorProfile, allFilter);
    g_object_ref(iccFilter);
    gtk_file_chooser_add_filter(priv->cmsMonitorProfile, iccFilter);
    gtk_file_chooser_set_filter(priv->cmsMonitorProfile, iccFilter);

    g_object_unref(iccFilter);
    g_object_unref(allFilter);

    list = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING, -1);

    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "1",
                       1, _("1:1 - Square / MF 6x6"),
                       -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "1.15",
                       1, _("1.15:1 - Movietone"),
                       -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "1.33",
                       1, _("1.33:1 (4:3, 12:9) - Super 35mm / DSLR / MF 645"),
                       -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "1.37",
                       1, _("1.37:1 - 35mm movie"),
                       -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "1.44",
                       1, _("1.44:1 - IMAX"),
                       -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "1.5",
                       1, _("1.50:1 (3:2, 15:10)- 35mm SLR"),
                       -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "1.6",
                       1, _("1.6:1 (8:5, 16:10) - Widescreen"),
                       -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "1.66",
                       1, _("1.66:1 (5:3, 15:9) - Super 16mm"),
                       -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "1.75",
                       1, _("1.75:1 (7:4) - Widescreen"),
                       -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "1.77",
                       1, _("1.77:1 (16:9) - APS-H / HDTV / Widescreen"),
                       -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "1.85",
                       1, _("1.85:1 - 35mm Widescreen"),
                       -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "2.0",
                       1, _("2.00:1 - SuperScope"),
                       -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "2.10",
                       1, _("2.10:1 (21:10) - Planned HDTV"),
                       -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "2.20",
                       1, _("2.20:1 (11:5, 22:10) - 70mm movie"),
                       -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "2.35",
                       1, _("2.35:1 - CinemaScope"),
                       -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "2.37",
                       1, _("2.37:1 (64:27)- HDTV cinema"),
                       -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "2.39",
                       1, _("2.39:1 (12:5)- Panavision"),
                       -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "2.55",
                       1, _("2.55:1 (23:9)- CinemaScope 55"),
                       -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "2.59",
                       1, _("2.59:1 (13:5)- Cinerama"),
                       -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "2.66",
                       1, _("2.66:1 (8:3, 24:9)- Super 16mm"),
                       -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "2.76",
                       1, _("2.76:1 (11:4) - Ultra Panavision"),
                       -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "2.93",
                       1, _("2.93:1 - MGM Camera 65"),
                       -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "3.0",
                       1, _("3:1 APS Panorama"),
                       -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "4.0",
                       1, _("4.00:1 - Polyvision"),
                       -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "12.0",
                       1, _("12.00:1 - Circle-Vision 360"),
                       -1);

    gtk_combo_box_set_model(priv->imgAspectRatio, GTK_TREE_MODEL(list));
    gtk_combo_box_set_id_column(priv->imgAspectRatio, 0);

    cellText = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(priv->imgAspectRatio), cellText, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(priv->imgAspectRatio),
                                   cellText, "text", 1, NULL);


    list = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING, -1);

    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "none",
                       1, _("None"),
                       -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "center-lines",
                       1, _("Center lines"),
                       -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "rule-of-3rds",
                       1, _("Rule of 3rds"),
                       -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "quarters",
                       1, _("Quarters"),
                       -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "rule-of-5ths",
                       1, _("Rule of 5ths"),
                       -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       0, "golden-sections",
                       1, _("Golden sections"),
                       -1);
    gtk_combo_box_set_model(priv->imgGridLines, GTK_TREE_MODEL(list));
    gtk_combo_box_set_id_column(priv->imgGridLines, 0);

    cellText = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(priv->imgGridLines), cellText, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(priv->imgGridLines),
                                   cellText, "text", 1, NULL);

    g_signal_connect(selection, "changed", G_CALLBACK(do_page_changed), preferences);

    g_signal_connect(preferences,
                     "notify::application",
                     G_CALLBACK(do_entangle_preferences_display_set_app),
                     NULL);

}


/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
