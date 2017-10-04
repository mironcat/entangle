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

#include <unistd.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "entangle-debug.h"
#include "entangle-help-about.h"


gboolean do_about_delete(GtkWidget *src,
                         GdkEvent *ev,
                         gpointer data);


G_DEFINE_TYPE(EntangleHelpAbout, entangle_help_about, GTK_TYPE_ABOUT_DIALOG);


static void entangle_help_about_class_init(EntangleHelpAboutClass *klass)
{
    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass),
                                                "/org/entangle_photo/Manager/frontend/entangle-help-about.ui");
}


EntangleHelpAbout *entangle_help_about_new(void)
{
    return ENTANGLE_HELP_ABOUT(g_object_new(ENTANGLE_TYPE_HELP_ABOUT, NULL));
}


static void do_about_response(GtkDialog *dialog,
                              gint response G_GNUC_UNUSED,
                              gpointer data G_GNUC_UNUSED)
{
    g_return_if_fail(ENTANGLE_IS_HELP_ABOUT(dialog));

    ENTANGLE_DEBUG("about response");

    gtk_widget_hide(GTK_WIDGET(dialog));
}


gboolean do_about_delete(GtkWidget *src,
                         GdkEvent *ev G_GNUC_UNUSED,
                         gpointer data G_GNUC_UNUSED)
{
    g_return_val_if_fail(ENTANGLE_IS_HELP_ABOUT(src), FALSE);

    ENTANGLE_DEBUG("about delete");

    gtk_widget_hide(src);
    return TRUE;
}

static void entangle_help_about_init(EntangleHelpAbout *about)
{
    gtk_widget_init_template(GTK_WIDGET(about));

    g_signal_connect(about, "response", G_CALLBACK(do_about_response), about);

    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(about), VERSION);
}


/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
