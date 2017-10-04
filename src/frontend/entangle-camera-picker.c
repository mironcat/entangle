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

#include "entangle-debug.h"
#include "entangle-camera-picker.h"

gboolean do_picker_close(GtkButton *src,
                         gpointer data);
gboolean do_picker_delete(GtkWidget *src,
                          GdkEvent *ev);

typedef struct _EntangleCameraPickerPrivate EntangleCameraPickerPrivate;
struct _EntangleCameraPickerPrivate {
    EntangleCameraList *cameras;
    gulong addSignal;
    gulong removeSignal;

    GtkListStore *model;

    GtkLabel *warningNoCameras;
    GtkTreeView *cameraList;
    GtkExpander *networkCamera;
    GtkEntry *ipAddress;
    GtkButton *pickerConnect;
};

G_DEFINE_TYPE_WITH_PRIVATE(EntangleCameraPicker, entangle_camera_picker, GTK_TYPE_DIALOG);

enum {
    PROP_O,
    PROP_CAMERAS
};

void do_picker_refresh(GtkButton *src,
                       EntangleCameraPicker *picker);
void do_picker_activate(GtkTreeView *src,
                        GtkTreePath *path,
                        GtkTreeViewColumn *col,
                        EntangleCameraPicker *picker);
void do_picker_connect(GtkButton *src,
                       EntangleCameraPicker *picker);
void do_ip_address_changed(GtkEntry *entry,
                           EntangleCameraPicker *picker);


static void entangle_camera_cell_data_model_func(GtkTreeViewColumn *col G_GNUC_UNUSED,
                                                 GtkCellRenderer *cell,
                                                 GtkTreeModel *model,
                                                 GtkTreeIter *iter,
                                                 gpointer data G_GNUC_UNUSED)
{
    GValue val;
    EntangleCamera *cam;

    memset(&val, 0, sizeof val);

    gtk_tree_model_get_value(model, iter, 0, &val);

    cam = g_value_get_object(&val);

    g_object_set(cell, "text", entangle_camera_get_model(cam), NULL);

    g_object_unref(cam);
}


static void entangle_camera_cell_data_port_func(GtkTreeViewColumn *col G_GNUC_UNUSED,
                                                GtkCellRenderer *cell,
                                                GtkTreeModel *model,
                                                GtkTreeIter *iter,
                                                gpointer data G_GNUC_UNUSED)
{
    GValue val;
    EntangleCamera *cam;

    memset(&val, 0, sizeof val);

    gtk_tree_model_get_value(model, iter, 0, &val);

    cam = g_value_get_object(&val);

    g_object_set(cell, "text", entangle_camera_get_port(cam), NULL);

    g_object_unref(cam);
}


static void entangle_camera_cell_data_capture_func(GtkTreeViewColumn *col G_GNUC_UNUSED,
                                                   GtkCellRenderer *cell,
                                                   GtkTreeModel *model,
                                                   GtkTreeIter *iter,
                                                   gpointer data G_GNUC_UNUSED)
{
    GValue val;
    EntangleCamera *cam;

    memset(&val, 0, sizeof val);

    gtk_tree_model_get_value(model, iter, 0, &val);

    cam = g_value_get_object(&val);

    ENTANGLE_DEBUG("Has %d", entangle_camera_get_has_capture(cam));

    g_object_set(cell, "text", entangle_camera_get_has_capture(cam) ? _("Yes") : _("No"), NULL);

    g_object_unref(cam);
}


static void do_model_sensitivity_update(EntangleCameraPicker *picker)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_PICKER(picker));

    EntangleCameraPickerPrivate *priv = entangle_camera_picker_get_instance_private(picker);

    if (priv->cameras && entangle_camera_list_count(priv->cameras)) {
        int w, h;
        gtk_window_get_default_size(GTK_WINDOW(picker), &w, &h);
        gtk_window_resize(GTK_WINDOW(picker), w, h);
        gtk_widget_set_sensitive(GTK_WIDGET(priv->cameraList), TRUE);
        gtk_widget_hide(GTK_WIDGET(priv->warningNoCameras));
        gtk_expander_set_expanded(priv->networkCamera, FALSE);
    } else {
        gtk_widget_set_sensitive(GTK_WIDGET(priv->cameraList), FALSE);
        gtk_widget_show(GTK_WIDGET(priv->warningNoCameras));
        gtk_expander_set_expanded(priv->networkCamera, TRUE);
        gtk_widget_grab_focus(GTK_WIDGET(priv->ipAddress));
    }
}


