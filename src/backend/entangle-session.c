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

#include <glib.h>
#include <gio/gio.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#include "entangle-debug.h"
#include "entangle-session.h"
#include "entangle-image.h"
#include "entangle-video.h"

struct _EntangleSession {
    GObject parent;
    char *directory;
    char *filenamePattern;
    gboolean recalculateDigit;
    int nextFilenameDigit;

    char *lastFilePrefixSrc;
    char *lastFilePrefixDst;

    GList *files;
};

G_DEFINE_TYPE(EntangleSession, entangle_session, G_TYPE_OBJECT);

enum {
    PROP_0,
    PROP_DIRECTORY,
    PROP_FILENAME_PATTERN,
};

static void entangle_session_get_property(GObject *object,
                                          guint prop_id,
                                          GValue *value,
                                          GParamSpec *pspec)
{
    EntangleSession *session = ENTANGLE_SESSION(object);

    switch (prop_id) {
    case PROP_DIRECTORY:
        g_value_set_string(value, session->directory);
        break;

    case PROP_FILENAME_PATTERN:
        g_value_set_string(value, session->filenamePattern);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void entangle_session_set_property(GObject *object,
                                          guint prop_id,
                                          const GValue *value,
                                          GParamSpec *pspec)
{
    EntangleSession *session = ENTANGLE_SESSION(object);

    switch (prop_id) {
    case PROP_DIRECTORY:
        g_free(session->directory);
        session->directory = g_value_dup_string(value);
        g_mkdir_with_parents(session->directory, 0777);
        break;

    case PROP_FILENAME_PATTERN:
        g_free(session->filenamePattern);
        session->filenamePattern = g_value_dup_string(value);
        session->recalculateDigit = TRUE;
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}


static void do_media_unref(gpointer object,
                           gpointer opaque G_GNUC_UNUSED)
{
    EntangleMedia *media = object;

    g_object_unref(media);
}

static void entangle_session_finalize(GObject *object)
{
    EntangleSession *session = ENTANGLE_SESSION(object);

    ENTANGLE_DEBUG("Finalize session %p", object);

    if (session->files) {
        g_list_foreach(session->files, do_media_unref, NULL);
        g_list_free(session->files);
    }

    g_free(session->lastFilePrefixSrc);
    g_free(session->lastFilePrefixDst);
    g_free(session->filenamePattern);
    g_free(session->directory);

    G_OBJECT_CLASS(entangle_session_parent_class)->finalize(object);
}


static void entangle_session_class_init(EntangleSessionClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = entangle_session_finalize;
    object_class->get_property = entangle_session_get_property;
    object_class->set_property = entangle_session_set_property;

    g_signal_new("session-media-added",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__OBJECT,
                 G_TYPE_NONE,
                 1,
                 ENTANGLE_TYPE_MEDIA);

    g_signal_new("session-media-removed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_FIRST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__OBJECT,
                 G_TYPE_NONE,
                 1,
                 ENTANGLE_TYPE_MEDIA);

    g_object_class_install_property(object_class,
                                    PROP_DIRECTORY,
                                    g_param_spec_string("directory",
                                                        "Session directory",
                                                        "Full path to session file",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));

    g_object_class_install_property(object_class,
                                    PROP_FILENAME_PATTERN,
                                    g_param_spec_string("filename-pattern",
                                                        "Filename patern",
                                                        "Pattern for creating new filenames",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));
}


/**
 * entangle_session_new:
 * @directory: the directory associated witht session
 * @filenamePattern: the filename generator pattern
 *
 * Create a new sesssion tracking media files present in
 * @directory. The @filenamePattern is used to generate
 * filenames for newly created files
 *
 * Returns: (transfer full): the new session
 */
EntangleSession *entangle_session_new(const char *directory,
                                      const char *filenamePattern)
{
    return ENTANGLE_SESSION(g_object_new(ENTANGLE_TYPE_SESSION,
                                         "directory", directory,
                                         "filename-pattern", filenamePattern,
                                         NULL));
}


static void entangle_session_init(EntangleSession *session)
{
    session->recalculateDigit = TRUE;
}


/**
 * entangle_session_directory:
 * @session: (transfer none): the session instance
 *
 * Get the directory associated with the session
 *
 * Returns: (transfer none): the session directory
 */
const char *entangle_session_directory(EntangleSession *session)
{
    g_return_val_if_fail(ENTANGLE_IS_SESSION(session), NULL);

    return session->directory;
}


/**
 * entangle_session_filename_pattern:
 * @session: (transfer none): the session instance
 *
 * Get the filename generator pattern
 *
 * Returns: (transfer none): the filename pattern
 */
const char *entangle_session_filename_pattern(EntangleSession *session)
{
    g_return_val_if_fail(ENTANGLE_IS_SESSION(session), NULL);

    return session->filenamePattern;
}


static gint entangle_session_next_digit(EntangleSession *session)
{
    gint maxDigit = -1;
    GList *files = session->files;
    const gchar *template = strchr(session->filenamePattern, 'X');
    gchar *prefix = g_strndup(session->filenamePattern, (template - session->filenamePattern));
    gint prefixlen = strlen(prefix);
    gsize templatelen = 0;
    const gchar *postfix;
    gsize postfixlen;

    while (*template == 'X') {
        templatelen++;
        template++;
    }

    postfix = template;
    postfixlen = strlen(postfix);

    ENTANGLE_DEBUG("Template '%s' with prefixlen %d, %zu digits and postfix %zu",
                   session->filenamePattern, prefixlen, templatelen, postfixlen);

    while (files) {
        EntangleMedia *media = files->data;
        const gchar *name = entangle_media_get_filename(media);
        gsize used = 0;
        gint digit = 0;

        if (!g_str_has_prefix(name, session->directory)) {
            ENTANGLE_DEBUG("File %s does not match directory",
                           entangle_media_get_filename(media));
            goto next;
        }
        name += strlen(session->directory);
        while (*name == '/')
            name++;

        /* Ignore files not matching the template prefix */
        if (!g_str_has_prefix(name, prefix)) {
            ENTANGLE_DEBUG("File %s does not match prefix",
                           entangle_media_get_filename(media));
            goto next;
        }

        name += prefixlen;

        /* Skip over filename matching digits */
        while (used < templatelen && g_ascii_isdigit(*name)) {
            digit *= 10;
            digit += *name - '0';
            name++;
            used++;
        }

        /* See if unexpectedly got a non-digit before end of template */
        if (used < templatelen) {
            ENTANGLE_DEBUG("File %s has too few digits",
                           entangle_media_get_filename(media));
            goto next;
        }

        if (!g_str_has_prefix(name, postfix)) {
            ENTANGLE_DEBUG("File %s does not match postfix",
                           entangle_media_get_filename(media));
            goto next;
        }

        name += postfixlen;

        /* Verify there is a file extension following the digits */
        if (*name != '.') {
            ENTANGLE_DEBUG("File %s has trailing data",
                           entangle_media_get_filename(media));
            goto next;
        }

        if (digit > maxDigit)
            maxDigit = digit;
        ENTANGLE_DEBUG("File %s matches maxDigit is %d",
                       entangle_media_get_filename(media), maxDigit);

    next:
        files = files->next;
    }

    g_free(prefix);

    return maxDigit + 1;
}


static char *entangle_session_next_file_prefix(EntangleSession *session)
{
    const char *template = strchr(session->filenamePattern, 'X');
    const char *postfix;
    char *prefix;
    char *format;
    char *filename;
    int ndigits;

    ENTANGLE_DEBUG("NEXT FILENAME '%s'", template);

    if (!template)
        return NULL;

    if (session->recalculateDigit) {
        session->nextFilenameDigit = entangle_session_next_digit(session);
        session->recalculateDigit = FALSE;
    }

    postfix = template;
    while (*postfix == 'X')
        postfix++;

    prefix = g_strndup(session->filenamePattern,
                       template - session->filenamePattern);

    ndigits = postfix - template;

    format = g_strdup_printf("%%s%%0%dd%%s", ndigits);

    ENTANGLE_DEBUG("Format '%s' prefix='%s' postfix='%s' ndigits=%d nextDigits=%d",
                   format, prefix, postfix, ndigits, session->nextFilenameDigit);

    filename = g_strdup_printf(format,
                               prefix,
                               session->nextFilenameDigit,
                               postfix);

    session->nextFilenameDigit++;

    g_free(prefix);
    g_free(format);

    return filename;
}


/**
 * entangle_session_next_filename:
 * @session: (transfer none): the session instance
 * @file: (transfer none): the file to obtain a filename for
 *
 * Generate a new unique filename for @file, taking into
 * account its file extension and any previously generated
 * filename.
 *
 * Returns: (transfer full): the new filename
 */
char *entangle_session_next_filename(EntangleSession *session,
                                     EntangleCameraFile *file)
{
    g_return_val_if_fail(ENTANGLE_IS_SESSION(session), NULL);

    const char *srcname = entangle_camera_file_get_name(file);
    char *dstname = NULL;
    char *ext = NULL;
    char *srcprefix = NULL;

    ENTANGLE_DEBUG("Next filename %s against (%s, %s)",
                   srcname,
                   session->lastFilePrefixSrc ? session->lastFilePrefixSrc : "",
                   session->lastFilePrefixDst ? session->lastFilePrefixDst : "");

    if (srcname) {
        const char *offset = strrchr(srcname, '.');
        if (!offset) {
            srcprefix = g_strdup(srcname);
        } else {
            srcprefix = g_strndup(srcname, (offset - srcname));
            ext = g_ascii_strdown(offset + 1, -1);
        }
    }

    if (srcprefix &&
        session->lastFilePrefixSrc &&
        session->lastFilePrefixDst &&
        g_str_equal(session->lastFilePrefixSrc, srcprefix)) {
    } else {
        g_free(session->lastFilePrefixSrc);
        g_free(session->lastFilePrefixDst);
        session->lastFilePrefixSrc = srcprefix;
        session->lastFilePrefixDst = entangle_session_next_file_prefix(session);
    }

    if (session->lastFilePrefixDst)
        dstname = g_strdup_printf("%s/%s.%s",
                                  session->directory,
                                  session->lastFilePrefixDst,
                                  ext ? ext : "jpeg");
    g_free(ext);

    ENTANGLE_DEBUG("Built '%s'", dstname);

    if (access(dstname, R_OK) == 0 || errno != ENOENT) {
        ENTANGLE_DEBUG("Filename %s unexpectedly exists", dstname);
        g_free(dstname);
        dstname = NULL;
    }

    return dstname;
}


static gint entangle_session_compare_media(gconstpointer a,
                                           gconstpointer b)
{
    EntangleMedia *mediaa = (EntangleMedia *)a;
    EntangleMedia *mediab = (EntangleMedia *)b;

    return entangle_media_get_last_modified(mediaa) -
        entangle_media_get_last_modified(mediab);
}

/**
 * entangle_session_add_media:
 * @session: (transfer none): the session instance
 * @media: (transfer none): the media file to add to the session
 *
 * Add @media to the @session
 */
void entangle_session_add_media(EntangleSession *session, EntangleMedia *media)
{
    g_return_if_fail(ENTANGLE_IS_SESSION(session));
    g_return_if_fail(ENTANGLE_IS_MEDIA(media));

    g_object_ref(media);

    session->files = g_list_insert_sorted(session->files,
                                        media,
                                        entangle_session_compare_media);

    g_signal_emit_by_name(session, "session-media-added", media);
}


/**
 * entangle_session_remove_media:
 * @session: (transfer none): the session instance
 * @media: (transfer none): the media to remove from the session
 *
 * Remove @media from the @session
 */
void entangle_session_remove_media(EntangleSession *session, EntangleMedia *media)
{
    g_return_if_fail(ENTANGLE_IS_SESSION(session));
    g_return_if_fail(ENTANGLE_IS_MEDIA(media));

    GList *tmp = g_list_find(session->files, media);

    if (!tmp)
        return;

    session->files = g_list_delete_link(session->files, tmp);

    g_signal_emit_by_name(session, "session-media-removed", media);
    g_object_unref(media);
}


/**
 * entangle_session_load:
 * @session: (transfer none): the session instance
 *
 * Load all the files present in the directory associated
 * with the session
 *
 * Returns: TRUE if the session was loaded
 */
gboolean entangle_session_load(EntangleSession *session)
{
    g_return_val_if_fail(ENTANGLE_IS_SESSION(session), FALSE);

    GFile *dir = g_file_new_for_path(session->directory);
    GList *tmp;
    GFileEnumerator *children = g_file_enumerate_children(dir,
                                                          "standard::name,standard::type",
                                                          G_FILE_QUERY_INFO_NONE,
                                                          NULL,
                                                          NULL);
    GFileInfo *childinfo;
    while ((childinfo = g_file_enumerator_next_file(children, NULL, NULL)) != NULL) {
        const gchar *thisname = g_file_info_get_name(childinfo);
        GFile *child = g_file_get_child(dir, thisname);
        if (g_file_info_get_file_type(childinfo) == G_FILE_TYPE_REGULAR ||
            g_file_info_get_file_type(childinfo) == G_FILE_TYPE_SYMBOLIC_LINK) {
            gchar *ctype;
            gchar *mimetype;
            EntangleMedia *media = NULL;
            char *filename = g_file_get_path(child);

            if (!(ctype = g_content_type_guess(filename,
                                               NULL, 0, NULL))) {
                ENTANGLE_DEBUG("No content type for %s", filename);
                g_free(filename);
                continue;
            }

            if (!(mimetype = g_content_type_get_mime_type(ctype))) {
                ENTANGLE_DEBUG("No mime type for content type %s file %s",
                               ctype, filename);
                g_free(filename);
                g_free(ctype);
                continue;
            }
            g_free(ctype);

            if (g_str_has_prefix(mimetype, "image/")) {
                media = ENTANGLE_MEDIA(entangle_image_new_file(filename));
            } else if (g_str_has_prefix(mimetype, "video/")) {
                media = ENTANGLE_MEDIA(entangle_video_new_file(filename));
            } else {
                ENTANGLE_DEBUG("Mime type %s for file %s not wanted",
                               mimetype, filename);
            }
            g_free(mimetype);
            if (media) {
                ENTANGLE_DEBUG("Adding '%s'", filename);
                session->files = g_list_prepend(session->files, media);
            }
            g_free(filename);
        }
        g_object_unref(child);
        g_object_unref(childinfo);
    }

    g_object_unref(children);

    session->files = g_list_sort(session->files,
                               entangle_session_compare_media);

    tmp = session->files;
    while (tmp) {
        EntangleMedia *media = ENTANGLE_MEDIA(tmp->data);
        g_signal_emit_by_name(session, "session-media-added", media);
        tmp = tmp->next;
    }

    session->recalculateDigit = TRUE;

    g_object_unref(dir);

    return TRUE;
}


/**
 * entangle_session_get_media_count:
 * @session: (transfer none): the session instance
 *
 * Get the total number of media files in the session
 *
 * Returns: the file count
 */
int entangle_session_get_media_count(EntangleSession *session)
{
    g_return_val_if_fail(ENTANGLE_IS_SESSION(session), 0);

    return g_list_length(session->files);
}


/**
 * entangle_session_get_media:
 * @session: (transfer none): the session instance
 * @idx: index of the media file to fetch
 *
 * Get the media file at position @idx in the session
 *
 * Returns: (transfer none): the media file
 */
EntangleMedia *entangle_session_get_media(EntangleSession *session, int idx)
{
    g_return_val_if_fail(ENTANGLE_IS_SESSION(session), NULL);

    return g_list_nth_data(session->files, idx);
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 *  tab-width: 8
 * End:
 */
