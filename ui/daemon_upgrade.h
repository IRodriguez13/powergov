/*
 * daemon_upgrade.h - Prompt when systemd daemon is older than the UI
 * Copyright (C) 2026 Iván Ezequiel Rodriguez
 * License: GPLv3+
 */
#pragma once

#include <gtk/gtk.h>

typedef void (*PgDaemonUpgradeInstallFn)(gpointer user_data);

void pg_daemon_upgrade_prompt(GtkWindow *parent,
                              const char *daemon_ver,
                              int api_too_old,
                              PgDaemonUpgradeInstallFn install_fn,
                              gpointer install_data);

int pg_daemon_upgrade_dismissed(const char *daemon_ver);
void pg_daemon_upgrade_save_dismissed(const char *daemon_ver);

int pg_daemon_version_outdated(const char *daemon_ver, int api_too_old);
