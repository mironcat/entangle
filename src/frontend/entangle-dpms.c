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

#include "entangle-dpms.h"
#include "entangle-debug.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#if defined(GDK_WINDOWING_X11)
#include <X11/Xlib.h>
#include <X11/extensions/dpms.h>

#include <gdk/gdkx.h>
#endif

#define ENTANGLE_DPMS_ERROR entangle_dpms_error_quark ()

static GQuark entangle_dpms_error_quark(void)
{
    return g_quark_from_static_string("entangle-dpms");
}


gboolean entangle_dpms_set_blanking(gboolean enabled,
                                    GError **error)
{
    GdkDisplay *dpy = gdk_display_get_default();

    ENTANGLE_DEBUG("Toggle set blanking %d", enabled);

#if defined(GDK_WINDOWING_X11)
    if (GDK_IS_X11_DISPLAY(dpy)) {
        Display *xdpy = gdk_x11_display_get_xdisplay(dpy);
        int ignore1, ignore2;

        if (!DPMSQueryExtension(xdpy, &ignore1, &ignore2) ||
            !DPMSCapable(xdpy)) {
            g_set_error(error, ENTANGLE_DPMS_ERROR, 0, "%s",
                        _("Screen blanking is not available on this display"));
            return FALSE;
        }

        DPMSEnable(xdpy);
        DPMSForceLevel(xdpy, enabled ? DPMSModeStandby : DPMSModeOn);

        return TRUE;
    }
#endif

    g_set_error(error, ENTANGLE_DPMS_ERROR, 0, "%s",
                _("Screen blanking is not implemented on this platform"));
    return FALSE;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