static void do_model_refresh(EntangleCameraPicker *picker)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_PICKER(picker));

    EntangleCameraPickerPrivate *priv = entangle_camera_picker_get_instance_private(picker);
    ENTANGLE_DEBUG("Refresh model");
    gtk_list_store_clear(priv->model);

    if (!priv->cameras) {
        do_model_sensitivity_update(picker);
        return;
    }

    for (int i = 0; i < entangle_camera_list_count(priv->cameras); i++) {
        EntangleCamera *cam = entangle_camera_list_get(priv->cameras, i);
        GtkTreeIter iter;

        gtk_list_store_append(priv->model, &iter);

        gtk_list_store_set(priv->model, &iter, 0, cam, -1);

        //g_object_unref(cam);
    }

    do_model_sensitivity_update(picker);
}


static void entangle_camera_picker_get_property(GObject *object,
                                                guint prop_id,
                                                GValue *value,
                                                GParamSpec *pspec)
{
    EntangleCameraPicker *picker = ENTANGLE_CAMERA_PICKER(object);
    EntangleCameraPickerPrivate *priv = entangle_camera_picker_get_instance_private(picker);

    switch (prop_id)
        {
        case PROP_CAMERAS:
            g_value_set_object(value, priv->cameras);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        }
}


static void do_camera_list_add(EntangleCameraList *cameras G_GNUC_UNUSED,
                               EntangleCamera *cam,
                               EntangleCameraPicker *picker)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_PICKER(picker));
    g_return_if_fail(ENTANGLE_IS_CAMERA(cam));

    EntangleCameraPickerPrivate *priv = entangle_camera_picker_get_instance_private(picker);
    GtkTreeIter iter;

    ENTANGLE_DEBUG("Add camrea %p to model", cam);
    gtk_list_store_append(priv->model, &iter);

    gtk_list_store_set(priv->model, &iter, 0, cam, -1);

    do_model_sensitivity_update(picker);
}


static void do_camera_list_remove(EntangleCameraList *cameras G_GNUC_UNUSED,
                                  EntangleCamera *cam,
                                  EntangleCameraPicker *picker)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_PICKER(picker));
    g_return_if_fail(ENTANGLE_IS_CAMERA(cam));

    EntangleCameraPickerPrivate *priv = entangle_camera_picker_get_instance_private(picker);
    GtkTreeIter iter;

    if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->model), &iter))
        return;

    do {
        GValue val;
        EntangleCamera *thiscam;
        memset(&val, 0, sizeof val);

        gtk_tree_model_get_value(GTK_TREE_MODEL(priv->model), &iter, 0, &val);

        thiscam = g_value_get_object(&val);
        g_value_unset(&val);

        if (cam == thiscam) {
            ENTANGLE_DEBUG("Remove camera %p from model", cam);
            gtk_list_store_remove(priv->model, &iter);
            break;
        }

    } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->model), &iter));

    do_model_sensitivity_update(picker);
}


static void entangle_camera_picker_set_property(GObject *object,
                                                guint prop_id,
                                                const GValue *value,
                                                GParamSpec *pspec)
{
    EntangleCameraPicker *picker = ENTANGLE_CAMERA_PICKER(object);

    ENTANGLE_DEBUG("Set prop %d", prop_id);

    switch (prop_id)
        {
        case PROP_CAMERAS:
            entangle_camera_picker_set_camera_list(picker,
                                                   ENTANGLE_CAMERA_LIST(g_value_get_object(value)));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        }
}


static void entangle_camera_picker_finalize(GObject *object)
{
    EntangleCameraPicker *picker = ENTANGLE_CAMERA_PICKER(object);
    EntangleCameraPickerPrivate *priv = entangle_camera_picker_get_instance_private(picker);

    ENTANGLE_DEBUG("Finalize camera picker");

    gtk_list_store_clear(priv->model);
    if (priv->cameras)
        g_object_unref(priv->cameras);
    g_object_unref(priv->model);

    G_OBJECT_CLASS(entangle_camera_picker_parent_class)->finalize(object);
}


