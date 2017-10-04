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
#include <unistd.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "entangle-debug.h"
#include "entangle-camera-support.h"


gboolean do_support_close(GtkButton *src,
                          gpointer data);
gboolean do_support_delete(GtkWidget *src,
                           GdkEvent *ev);

typedef struct _EntangleCameraSupportPrivate EntangleCameraSupportPrivate;
struct _EntangleCameraSupportPrivate {
    EntangleCameraList *cameraList;

    GtkLabel *infoText;
};

G_DEFINE_TYPE_WITH_PRIVATE(EntangleCameraSupport, entangle_camera_support, GTK_TYPE_DIALOG);

enum {
    PROP_O,
    PROP_CAMERA_LIST,
};


static void entangle_camera_support_get_property(GObject *object,
                                                 guint prop_id,
                                                 GValue *value,
                                                 GParamSpec *pspec)
{
    EntangleCameraSupport *support = ENTANGLE_CAMERA_SUPPORT(object);
    EntangleCameraSupportPrivate *priv = entangle_camera_support_get_instance_private(support);

    switch (prop_id)
        {
        case PROP_CAMERA_LIST:
            g_value_set_object(value, priv->cameraList);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        }
}


static void entangle_camera_support_set_property(GObject *object,
                                                 guint prop_id,
                                                 const GValue *value,
                                                 GParamSpec *pspec)
{
    EntangleCameraSupport *support = ENTANGLE_CAMERA_SUPPORT(object);

    ENTANGLE_DEBUG("Set prop %d", prop_id);

    switch (prop_id)
        {
        case PROP_CAMERA_LIST:
            entangle_camera_support_set_camera_list(support, g_value_get_object(value));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        }
}


static void entangle_camera_support_finalize(GObject *object)
{
    EntangleCameraSupport *support = ENTANGLE_CAMERA_SUPPORT(object);
    EntangleCameraSupportPrivate *priv = entangle_camera_support_get_instance_private(support);

    if (priv->cameraList)
        g_object_unref(priv->cameraList);

    G_OBJECT_CLASS(entangle_camera_support_parent_class)->finalize(object);
}


static void entangle_camera_support_class_init(EntangleCameraSupportClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = entangle_camera_support_finalize;
    object_class->get_property = entangle_camera_support_get_property;
    object_class->set_property = entangle_camera_support_set_property;

    g_object_class_install_property(object_class,
                                    PROP_CAMERA_LIST,
                                    g_param_spec_object("camera-list",
                                                        "Camera List",
                                                        "Camera list to query",
                                                        ENTANGLE_TYPE_CAMERA_LIST,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass),
                                                "/org/entangle_photo/Manager/frontend/entangle-camera-support.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass),
                                                 EntangleCameraSupport, infoText);
}


EntangleCameraSupport *entangle_camera_support_new(void)
{
    return ENTANGLE_CAMERA_SUPPORT(g_object_new(ENTANGLE_TYPE_CAMERA_SUPPORT, NULL));
}


gboolean do_support_close(GtkButton *src G_GNUC_UNUSED,
                          gpointer data)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA_SUPPORT(data), FALSE);

    EntangleCameraSupport *support = ENTANGLE_CAMERA_SUPPORT(data);

    ENTANGLE_DEBUG("support close");

    gtk_widget_hide(GTK_WIDGET(support));
    return FALSE;
}

gboolean do_support_delete(GtkWidget *src,
                           GdkEvent *ev G_GNUC_UNUSED)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA_SUPPORT(src), FALSE);

    ENTANGLE_DEBUG("support delete");

    gtk_widget_hide(src);
    return FALSE;
}


static void entangle_camera_support_init(EntangleCameraSupport *support)
{
    gtk_widget_init_template(GTK_WIDGET(support));
}


static void do_support_refresh(EntangleCameraSupport *support)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_SUPPORT(support));

    EntangleCameraSupportPrivate *priv = entangle_camera_support_get_instance_private(support);

    if (priv->cameraList) {
        GList *cameras, *tmp;
        GString *str = g_string_new("");
        cameras = entangle_camera_list_get_cameras(priv->cameraList);
        tmp = cameras = g_list_reverse(cameras);
        while (tmp) {
            EntangleCamera *cam = tmp->data;
            gboolean cap = entangle_camera_get_has_capture(cam);
            gboolean pre = entangle_camera_get_has_preview(cam);
            gboolean set = entangle_camera_get_has_settings(cam);
            if (cap || pre) {
                gboolean done = FALSE;
                g_string_append(str, entangle_camera_get_model(cam));
                g_string_append(str, " (");
                if (cap) {
                    g_string_append(str, _("capture"));
                    done = TRUE;
                }
                if (pre) {
                    if (done)
                        g_string_append(str, ", ");
                    g_string_append(str, _("preview"));
                    done = TRUE;
                }
                if (set) {
                    if (done)
                        g_string_append(str, ", ");
                    g_string_append(str, _("settings"));
                }
                g_string_append(str, ")");
                g_string_append(str, "\n");
            }
            tmp = tmp->next;
        }
        g_list_free(cameras);
        gtk_label_set_text(priv->infoText, str->str);
        g_string_free(str, TRUE);
    } else {
        gtk_label_set_text(priv->infoText, "");
    }
}


/**
 * entangle_camera_support_set_camera_list:
 * @support: the camera support widget
 * @list: (transfer none)(allow-none): the list of cameras
 *
 * Set the camera list for the widget
 */
void entangle_camera_support_set_camera_list(EntangleCameraSupport *support,
                                             EntangleCameraList *list)
{
    g_return_if_fail(ENTANGLE_IS_CAMERA_SUPPORT(support));
    g_return_if_fail(ENTANGLE_IS_CAMERA_LIST(list));

    EntangleCameraSupportPrivate *priv = entangle_camera_support_get_instance_private(support);

    if (priv->cameraList)
        g_object_unref(priv->cameraList);
    priv->cameraList = list;
    if (priv->cameraList)
        g_object_ref(priv->cameraList);

    do_support_refresh(support);
}


/**
 * entangle_camera_support_get_camera_list:
 * @support: the camera support widget
 *
 * Get the camera list
 *
 * Returns: (transfer none): the camera list or NULL
 */
EntangleCameraList *entangle_camera_support_get_camera_list(EntangleCameraSupport *support)
{
    g_return_val_if_fail(ENTANGLE_IS_CAMERA_SUPPORT(support), NULL);

    EntangleCameraSupportPrivate *priv = entangle_camera_support_get_instance_private(support);

    return priv->cameraList;
}


/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
