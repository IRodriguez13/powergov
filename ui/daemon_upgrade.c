/*
 * daemon_upgrade.c - Daemon vs UI version mismatch dialog
 * Copyright (C) 2026 Iván Ezequiel Rodriguez
 * License: GPLv3+
 */
#include "daemon_upgrade.h"
#include "i18n.h"
#include "version.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>

#define PG_DAEMON_DISMISS_DIR  ".config/powergov"
#define PG_DAEMON_DISMISS_FILE "daemon-upgrade-dismissed"

typedef struct
{
    GtkWindow *parent;
    char daemon_ver[64];
    int api_too_old;
    PgDaemonUpgradeInstallFn install_fn;
    gpointer install_data;
} DaemonUpgradeNotice;

static char *dismiss_path(void)
{
    const char *home = g_get_home_dir();

    if (!home)
        return NULL;
    return g_build_filename(home, PG_DAEMON_DISMISS_DIR,
                            PG_DAEMON_DISMISS_FILE, NULL);
}

int pg_daemon_upgrade_dismissed(const char *daemon_ver)
{
    char *path;
    char line[256];
    FILE *f;
    char key[128];
    int found = 0;

    if (!daemon_ver || !daemon_ver[0])
        return 0;

    snprintf(key, sizeof(key), "%s|%s", POWERGOV_VERSION, daemon_ver);
    path = dismiss_path();
    if (!path)
        return 0;

    f = g_fopen(path, "r");
    g_free(path);
    if (!f)
        return 0;

    while (fgets(line, sizeof(line), f))
    {
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = '\0';
        if (strcmp(line, key) == 0)
            found = 1;
    }
    fclose(f);
    return found;
}

void pg_daemon_upgrade_save_dismissed(const char *daemon_ver)
{
    char *path;
    char *dir;
    FILE *f;
    char key[128];

    if (!daemon_ver || !daemon_ver[0])
        return;

    snprintf(key, sizeof(key), "%s|%s", POWERGOV_VERSION, daemon_ver);
    path = dismiss_path();
    if (!path)
        return;

    dir = g_path_get_dirname(path);
    g_mkdir_with_parents(dir, 0700);
    g_free(dir);

    f = g_fopen(path, "a");
    if (!f)
    {
        g_free(path);
        return;
    }
    fprintf(f, "%s\n", key);
    fclose(f);
    g_free(path);
}

int pg_daemon_version_outdated(const char *daemon_ver, int api_too_old)
{
    if (api_too_old)
        return 1;
    if (!daemon_ver || !daemon_ver[0])
        return 0;
    return powergov_version_newer(POWERGOV_VERSION, daemon_ver);
}

static gboolean show_daemon_upgrade_idle(gpointer data)
{
    DaemonUpgradeNotice *notice = data;
    GtkWidget *dlg;
    char body[512];
    char daemon_label[80];
    int response;

    if (!notice || !notice->parent)
        goto out;

    if (notice->daemon_ver[0])
        snprintf(daemon_label, sizeof(daemon_label), "%s", notice->daemon_ver);
    else
        g_strlcpy(daemon_label, _(PG_TR_DAEMON_VER_UNKNOWN), sizeof(daemon_label));

    snprintf(body, sizeof(body), _(PG_TR_DAEMON_OLD_BODY),
             POWERGOV_VERSION, daemon_label);

    dlg = gtk_message_dialog_new(
        notice->parent,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_WARNING,
        GTK_BUTTONS_NONE,
        "%s",
        _(PG_TR_DAEMON_OLD_TITLE));
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg),
                                             "%s", body);
    gtk_dialog_add_button(GTK_DIALOG(dlg), _(PG_TR_DAEMON_OLD_BTN_LATER),
                          GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dlg), _(PG_TR_DAEMON_OLD_BTN_UPGRADE),
                          GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);

    response = gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);

    if (response == GTK_RESPONSE_OK && notice->install_fn)
        notice->install_fn(notice->install_data);
    else if (response == GTK_RESPONSE_CANCEL)
        pg_daemon_upgrade_save_dismissed(notice->daemon_ver);

out:
    if (notice)
    {
        if (notice->parent)
            g_object_unref(notice->parent);
        g_free(notice);
    }
    return G_SOURCE_REMOVE;
}

void pg_daemon_upgrade_prompt(GtkWindow *parent,
                              const char *daemon_ver,
                              int api_too_old,
                              PgDaemonUpgradeInstallFn install_fn,
                              gpointer install_data)
{
    DaemonUpgradeNotice *notice;
    char verbuf[64];

    if (!parent)
        return;

    verbuf[0] = '\0';
    if (daemon_ver && daemon_ver[0])
        g_strlcpy(verbuf, daemon_ver, sizeof(verbuf));
    else if (!powergov_probe_installed_daemon_version(verbuf, sizeof(verbuf)))
        verbuf[0] = '\0';

    if (!pg_daemon_version_outdated(verbuf, api_too_old))
        return;

    if (pg_daemon_upgrade_dismissed(verbuf))
        return;

    notice = g_malloc0(sizeof(*notice));
    notice->parent = GTK_WINDOW(g_object_ref(parent));
    g_strlcpy(notice->daemon_ver, verbuf, sizeof(notice->daemon_ver));
    notice->api_too_old = api_too_old;
    notice->install_fn = install_fn;
    notice->install_data = install_data;
    g_idle_add(show_daemon_upgrade_idle, notice);
}