static void entangle_camera_picker_class_init(EntangleCameraPickerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = entangle_camera_picker_finalize;
    object_class->get_property = entangle_camera_picker_get_property;
    object_class->set_property = entangle_camera_picker_set_property;

    g_signal_new("picker-connect",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__OBJECT,
                 G_TYPE_NONE,
                 1,
                 G_TYPE_OBJECT);
    g_signal_new("picker-refresh",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE,
                 0);

    g_object_class_install_property(object_class,
                                    PROP_CAMERAS,
                                    g_param_spec_object("cameras",
                                                        "Camera List",
                                                        "List of known camera objects",
                                                        ENTANGLE_TYPE_CAMERA_LIST,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass),
                                                "/org/entangle_photo/Manager/frontend/entangle-camera-picker.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntangleCameraPicker, warningNoCameras);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntangleCameraPicker, cameraList);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntangleCameraPicker, networkCamera);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntangleCameraPicker, ipAddress);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntangleCameraPicker, pickerConnect);
}


EntangleCameraPicker *entangle_camera_picker_new(void)
{
    return ENTANGLE_CAMERA_PICKER(g_object_new(ENTANGLE_TYPE_CAMERA_PICKER, NULL));
}


gboolean do_picker_close(GtkButton *src G_GNUC_UNUSED,
                         gpointer data)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA_PICKER(data), FALSE);

    EntangleCameraPicker *picker = ENTANGLE_CAMERA_PICKER(data);

    ENTANGLE_DEBUG("picker delete");
    gtk_widget_hide(GTK_WIDGET(picker));
    return TRUE;
}

gboolean do_picker_delete(GtkWidget *src,
                          GdkEvent *ev G_GNUC_UNUSED)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA_PICKER(src), FALSE);

    ENTANGLE_DEBUG("picker delete");
    gtk_widget_hide(src);
    return TRUE;
}

void do_picker_refresh(GtkButton *src G_GNUC_UNUSED,
                       EntangleCameraPicker *picker)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_PICKER(picker));

    ENTANGLE_DEBUG("picker refresh %p", picker);
    g_signal_emit_by_name(picker, "picker-refresh", NULL);
}

static EntangleCamera *entangle_picker_get_selected_camera(EntangleCameraPicker *picker)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA_PICKER(picker), NULL);

    EntangleCameraPickerPrivate *priv = entangle_camera_picker_get_instance_private(picker);
    GtkTreeSelection *sel;
    GtkTreeIter iter;
    gboolean selected;
    GValue val;

    ENTANGLE_DEBUG("select camera");

    sel = gtk_tree_view_get_selection(priv->cameraList);

    selected = gtk_tree_selection_get_selected(sel, NULL, &iter);
    if (!selected) {
        const gchar *addr = gtk_entry_get_text(priv->ipAddress);
        if (g_str_equal(addr, ""))
            return NULL;

        gchar *port = g_strdup_printf("ptpip:%s", addr);
        EntangleCamera *cam = entangle_camera_new("PTP/IP Camera", port, FALSE, FALSE, FALSE);
        g_free(port);

        return cam;
    }

    memset(&val, 0, sizeof val);
    gtk_tree_model_get_value(GTK_TREE_MODEL(priv->model), &iter, 0, &val);

    return g_value_get_object(&val);
}


void do_picker_activate(GtkTreeView *src G_GNUC_UNUSED,
                        GtkTreePath *path G_GNUC_UNUSED,
                        GtkTreeViewColumn *col G_GNUC_UNUSED,
                        EntangleCameraPicker *picker)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_PICKER(picker));

    EntangleCamera *cam;
    cam = entangle_picker_get_selected_camera(picker);
    ENTANGLE_DEBUG("picker activate %p %p", picker, cam);

    if (cam) {
        GValue val;
        memset(&val, 0, sizeof val);
        g_value_init(&val, G_TYPE_OBJECT);
        g_value_set_object(&val, cam);
        //g_signal_emit_by_name(picker, "picker-connect", &val);
        g_signal_emit_by_name(picker, "picker-connect", cam);
        g_value_unset(&val);
        g_object_unref(cam);
    }
}


void do_picker_connect(GtkButton *src G_GNUC_UNUSED,
                       EntangleCameraPicker *picker)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_PICKER(picker));

    EntangleCamera *cam;
    cam = entangle_picker_get_selected_camera(picker);
    ENTANGLE_DEBUG("picker connect %p %p", picker, cam);
    if (cam) {
        GValue val;
        memset(&val, 0, sizeof val);
        g_value_init(&val, G_TYPE_OBJECT);
        g_value_set_object(&val, cam);
        //g_signal_emit_by_name(picker, "picker-connect", &val);
        g_signal_emit_by_name(picker, "picker-connect", cam);
        g_value_unset(&val);
        g_object_unref(cam);
    }
}


