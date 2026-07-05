/*
 * update_check.c - Compare local version with GitHub Releases (public API)
 * Copyright (C) 2026 Iván Ezequiel Rodriguez
 * License: GPLv3+
 */
#include "update_check.h"
#include "i18n.h"
#include "version.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PG_GITHUB_API_LATEST \
    "https://api.github.com/repos/IRodriguez13/powergov/releases/latest"
#define PG_RELEASES_PAGE \
    POWERGOV_SOURCE_URL "/releases/latest"

typedef struct
{
    GtkWindow *parent;
    char remote_ver[32];
    char page_url[256];
} UpdateNotice;

static int parse_version_triple(const char *s, int *maj, int *min, int *pat)
{
    const char *p = s;
    int a = 0;
    int b = 0;
    int c = 0;

    if (!s || !maj || !min || !pat)
        return 0;

    while (*p == 'v' || *p == 'V' || *p == ' ')
        p++;

    if (sscanf(p, "%d.%d.%d", &a, &b, &c) < 2)
        return 0;

    *maj = a;
    *min = b;
    *pat = c;
    return 1;
}

static int version_newer_than_local(const char *remote, const char *local)
{
    int rma, rmi, rpa;
    int lma, lmi, lpa;

    if (!parse_version_triple(remote, &rma, &rmi, &rpa))
        return 0;
    if (!parse_version_triple(local, &lma, &lmi, &lpa))
        return 1;

    if (rma != lma)
        return rma > lma;
    if (rmi != lmi)
        return rmi > lmi;
    return rpa > lpa;
}

static int extract_json_string(const char *json, const char *key,
                               char *out, size_t outsz)
{
    char pattern[64];
    const char *p;
    const char *start;
    const char *end;
    size_t len;

    if (!json || !key || !out || outsz == 0)
        return 0;

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr(json, pattern);
    if (!p)
        return 0;

    p = strchr(p, ':');
    if (!p)
        return 0;

    p++;
    while (*p == ' ' || *p == '\t')
        p++;

    if (*p != '"')
        return 0;

    start = p + 1;
    end = strchr(start, '"');
    if (!end)
        return 0;

    len = (size_t)(end - start);
    if (len >= outsz)
        len = outsz - 1;

    memcpy(out, start, len);
    out[len] = '\0';
    return 1;
}

static int fetch_latest_release(char *body, size_t body_sz)
{
    GError *err = NULL;
    gchar *stdout_buf = NULL;
    gchar *stderr_buf = NULL;
    gint status = 0;
    char cmd[512];
    int ok = 0;

    if (!body || body_sz == 0)
        return 0;

    body[0] = '\0';

    if (access("/usr/bin/curl", X_OK) == 0)
    {
        snprintf(cmd, sizeof(cmd),
                 "/usr/bin/curl -fsSL --max-time 15 "
                 "-H 'User-Agent: PowerGov/%s' '%s'",
                 POWERGOV_VERSION, PG_GITHUB_API_LATEST);
    }
    else if (access("/usr/bin/wget", X_OK) == 0)
    {
        snprintf(cmd, sizeof(cmd),
                 "/usr/bin/wget -qO- --timeout=15 '%s'",
                 PG_GITHUB_API_LATEST);
    }
    else
    {
        return 0;
    }

    if (!g_spawn_command_line_sync(cmd, &stdout_buf, &stderr_buf,
                                   &status, &err))
    {
        g_clear_error(&err);
        g_free(stdout_buf);
        g_free(stderr_buf);
        return 0;
    }

    if (g_spawn_check_wait_status(status, NULL) && stdout_buf && stdout_buf[0])
    {
        g_strlcpy(body, stdout_buf, body_sz);
        ok = 1;
    }

    g_free(stdout_buf);
    g_free(stderr_buf);
    return ok;
}

static char *dismiss_path(void)
{
    const char *cfg = g_get_user_config_dir();
    return g_build_filename(cfg, "powergov", "update-dismissed", NULL);
}

