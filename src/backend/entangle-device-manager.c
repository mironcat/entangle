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

#define G_UDEV_API_IS_SUBJECT_TO_CHANGE
#ifdef G_UDEV_API_IS_SUBJECT_TO_CHANGE
#include <gudev/gudev.h>
#endif

#include "entangle-debug.h"
#include "entangle-device-manager.h"

struct _EntangleDeviceManager
{
    GObject parent;
    GUdevClient *ctx;
};

G_DEFINE_TYPE(EntangleDeviceManager, entangle_device_manager, G_TYPE_OBJECT);

static void
entangle_device_manager_finalize(GObject *object)
{
    EntangleDeviceManager *manager = ENTANGLE_DEVICE_MANAGER(object);

    ENTANGLE_DEBUG("Finalize manager");

    if (manager->ctx)
        g_object_unref(manager->ctx);

    G_OBJECT_CLASS(entangle_device_manager_parent_class)->finalize(object);
}

static void
entangle_device_manager_class_init(EntangleDeviceManagerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = entangle_device_manager_finalize;

    g_signal_new("device-added", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST,
                 0, NULL, NULL, g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1,
                 G_TYPE_STRING, G_TYPE_STRING);

    g_signal_new("device-removed", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST,
                 0, NULL, NULL, g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1,
                 G_TYPE_STRING, G_TYPE_STRING);
}

static void
do_udev_event(GUdevClient *client G_GNUC_UNUSED,
              const char *action,
              GUdevDevice *dev,
              gpointer opaque)
{
    EntangleDeviceManager *manager = opaque;
    const gchar *sysfs;
    const gchar *usbbus, *usbdev;
    const gchar *devtype;
    const gchar *serial;
    gchar *port;

    if (strcmp(action, "add") != 0 && strcmp(action, "remove") != 0)
        return;

    devtype = g_udev_device_get_devtype(dev);
    if ((devtype == NULL) || strcmp(devtype, "usb_device") != 0)
        return;

    sysfs = g_udev_device_get_sysfs_path(dev);

    usbbus = g_udev_device_get_property(dev, "BUSNUM");
    usbdev = g_udev_device_get_property(dev, "DEVNUM");
    serial = g_udev_device_get_property(dev, "ID_SERIAL");

    if (sysfs == NULL || usbbus == NULL || usbdev == NULL)
        return;

    port = g_strdup_printf("usb:%s,%s", usbbus, usbdev);

    ENTANGLE_DEBUG("%s device '%s' '%s'", action, sysfs, port);

    if (strcmp(action, "add") == 0) {
        g_signal_emit_by_name(manager, "device-added", port, serial);
    } else {
        g_signal_emit_by_name(manager, "device-removed", port, serial);
    }
    g_free(port);
}

EntangleDeviceManager *
entangle_device_manager_new(void)
{
    return ENTANGLE_DEVICE_MANAGER(
        g_object_new(ENTANGLE_TYPE_DEVICE_MANAGER, NULL));
}

static void
entangle_device_manager_init_devices(EntangleDeviceManager *manager)
{
    GList *devs, *tmp;
    const gchar *const subsys[] = {
        "usb/usb_device",
        NULL,
    };

    ENTANGLE_DEBUG("Init udev");

    manager->ctx = g_udev_client_new(subsys);

    g_signal_connect(manager->ctx, "uevent", G_CALLBACK(do_udev_event),
                     manager);

    devs = g_udev_client_query_by_subsystem(manager->ctx, "usb");

    tmp = devs;
    while (tmp) {
        GUdevDevice *dev = tmp->data;

        do_udev_event(manager->ctx, "add", dev, manager);

        g_object_unref(dev);
        tmp = tmp->next;
    }

    g_list_free(devs);
}

static void
entangle_device_manager_init(EntangleDeviceManager *manager)
{
    entangle_device_manager_init_devices(manager);
}

char *
entangle_device_manager_serial_id(EntangleDeviceManager *manager,
                                  const char *devpath)
{
    GUdevDevice *dev;
    char *ret;

    dev = g_udev_client_query_by_device_file(manager->ctx, devpath);

    if (!dev)
        return NULL;

    ret = g_strdup(g_udev_device_get_property(dev, "ID_SERIAL"));
    g_object_unref(dev);
    return ret;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
