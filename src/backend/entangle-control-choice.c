/*
 *  Entangle: Tethered Camera Control & Capture
 *
 *  Copyright (C) 2009-2018 Daniel P. Berrange
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

#include "entangle-debug.h"
#include "entangle-control-choice.h"

struct _EntangleControlChoice {
    EntangleControl parent;
    char *value;
    size_t nentries;
    char **entries;
};

G_DEFINE_TYPE(EntangleControlChoice, entangle_control_choice, ENTANGLE_TYPE_CONTROL);

enum {
    PROP_0,
    PROP_VALUE,
};

static void entangle_control_get_property(GObject *object,
                                          guint prop_id,
                                          GValue *value,
                                          GParamSpec *pspec)
{
    EntangleControlChoice *control = ENTANGLE_CONTROL_CHOICE(object);

    switch (prop_id)
        {
        case PROP_VALUE:
            g_value_set_string(value, control->value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        }
}

static void entangle_control_set_property(GObject *object,
                                          guint prop_id,
                                          const GValue *value,
                                          GParamSpec *pspec)
{
    EntangleControlChoice *control = ENTANGLE_CONTROL_CHOICE(object);
    gchar *newvalue;

    switch (prop_id)
        {
        case PROP_VALUE:
            newvalue = g_value_dup_string(value);
            if ((newvalue && !control->value) ||
                (!newvalue && control->value) ||
                (newvalue && control->value &&
                 !g_str_equal(newvalue, control->value))) {
                g_free(control->value);
                control->value = newvalue;
                entangle_control_set_dirty(ENTANGLE_CONTROL(object), TRUE);
            } else {
                g_free(newvalue);
            }
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        }
}


static void entangle_control_choice_finalize(GObject *object)
{
    EntangleControlChoice *control = ENTANGLE_CONTROL_CHOICE(object);

    for (int i = 0; i < control->nentries; i++)
        g_free(control->entries[i]);
    g_free(control->entries);

    G_OBJECT_CLASS(entangle_control_choice_parent_class)->finalize(object);
}

static void entangle_control_choice_class_init(EntangleControlChoiceClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = entangle_control_choice_finalize;
    object_class->get_property = entangle_control_get_property;
    object_class->set_property = entangle_control_set_property;

    g_object_class_install_property(object_class,
                                    PROP_VALUE,
                                    g_param_spec_string("value",
                                                        "Control value",
                                                        "Current value of the control",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));
}


EntangleControlChoice *entangle_control_choice_new(const char *path,
                                                   int id,
                                                   const char *label,
                                                   const char *info,
                                                   gboolean readonly)
{
    g_return_val_if_fail(path != NULL, NULL);
    g_return_val_if_fail(label != NULL, NULL);

    return ENTANGLE_CONTROL_CHOICE(g_object_new(ENTANGLE_TYPE_CONTROL_CHOICE,
                                                "path", path,
                                                "id", id,
                                                "label", label,
                                                "info", info,
                                                "readonly", readonly,
                                                NULL));
}


static void entangle_control_choice_init(EntangleControlChoice *control G_GNUC_UNUSED)
{
}

void entangle_control_choice_add_entry(EntangleControlChoice *control,
                                       const char *entry)
{
    g_return_if_fail(ENTANGLE_IS_CONTROL_CHOICE(control));
    g_return_if_fail(entry != NULL);

    control->entries = g_renew(char *, control->entries, control->nentries+1);
    control->entries[control->nentries++] = g_strdup(entry);
}


void entangle_control_choice_clear_entries(EntangleControlChoice *control)
{
    g_return_if_fail(ENTANGLE_IS_CONTROL_CHOICE(control));

    for (int i = 0; i < control->nentries; i++)
        g_free(control->entries[i]);
    g_free(control->entries);
    control->entries = NULL;
    control->nentries = 0;
}


int entangle_control_choice_entry_count(EntangleControlChoice *control)
{
    g_return_val_if_fail(ENTANGLE_IS_CONTROL_CHOICE(control), 0);

    return control->nentries;
}

const char *entangle_control_choice_entry_get(EntangleControlChoice *control,
                                              int idx)
{
    g_return_val_if_fail(ENTANGLE_IS_CONTROL_CHOICE(control), NULL);

    if (idx < 0 || idx >= control->nentries)
        return NULL;

    return control->entries[idx];
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