static int update_dismissed(const char *remote_ver)
{
    char *path;
    gchar *contents = NULL;
    gsize len = 0;
    int dismissed = 0;

    if (!remote_ver || !remote_ver[0])
        return 0;

    path = dismiss_path();
    if (g_file_get_contents(path, &contents, &len, NULL) && contents)
    {
        g_strstrip(contents);
        dismissed = (strcmp(contents, remote_ver) == 0);
    }

    g_free(contents);
    g_free(path);
    return dismissed;
}

static void save_dismissed(const char *remote_ver)
{
    char *path;
    char *dir;

    if (!remote_ver || !remote_ver[0])
        return;

    path = dismiss_path();
    dir = g_path_get_dirname(path);
    g_mkdir_with_parents(dir, 0755);
    g_file_set_contents(path, remote_ver, -1, NULL);
    g_free(dir);
    g_free(path);
}

static void open_release_page(UpdateNotice *notice)
{
    GError *err = NULL;

    if (!notice)
        return;

    if (!gtk_show_uri_on_window(notice->parent, notice->page_url,
                                GDK_CURRENT_TIME, &err))
    {
        char *argv[3];

        argv[0] = "xdg-open";
        argv[1] = notice->page_url;
        argv[2] = NULL;
        g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
                      NULL, NULL, NULL, NULL);
        g_clear_error(&err);
    }
}

static gboolean show_update_dialog_idle(gpointer data)
{
    UpdateNotice *notice = data;
    GtkWidget *dlg;
    char body[384];
    int response;

    if (!notice || !notice->parent)
        goto out;

    snprintf(body, sizeof(body), _(PG_TR_UPDATE_AVAILABLE_BODY),
             notice->remote_ver, POWERGOV_VERSION);

    dlg = gtk_message_dialog_new(
        notice->parent,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_INFO,
        GTK_BUTTONS_NONE,
        "%s",
        _(PG_TR_UPDATE_AVAILABLE_TITLE));
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg),
                                             "%s", body);
    gtk_dialog_add_button(GTK_DIALOG(dlg), _(PG_TR_UPDATE_BTN_LATER),
                          GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dlg), _(PG_TR_UPDATE_BTN_OPEN),
                          GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);

    response = gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);

    if (response == GTK_RESPONSE_OK)
        open_release_page(notice);
    else
        save_dismissed(notice->remote_ver);

out:
    if (notice)
    {
        if (notice->parent)
            g_object_unref(notice->parent);
        g_free(notice);
    }
    return G_SOURCE_REMOVE;
}

static gpointer update_worker(gpointer data)
{
    GtkWindow *parent = data;
    char json[8192];
    char tag[64];
    char html_url[256];
    UpdateNotice *notice;

    if (getenv("POWERGOV_SKIP_UPDATE_CHECK"))
        goto out_parent;

    if (!fetch_latest_release(json, sizeof(json)))
        goto out_parent;

    if (!extract_json_string(json, "tag_name", tag, sizeof(tag)))
        goto out_parent;

    if (!version_newer_than_local(tag, POWERGOV_VERSION))
        goto out_parent;

    if (update_dismissed(tag))
        goto out_parent;

    if (!extract_json_string(json, "html_url", html_url, sizeof(html_url)))
        g_strlcpy(html_url, PG_RELEASES_PAGE, sizeof(html_url));

    notice = g_malloc0(sizeof(*notice));
    notice->parent = parent;
    g_strlcpy(notice->remote_ver, tag, sizeof(notice->remote_ver));
    g_strlcpy(notice->page_url, html_url, sizeof(notice->page_url));
    g_idle_add(show_update_dialog_idle, notice);
    return NULL;

out_parent:
    if (parent)
        g_object_unref(parent);
    return NULL;
}

void pg_update_check_start(GtkWindow *parent)
{
    if (!parent)
        return;
    if (getenv("POWERGOV_SKIP_UPDATE_CHECK"))
        return;

    g_thread_new("pg-update-check", update_worker,
                 g_object_ref(parent));
}