static void do_camera_select(GtkTreeSelection *sel, EntangleCameraPicker *picker)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_PICKER(picker));

    EntangleCameraPickerPrivate *priv = entangle_camera_picker_get_instance_private(picker);
    GtkTreeIter iter;
    gboolean selected;

    ENTANGLE_DEBUG("selection changed");

    selected = gtk_tree_selection_get_selected(sel, NULL, &iter);

    if (selected) {
        gtk_expander_set_expanded(priv->networkCamera, FALSE);
        gtk_entry_set_text(priv->ipAddress, "");
    }

    gtk_widget_set_sensitive(GTK_WIDGET(priv->pickerConnect), selected);
}


void do_ip_address_changed(GtkEntry *entry, EntangleCameraPicker *picker)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_PICKER(picker));

    EntangleCameraPickerPrivate *priv = entangle_camera_picker_get_instance_private(picker);
    GtkTreeSelection *sel;
    GtkTreeIter iter;
    gboolean selected;
    const gchar *addr;

    addr = gtk_entry_get_text(entry);

    sel = gtk_tree_view_get_selection(priv->cameraList);
    selected = gtk_tree_selection_get_selected(sel, NULL, &iter);
    if (!g_str_equal(addr, "") && selected)
        gtk_tree_selection_unselect_all(sel);

    gtk_widget_set_sensitive(GTK_WIDGET(priv->pickerConnect), g_hostname_is_ip_address(addr));
}


static void entangle_camera_picker_init(EntangleCameraPicker *picker)
{
    EntangleCameraPickerPrivate *priv = entangle_camera_picker_get_instance_private(picker);
    GtkCellRenderer *model;
    GtkCellRenderer *port;
    GtkCellRenderer *capture;
    GtkTreeViewColumn *modelCol;
    GtkTreeViewColumn *portCol;
    GtkTreeViewColumn *captureCol;
    GtkTreeSelection *sel;

    gtk_widget_init_template(GTK_WIDGET(picker));

    g_assert(priv->pickerConnect);
    g_assert(priv->ipAddress);
    g_assert(priv->networkCamera);
    g_assert(priv->cameraList);
    g_assert(priv->warningNoCameras);

    priv->model = gtk_list_store_new(1, G_TYPE_OBJECT);

    model = gtk_cell_renderer_text_new();
    port = gtk_cell_renderer_text_new();
    capture = gtk_cell_renderer_text_new();

    modelCol = gtk_tree_view_column_new_with_attributes(_("Model"), model, NULL);
    portCol = gtk_tree_view_column_new_with_attributes(_("Port"), port, NULL);
    captureCol = gtk_tree_view_column_new_with_attributes(_("Capture"), capture, NULL);

    g_object_set(modelCol, "expand", TRUE, NULL);
    g_object_set(portCol, "expand", FALSE, NULL);
    g_object_set(captureCol, "expand", FALSE, NULL);

    gtk_tree_view_append_column(priv->cameraList, modelCol);
    gtk_tree_view_append_column(priv->cameraList, portCol);
    gtk_tree_view_append_column(priv->cameraList, captureCol);

    gtk_tree_view_column_set_cell_data_func(modelCol, model, entangle_camera_cell_data_model_func, NULL, NULL);
    gtk_tree_view_column_set_cell_data_func(portCol, port, entangle_camera_cell_data_port_func, NULL, NULL);
    gtk_tree_view_column_set_cell_data_func(captureCol, capture, entangle_camera_cell_data_capture_func, NULL, NULL);

    gtk_tree_view_set_model(priv->cameraList, GTK_TREE_MODEL(priv->model));

    sel = gtk_tree_view_get_selection(priv->cameraList);

    g_signal_connect(sel, "changed", G_CALLBACK(do_camera_select), picker);

    gtk_widget_set_sensitive(GTK_WIDGET(priv->pickerConnect), FALSE);

    do_model_sensitivity_update(picker);
}


void entangle_camera_picker_set_camera_list(EntangleCameraPicker *picker,
                                            EntangleCameraList *cameras)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_PICKER(picker));

    EntangleCameraPickerPrivate *priv = entangle_camera_picker_get_instance_private(picker);

    if (priv->cameras) {
        g_signal_handler_disconnect(priv->cameras, priv->addSignal);
        g_signal_handler_disconnect(priv->cameras, priv->removeSignal);
        g_object_unref(priv->cameras);
    }
    priv->cameras = g_object_ref(cameras);
    priv->addSignal = g_signal_connect(priv->cameras, "camera-added",
                                       G_CALLBACK(do_camera_list_add), picker);
    priv->removeSignal = g_signal_connect(priv->cameras, "camera-removed",
                                          G_CALLBACK(do_camera_list_remove), picker);
    do_model_refresh(picker);
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
