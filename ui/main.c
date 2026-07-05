/*
 * main.c - GTK desktop UI for powergov (libpowergov + Unix socket)
 * Copyright (C) 2026 Iván Ezequiel Rodriguez
 * License: GPLv3+
 */
#include <powergov/client.h>
#include "i18n.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>

#define REFRESH_MS           4000
#define FEEDBACK_MS          5000
#define ACTION_LOG_MAX_LINES 40
#define DEV_LOG_UI_MAX_LINES 500
#define POLKIT_DEV_MODE      "org.powergov.dev-mode"
#define POLKIT_MANAGE_SVC    "org.powergov.manage-service"
#define POLKIT_INSTALL_SVC   "org.powergov.install-service"
#define POWERGOV_PKEXEC      "/usr/bin/pkexec"
#define POWERGOV_DEV_AUTH    "/usr/local/libexec/powergov/dev-auth"
#define POWERGOV_INSTALL_HELPER \
    "/usr/local/libexec/powergov/install-service-resident.sh"
#define POWERGOV_INSTALL_STAGING_DEFAULT \
    "/usr/local/libexec/powergov/staging"
#define POWERGOV_UNINSTALL_HELPER \
    "/usr/local/libexec/powergov/powergov-uninstall.sh"
#define POWERGOV_USER_CLEANUP_HELPER \
    "/usr/local/libexec/powergov/remove-appimage-user-files.sh"
#define POWERGOV_STAGING_REL     ".staging/install"
#define POWERGOV_STAGING_NAME      "powergov"
#ifndef POWERGOV_ICON_ROOT
#define POWERGOV_ICON_ROOT   "/usr/share/icons/hicolor"
#endif

typedef struct _AppCtx AppCtx;

typedef enum
{
    UI_ACT_SET_MODE = 1,
    UI_ACT_SET_BATTERY
} UiActionKind;

typedef struct
{
    AppCtx *ctx;
    int daemon_up;
    int systemd_active;
    int service_installed;
    int fetch_dev;
    int st_ok;
    int sys_ok;
    int cpu_ok;
    int compat_ok;
    int metrics_ok;
    int log_ok;
    powergov_reply_status_t st;
    powergov_reply_system_t sys;
    powergov_reply_cpu_t cpu;
    powergov_reply_compat_t compat;
    powergov_reply_metrics_t metrics;
    powergov_reply_log_t log;
} UiSnapshot;

typedef struct
{
    UiActionKind kind;
    int mode;
    int battery_thr;
    int battery_on;
    int send_ok;
    int verify_ok;
    powergov_reply_status_t st;
} UiActionResult;

struct _AppCtx
{
    GtkWidget *window;
    GtkWidget *status_label;
    GtkWidget *power_label;
    GtkWidget *feedback_label;
    GtkWidget *action_log_view;
    GtkWidget *mode_box;
    GtkWidget *profile_hint_label;
    GtkWidget *advanced_modes_label;
    GtkWidget *battery_switch;
    GtkWidget *battery_scale;
    GtkWidget *main_notebook;
    GtkWidget *dev_notebook;
    GtkWidget *dev_lock_banner;
    GtkWidget *dev_btn;
    GtkWidget *lang_btn;
    GtkWidget *battery_label;
    GtkWidget *activity_label;
    GtkWidget *service_box;
    GtkWidget *start_btn;
    GtkWidget *stop_btn;
    GtkWidget *restart_btn;
    GtkWidget *uninstall_btn;
    GtkWidget *sys_view;
    GtkWidget *cpu_view;
    GtkWidget *compat_view;
    GtkWidget *metrics_view;
    GtkWidget *log_view;
    GtkWidget *mode_buttons[3];
    gulong mode_handlers[3];
    guint timer_id;
    guint feedback_timeout_id;
    gulong battery_sw_handler;
    int daemon_up;
    int service_installed;
    int dev_unlocked;
    int install_busy;
    int uninstall_busy;
    int startup_prompt_done;
    int dev_log_initialized;
    int metrics_initialized;
    int refresh_busy;
    int ui_sync;
    char dev_log_last_snapshot[POWERGOV_SOCK_LOG_SZ + 1];
    char metrics_last_snapshot[POWERGOV_SOCK_METRICS_SZ + 1];
    char install_tmpdir[512];
};

static int pidfile_daemon_alive(void)
{
    FILE *f;
    int pid;

    f = fopen(POWERGOV_PIDFILE_PATH, "r");
    if (!f)
        return 0;
    if (fscanf(f, "%d", &pid) != 1)
    {
        fclose(f);
        return 0;
    }
    fclose(f);
    return (pid > 0 && kill((pid_t)pid, 0) == 0);
}

static int powergov_service_installed(void)
{
    if (access("/etc/systemd/system/powergov.service", F_OK) == 0)
        return 1;
    if (access("/usr/local/bin/powergov", F_OK) == 0)
        return 1;
    return 0;
}

static int read_self_dir(char *buf, size_t len)
{
    ssize_t n;

    if (!buf || len == 0)
        return 0;

    n = readlink("/proc/self/exe", buf, len - 1);
    if (n <= 0)
        return 0;

    buf[n] = '\0';
    {
        char *slash = strrchr(buf, '/');
        if (slash)
            *slash = '\0';
        else
            return 0;
    }
    return 1;
}

static int staging_has_daemon(const char *dir)
{
    char path[512];

    if (!dir || !dir[0])
        return 0;

    snprintf(path, sizeof(path), "%s/%s", dir, POWERGOV_STAGING_NAME);
    return access(path, X_OK) == 0;
}

/* pkexec runs as root with another cwd — staging/helper must be absolute. */
static int canonical_existing_path(const char *in, char *out, size_t outsz)
{
    char joined[PATH_MAX];
    char *resolved;
    const char *probe;
    int ok;

    if (!in || !in[0] || !out || outsz == 0)
        return 0;

    if (in[0] == '/')
    {
        probe = in;
    }
    else
    {
        char cwd[PATH_MAX];

        if (!getcwd(cwd, sizeof(cwd)))
            return 0;
        if (snprintf(joined, sizeof(joined), "%s/%s", cwd, in) >=
            (int)sizeof(joined))
            return 0;
        probe = joined;
    }

    resolved = realpath(probe, NULL);
    if (!resolved)
        return 0;

    ok = 0;
    if (strlen(resolved) + 1 <= outsz)
    {
        snprintf(out, outsz, "%s", resolved);
        ok = 1;
    }

    free(resolved);
    return ok;
}

static int try_staging_candidate(const char *candidate, char *out, size_t outsz)
{
    char abs[512];

    if (!candidate || !candidate[0])
        return 0;

    if (!canonical_existing_path(candidate, abs, sizeof(abs)))
        return 0;

    if (!staging_has_daemon(abs))
        return 0;

    snprintf(out, outsz, "%s", abs);
    return 1;
}

static int try_helper_candidate(const char *candidate, char *out, size_t outsz)
{
    char abs[512];

    if (!candidate || !candidate[0])
        return 0;

    if (!canonical_existing_path(candidate, abs, sizeof(abs)))
        return 0;

    if (access(abs, X_OK) != 0)
        return 0;

    snprintf(out, outsz, "%s", abs);
    return 1;
}

static int resolve_install_staging(char *out, size_t outsz)
{
    const char *env;
    char self_dir[512];
    char path[512];

    if (!out || outsz == 0)
        return 0;

    env = getenv("POWERGOV_INSTALL_STAGING");
    if (env && try_staging_candidate(env, out, outsz))
        return 1;

    if (try_staging_candidate(POWERGOV_INSTALL_STAGING_DEFAULT, out, outsz))
        return 1;

    if (read_self_dir(self_dir, sizeof(self_dir)))
    {
        snprintf(path, sizeof(path), "%s/../lib/powergov/staging", self_dir);
        if (try_staging_candidate(path, out, outsz))
            return 1;
    }

    if (try_staging_candidate(POWERGOV_STAGING_REL, out, outsz))
        return 1;

    return 0;
}

static int resolve_install_helper(char *out, size_t outsz)
{
    const char *env;
    char self_dir[512];
    char path[512];

    if (!out || outsz == 0)
        return 0;

    env = getenv("POWERGOV_INSTALL_HELPER");
    if (env && try_helper_candidate(env, out, outsz))
        return 1;

    if (try_helper_candidate(POWERGOV_INSTALL_HELPER, out, outsz))
        return 1;

    if (read_self_dir(self_dir, sizeof(self_dir)))
    {
        snprintf(path, sizeof(path),
                 "%s/../libexec/powergov/install-service-resident.sh",
                 self_dir);
        if (try_helper_candidate(path, out, outsz))
            return 1;
    }

    if (try_helper_candidate("scripts/install-service-resident.sh", out, outsz))
        return 1;

    return 0;
}

static int resolve_uninstall_helper(char *out, size_t outsz)
{
    const char *env;
    char self_dir[512];
    char path[512];

    if (!out || outsz == 0)
        return 0;

    env = getenv("POWERGOV_UNINSTALL_HELPER");
    if (env && try_helper_candidate(env, out, outsz))
        return 1;

    if (try_helper_candidate(POWERGOV_UNINSTALL_HELPER, out, outsz))
        return 1;

    if (read_self_dir(self_dir, sizeof(self_dir)))
    {
        snprintf(path, sizeof(path),
                 "%s/../libexec/powergov/powergov-uninstall.sh", self_dir);
        if (try_helper_candidate(path, out, outsz))
            return 1;
    }

    if (try_helper_candidate("scripts/powergov-uninstall.sh", out, outsz))
        return 1;

    return 0;
}

static int resolve_user_cleanup_helper(char *out, size_t outsz)
{
    const char *env;
    char self_dir[512];
    char path[512];

    if (!out || outsz == 0)
        return 0;

    env = getenv("POWERGOV_USER_CLEANUP_HELPER");
    if (env && try_helper_candidate(env, out, outsz))
        return 1;

    if (try_helper_candidate(POWERGOV_USER_CLEANUP_HELPER, out, outsz))
        return 1;

    if (read_self_dir(self_dir, sizeof(self_dir)))
    {
        snprintf(path, sizeof(path),
                 "%s/../libexec/powergov/remove-appimage-user-files.sh",
                 self_dir);
        if (try_helper_candidate(path, out, outsz))
            return 1;
    }

    if (try_helper_candidate("scripts/remove-appimage-user-files.sh", out, outsz))
        return 1;

    return 0;
}

static void start_install_async(AppCtx *ctx);
static int prompt_install_service(AppCtx *ctx);
static int ensure_daemon_for_action(AppCtx *ctx);
static void refresh_async(AppCtx *ctx);
static void update_service_buttons(AppCtx *ctx, int systemd_active);

static int path_on_appimage_mount(const char *path)
{
    return path && strncmp(path, "/tmp/.mount_", 12) == 0;
}

static void cleanup_install_tmpdir(AppCtx *ctx)
{
    char *argv[4];
    GError *err = NULL;

    if (!ctx || !ctx->install_tmpdir[0])
        return;

    argv[0] = "rm";
    argv[1] = "-rf";
    argv[2] = ctx->install_tmpdir;
    argv[3] = NULL;

    if (!g_spawn_sync(NULL, argv, NULL,
                      G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL |
                      G_SPAWN_STDERR_TO_DEV_NULL,
                      NULL, NULL, NULL, NULL, NULL, &err))
        g_clear_error(&err);

    ctx->install_tmpdir[0] = '\0';
}

static int spawn_argv_ok(char **argv)
{
    GError *err = NULL;
    int ok;

    ok = g_spawn_sync(NULL, argv, NULL,
                      G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL |
                      G_SPAWN_STDERR_TO_DEV_NULL,
                      NULL, NULL, NULL, NULL, NULL, &err);
    if (!ok)
        g_clear_error(&err);
    return ok;
}

/* pkexec cannot execute from AppImage FUSE mounts — copy to /tmp first. */
static int materialize_install_bundle(AppCtx *ctx, const char *helper_src,
                                      const char *staging_src,
                                      char *helper_dst, size_t helper_dst_sz,
                                      char *staging_dst, size_t staging_dst_sz)
{
    char tmpl[] = "/tmp/powergov-install-XXXXXX";
    char *tmpdir;
    char staging_path[512];
    char helper_path[512];
    char cp_src[512];
    char *mkdir_argv[4];
    char *cp_tree_argv[5];
    char *cp_helper_argv[5];

    if (!path_on_appimage_mount(helper_src) &&
        !path_on_appimage_mount(staging_src))
    {
        snprintf(helper_dst, helper_dst_sz, "%s", helper_src);
        snprintf(staging_dst, staging_dst_sz, "%s", staging_src);
        return 1;
    }

    tmpdir = mkdtemp(tmpl);
    if (!tmpdir)
        return 0;

    snprintf(staging_path, sizeof(staging_path), "%s/staging", tmpdir);
    snprintf(helper_path, sizeof(helper_path),
             "%s/install-service-resident.sh", tmpdir);
    snprintf(cp_src, sizeof(cp_src), "%s/.", staging_src);

    mkdir_argv[0] = "mkdir";
    mkdir_argv[1] = "-p";
    mkdir_argv[2] = staging_path;
    mkdir_argv[3] = NULL;

    cp_tree_argv[0] = "cp";
    cp_tree_argv[1] = "-a";
    cp_tree_argv[2] = cp_src;
    cp_tree_argv[3] = staging_path;
    cp_tree_argv[4] = NULL;

    cp_helper_argv[0] = "cp";
    cp_helper_argv[1] = (char *)helper_src;
    cp_helper_argv[2] = helper_path;
    cp_helper_argv[3] = NULL;

    if (!spawn_argv_ok(mkdir_argv) ||
        !spawn_argv_ok(cp_tree_argv) ||
        !spawn_argv_ok(cp_helper_argv) ||
        chmod(helper_path, 0755) != 0)
    {
        snprintf(ctx->install_tmpdir, sizeof(ctx->install_tmpdir), "%s", tmpdir);
        cleanup_install_tmpdir(ctx);
        return 0;
    }

    snprintf(helper_dst, helper_dst_sz, "%s", helper_path);
    snprintf(staging_dst, staging_dst_sz, "%s", staging_path);
    snprintf(ctx->install_tmpdir, sizeof(ctx->install_tmpdir), "%s", tmpdir);
    return 1;
}

static int materialize_uninstall_helper(AppCtx *ctx, const char *helper_src,
                                        char *helper_dst, size_t helper_dst_sz)
{
    char tmpl[] = "/tmp/powergov-uninstall-XXXXXX";
    char *tmpdir;
    char helper_path[512];
    char *cp_argv[4];

    if (!path_on_appimage_mount(helper_src))
    {
        snprintf(helper_dst, helper_dst_sz, "%s", helper_src);
        return 1;
    }

    tmpdir = mkdtemp(tmpl);
    if (!tmpdir)
        return 0;

    snprintf(helper_path, sizeof(helper_path),
             "%s/powergov-uninstall.sh", tmpdir);

    cp_argv[0] = "cp";
    cp_argv[1] = (char *)helper_src;
    cp_argv[2] = helper_path;
    cp_argv[3] = NULL;

    if (!spawn_argv_ok(cp_argv) || chmod(helper_path, 0755) != 0)
    {
        snprintf(ctx->install_tmpdir, sizeof(ctx->install_tmpdir), "%s", tmpdir);
        cleanup_install_tmpdir(ctx);
        return 0;
    }

    snprintf(helper_dst, helper_dst_sz, "%s", helper_path);
    snprintf(ctx->install_tmpdir, sizeof(ctx->install_tmpdir), "%s", tmpdir);
    return 1;
}

static void run_user_cleanup_helper(void)
{
    char helper[512];
    char *argv[2];
    GError *err = NULL;

    if (!resolve_user_cleanup_helper(helper, sizeof(helper)))
        return;

    argv[0] = helper;
    argv[1] = NULL;
    if (!g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
                       NULL, NULL, NULL, &err))
        g_clear_error(&err);
}

static gboolean refresh_async_idle(gpointer data)
{
    refresh_async((AppCtx *)data);
    return G_SOURCE_REMOVE;
}

static int dev_mode_active(const AppCtx *ctx)
{
    return ctx->dev_unlocked;
}

static void apply_ui_theme(void)
{
    GtkCssProvider *provider = gtk_css_provider_new();

    gtk_css_provider_load_from_data(provider,
        ".pg-feedback-ok { color: #A5D6A7; font-weight: 600; }\n"
        ".pg-feedback-err { color: #FFCC80; font-weight: 600; }\n"
        ".pg-dev-lock { color: #B0BEC5; font-style: italic; padding: 12px; }\n"
        ".pg-title { font-size: 18pt; font-weight: 700; }\n"
        ".pg-profile-hint { color: #B0BEC5; font-size: 10pt; }\n"
        ".pg-advanced-label { color: #90A4AE; font-size: 9pt; "
        "margin-top: 6px; margin-bottom: 2px; }\n"
        ".pg-mode-smart { background-color: rgba(129, 199, 132, 0.08); "
        "border-radius: 6px; }\n",
        -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

static GdkPixbuf *load_brand_icon(int size)
{
    static const char *roots[] = {
        "data/icons/hicolor",
        POWERGOV_ICON_ROOT,
        NULL
    };
    char path[512];
    GError *err = NULL;
    int i;

    for (i = 0; roots[i]; i++)
    {
        snprintf(path, sizeof(path), "%s/%dx%d/apps/powergov.png",
                 roots[i], size, size);
        if (access(path, R_OK) == 0)
        {
            GdkPixbuf *raw = gdk_pixbuf_new_from_file(path, &err);
            GdkPixbuf *scaled;

            g_clear_error(&err);
            if (!raw)
                continue;
            if (gdk_pixbuf_get_width(raw) == size &&
                gdk_pixbuf_get_height(raw) == size)
                return raw;

            scaled = gdk_pixbuf_scale_simple(raw, size, size, GDK_INTERP_BILINEAR);
            g_object_unref(raw);
            return scaled;
        }
    }

    return NULL;
}

static void apply_window_branding(GtkWindow *window)
{
    GList *icons = NULL;
    GdkPixbuf *icon;

    icon = load_brand_icon(256);
    if (icon)
        icons = g_list_append(icons, icon);
    icon = load_brand_icon(48);
    if (icon)
        icons = g_list_append(icons, icon);

    if (icons)
    {
        gtk_window_set_icon_list(window, icons);
        g_list_free(icons);
    }
}

static void update_dev_tab_access(AppCtx *ctx)
{
    gtk_widget_set_sensitive(ctx->dev_notebook, ctx->dev_unlocked);
    gtk_widget_set_visible(ctx->dev_lock_banner, !ctx->dev_unlocked);
    if (ctx->dev_unlocked)
        gtk_button_set_label(GTK_BUTTON(ctx->dev_btn), _(PG_TR_BTN_USER_MODE));
    else
        gtk_button_set_label(GTK_BUTTON(ctx->dev_btn), _(PG_TR_BTN_DEV_MODE));
}

static void fill_text_view(GtkWidget *view, const char *text)
{
    GtkTextBuffer *buf;

    if (!view)
        return;
    buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
    gtk_text_buffer_set_text(buf, text ? text : "", -1);
}

static GtkAdjustment *text_view_vadj(GtkWidget *view)
{
    GtkWidget *parent;

    if (!view)
        return NULL;
    parent = gtk_widget_get_parent(view);
    if (!parent || !GTK_IS_SCROLLED_WINDOW(parent))
        return NULL;
    return gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(parent));
}

static int text_view_at_bottom(GtkWidget *view)
{
    GtkAdjustment *vadj;
    gdouble upper;
    gdouble page;
    gdouble value;

    vadj = text_view_vadj(view);
    if (!vadj)
        return 1;

    upper = gtk_adjustment_get_upper(vadj);
    page = gtk_adjustment_get_page_size(vadj);
    value = gtk_adjustment_get_value(vadj);

    if (upper <= page + 1.0)
        return 1;

    return (value >= upper - page - 8.0);
}

static void text_view_scroll_end(GtkWidget *view)
{
    GtkTextBuffer *buf;
    GtkTextIter end;
    GtkTextMark *mark;

    if (!view)
        return;

    buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
    gtk_text_buffer_get_end_iter(buf, &end);
    mark = gtk_text_buffer_create_mark(buf, NULL, &end, FALSE);
    gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(view), mark);
    gtk_text_buffer_delete_mark(buf, mark);
}

static int text_view_first_visible_line(GtkWidget *view)
{
    GdkRectangle rect;
    GtkTextIter iter;

    if (!view)
        return 0;

    gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(view), &rect);
    gtk_text_view_get_line_at_y(GTK_TEXT_VIEW(view), &iter, rect.y, NULL);
    return gtk_text_iter_get_line(&iter);
}

static void text_view_scroll_to_line(GtkWidget *view, int line)
{
    GtkTextBuffer *buf;
    GtkTextIter iter;
    int max_line;

    if (!view)
        return;

    buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
    max_line = gtk_text_buffer_get_line_count(buf) - 1;
    if (line < 0)
        line = 0;
    if (line > max_line)
        line = max_line;

    gtk_text_buffer_get_iter_at_line(buf, &iter, line);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(view), &iter,
                                 0.0, FALSE, 0.0, 0.0);
}

static void update_panel_text_view(GtkWidget *view, const char *text,
                                   char *last_snap, size_t snap_sz,
                                   int *initialized)
{
    int at_bottom;
    int was_init;
    int visible_line;

    if (!view || !text)
        return;

    if (*initialized && strcmp(text, last_snap) == 0)
        return;

    was_init = *initialized;
    at_bottom = text_view_at_bottom(view);
    visible_line = was_init && !at_bottom ? text_view_first_visible_line(view) : 0;

    fill_text_view(view, text);
    g_strlcpy(last_snap, text, snap_sz);
    *initialized = 1;

    if (at_bottom)
        text_view_scroll_end(view);
    else if (was_init)
        text_view_scroll_to_line(view, visible_line);
}

static GtkAdjustment *dev_log_view_vadj(GtkWidget *view)
{
    return text_view_vadj(view);
}

static int dev_log_view_at_bottom(GtkWidget *view)
{
    return text_view_at_bottom(view);
}

static void dev_log_view_scroll_end(GtkWidget *view)
{
    text_view_scroll_end(view);
}

static size_t snapshot_overlap(const char *prev, const char *next)
{
    size_t plen;
    size_t nlen;
    size_t max_ov;
    size_t i;

    if (!prev || !next || !prev[0] || !next[0])
        return 0;

    plen = strlen(prev);
    nlen = strlen(next);
    max_ov = plen < nlen ? plen : nlen;

    for (i = max_ov; i > 0; i--)
    {
        if (memcmp(prev + plen - i, next, i) == 0)
            return i;
    }
    return 0;
}

static void dev_log_view_append(GtkWidget *view, const char *text, int trim_old)
{
    GtkTextBuffer *buf;
    GtkTextIter end;
    int lines;
    GtkAdjustment *vadj;
    gdouble scroll_before;
    int had_scroll;

    if (!view || !text || !text[0])
        return;

    vadj = dev_log_view_vadj(view);
    had_scroll = (vadj && gtk_adjustment_get_upper(vadj) >
                  gtk_adjustment_get_page_size(vadj) + 1.0);
    scroll_before = vadj ? gtk_adjustment_get_value(vadj) : 0.0;

    buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
    gtk_text_buffer_get_end_iter(buf, &end);
    gtk_text_buffer_insert(buf, &end, text, -1);

    if (trim_old)
    {
        lines = gtk_text_buffer_get_line_count(buf);
        if (lines > DEV_LOG_UI_MAX_LINES)
        {
            GtkTextIter start;
            GtkTextIter cut;
            int drop = lines - DEV_LOG_UI_MAX_LINES;

            gtk_text_buffer_get_iter_at_line(buf, &start, 0);
            gtk_text_buffer_get_iter_at_line(buf, &cut, drop);
            gtk_text_buffer_delete(buf, &start, &cut);
        }
    }
    else if (had_scroll && vadj)
    {
        gtk_adjustment_set_value(vadj, scroll_before);
    }
}

static void update_dev_log_view(AppCtx *ctx, const char *new_text)
{
    const char *text;
    size_t overlap;
    int at_bottom;
    gdouble saved_scroll = 0.0;
    GtkAdjustment *vadj;

    if (!ctx || !ctx->log_view)
        return;

    text = new_text ? new_text : "";
    vadj = dev_log_view_vadj(ctx->log_view);
    at_bottom = dev_log_view_at_bottom(ctx->log_view);
    if (vadj && !at_bottom)
        saved_scroll = gtk_adjustment_get_value(vadj);

    if (!ctx->dev_log_initialized)
    {
        if (text[0] != '\0')
        {
            fill_text_view(ctx->log_view, text);
            g_strlcpy(ctx->dev_log_last_snapshot, text,
                      sizeof(ctx->dev_log_last_snapshot));
            ctx->dev_log_initialized = 1;
        }
        if (at_bottom)
            dev_log_view_scroll_end(ctx->log_view);
        return;
    }

    if (text[0] == '\0')
        return;

    if (strcmp(text, ctx->dev_log_last_snapshot) == 0)
        return;

    overlap = snapshot_overlap(ctx->dev_log_last_snapshot, text);
    if (overlap > 0)
    {
        if (text[overlap] != '\0')
            dev_log_view_append(ctx->log_view, text + overlap, at_bottom);
    }
    else
    {
        dev_log_view_append(ctx->log_view, "\n---\n", at_bottom);
        dev_log_view_append(ctx->log_view, text, at_bottom);
    }

    g_strlcpy(ctx->dev_log_last_snapshot, text,
              sizeof(ctx->dev_log_last_snapshot));

    if (at_bottom)
        dev_log_view_scroll_end(ctx->log_view);
    else if (vadj)
        gtk_adjustment_set_value(vadj, saved_scroll);
}

static void append_action_log(AppCtx *ctx, const char *line)
{
    GtkTextBuffer *buf;
    GtkTextIter end;
    char ts[32];
    time_t now;
    struct tm tm;

    if (!ctx->action_log_view)
        return;

    now = time(NULL);
    localtime_r(&now, &tm);
    strftime(ts, sizeof(ts), "%H:%M:%S", &tm);

    buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctx->action_log_view));
    gtk_text_buffer_get_end_iter(buf, &end);
    gtk_text_buffer_insert(buf, &end, "[", -1);
    gtk_text_buffer_insert(buf, &end, ts, -1);
    gtk_text_buffer_insert(buf, &end, "] ", -1);
    gtk_text_buffer_insert(buf, &end, line, -1);
    gtk_text_buffer_insert(buf, &end, "\n", -1);

    {
        int lines = gtk_text_buffer_get_line_count(buf);
        if (lines > ACTION_LOG_MAX_LINES)
        {
            GtkTextIter start;
            gtk_text_buffer_get_iter_at_line(buf, &start, lines - ACTION_LOG_MAX_LINES);
            gtk_text_buffer_delete(buf, &start, &end);
        }
    }
}

static gboolean feedback_clear_cb(gpointer data)
{
    AppCtx *ctx = data;
    GtkStyleContext *sc;

    ctx->feedback_timeout_id = 0;
    sc = gtk_widget_get_style_context(ctx->feedback_label);
    gtk_style_context_remove_class(sc, "pg-feedback-ok");
    gtk_style_context_remove_class(sc, "pg-feedback-err");
    gtk_label_set_text(GTK_LABEL(ctx->feedback_label), "");
    return G_SOURCE_REMOVE;
}

static void show_feedback(AppCtx *ctx, int ok, const char *text)
{
    GtkStyleContext *sc;

    if (!ctx->feedback_label)
        return;

    if (ctx->feedback_timeout_id)
        g_source_remove(ctx->feedback_timeout_id);

    sc = gtk_widget_get_style_context(ctx->feedback_label);
    gtk_style_context_remove_class(sc, "pg-feedback-ok");
    gtk_style_context_remove_class(sc, "pg-feedback-err");
    gtk_style_context_add_class(sc, ok ? "pg-feedback-ok" : "pg-feedback-err");
    gtk_label_set_text(GTK_LABEL(ctx->feedback_label), text ? text : "");
    ctx->feedback_timeout_id = g_timeout_add(FEEDBACK_MS, feedback_clear_cb, ctx);
}

static void clear_dev_views(AppCtx *ctx)
{
    fill_text_view(ctx->sys_view, "");
    fill_text_view(ctx->cpu_view, "");
    fill_text_view(ctx->compat_view, "");
    fill_text_view(ctx->metrics_view, "");
    fill_text_view(ctx->log_view, "");
    ctx->dev_log_initialized = 0;
    ctx->dev_log_last_snapshot[0] = '\0';
    ctx->metrics_initialized = 0;
    ctx->metrics_last_snapshot[0] = '\0';
}

static void install_child_exit(GPid pid, gint status, gpointer data)
{
    AppCtx *ctx = data;
    GError *err = NULL;

    g_spawn_close_pid(pid);
    ctx->install_busy = 0;
    cleanup_install_tmpdir(ctx);
    update_service_buttons(ctx, ctx->daemon_up);

    if (g_spawn_check_wait_status(status, &err))
    {
        ctx->service_installed = 1;
        append_action_log(ctx, _(PG_TR_LOG_INSTALL_OK));
        show_feedback(ctx, 1, _(PG_TR_FB_INSTALL_OK));
        g_timeout_add(1500, refresh_async_idle, ctx);
        g_clear_error(&err);
        return;
    }

    show_feedback(ctx, 0, _(PG_TR_ERR_INSTALL_DENIED));
    append_action_log(ctx, _(PG_TR_ERR_INSTALL_DENIED));
    g_clear_error(&err);
}

static void uninstall_child_exit(GPid pid, gint status, gpointer data)
{
    AppCtx *ctx = data;
    GError *err = NULL;

    g_spawn_close_pid(pid);
    ctx->uninstall_busy = 0;
    cleanup_install_tmpdir(ctx);
    update_service_buttons(ctx, ctx->daemon_up);

    if (g_spawn_check_wait_status(status, &err))
    {
        ctx->service_installed = 0;
        ctx->daemon_up = 0;
        run_user_cleanup_helper();
        append_action_log(ctx, _(PG_TR_LOG_UNINSTALL_OK));
        show_feedback(ctx, 1, _(PG_TR_FB_UNINSTALL_OK));
        g_clear_error(&err);
        return;
    }

    show_feedback(ctx, 0, _(PG_TR_ERR_UNINSTALL_DENIED));
    append_action_log(ctx, _(PG_TR_ERR_UNINSTALL_DENIED));
    g_clear_error(&err);
}

static void start_uninstall_async(AppCtx *ctx)
{
    char helper[512];
    char helper_run[512];
    char *argv[3];
    GError *err = NULL;
    GPid pid;

    if (ctx->uninstall_busy)
        return;

    cleanup_install_tmpdir(ctx);

    if (!resolve_uninstall_helper(helper, sizeof(helper)))
    {
        show_feedback(ctx, 0, _(PG_TR_ERR_UNINSTALL_UNAVAILABLE));
        append_action_log(ctx, _(PG_TR_ERR_UNINSTALL_UNAVAILABLE));
        return;
    }

    if (!materialize_uninstall_helper(ctx, helper, helper_run,
                                      sizeof(helper_run)))
    {
        show_feedback(ctx, 0, _(PG_TR_ERR_UNINSTALL_UNAVAILABLE));
        append_action_log(ctx, _(PG_TR_ERR_UNINSTALL_UNAVAILABLE));
        return;
    }

    if (access(POWERGOV_PKEXEC, X_OK) != 0)
    {
        show_feedback(ctx, 0, _(PG_TR_ERR_DEV_NO_PKEXEC));
        cleanup_install_tmpdir(ctx);
        return;
    }

    argv[0] = (char *)POWERGOV_PKEXEC;
    argv[1] = helper_run;
    argv[2] = NULL;

    if (!g_spawn_async(NULL, argv, NULL,
                       G_SPAWN_DO_NOT_REAP_CHILD,
                       NULL, NULL, &pid, &err))
    {
        show_feedback(ctx, 0, _(PG_TR_ERR_UNINSTALL_FAILED));
        cleanup_install_tmpdir(ctx);
        g_clear_error(&err);
        return;
    }

    ctx->uninstall_busy = 1;
    update_service_buttons(ctx, ctx->daemon_up);
    show_feedback(ctx, 1, _(PG_TR_FB_UNINSTALL_RUNNING));
    append_action_log(ctx, _(PG_TR_LOG_UNINSTALL_STARTED));
    g_child_watch_add(pid, uninstall_child_exit, ctx);
}

static int prompt_uninstall(AppCtx *ctx)
{
    GtkWidget *dlg;
    int response;

    dlg = gtk_message_dialog_new(
        GTK_WINDOW(ctx->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_WARNING,
        GTK_BUTTONS_NONE,
        "%s",
        _(PG_TR_UNINSTALL_DIALOG_TITLE));
    gtk_message_dialog_format_secondary_text(
        GTK_MESSAGE_DIALOG(dlg), "%s", _(PG_TR_UNINSTALL_DIALOG_BODY));
    gtk_dialog_add_button(GTK_DIALOG(dlg), _(PG_TR_UNINSTALL_DIALOG_NO),
                          GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dlg), _(PG_TR_UNINSTALL_DIALOG_YES),
                          GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_CANCEL);

    response = gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);

    if (response == GTK_RESPONSE_OK)
    {
        start_uninstall_async(ctx);
        return 1;
    }

    return 0;
}

static void on_uninstall_clicked(GtkButton *btn, gpointer data)
{
    AppCtx *ctx = data;

    (void)btn;
    prompt_uninstall(ctx);
}

static void start_install_async(AppCtx *ctx)
{
    char helper[512];
    char staging[512];
    char helper_run[512];
    char staging_run[512];
    char *argv[4];
    GError *err = NULL;
    GPid pid;

    (void)POLKIT_INSTALL_SVC;

    if (ctx->install_busy)
        return;

    cleanup_install_tmpdir(ctx);

    if (!resolve_install_helper(helper, sizeof(helper)) ||
        !resolve_install_staging(staging, sizeof(staging)))
    {
        show_feedback(ctx, 0, _(PG_TR_ERR_INSTALL_UNAVAILABLE));
        append_action_log(ctx, _(PG_TR_ERR_INSTALL_UNAVAILABLE));
        return;
    }

    if (path_on_appimage_mount(helper) || path_on_appimage_mount(staging))
        show_feedback(ctx, 1, _(PG_TR_FB_INSTALL_PREPARING));

    if (!materialize_install_bundle(ctx, helper, staging,
                                    helper_run, sizeof(helper_run),
                                    staging_run, sizeof(staging_run)))
    {
        show_feedback(ctx, 0, _(PG_TR_ERR_INSTALL_UNAVAILABLE));
        append_action_log(ctx, _(PG_TR_ERR_INSTALL_UNAVAILABLE));
        return;
    }

    if (access(POWERGOV_PKEXEC, X_OK) != 0)
    {
        show_feedback(ctx, 0, _(PG_TR_ERR_DEV_NO_PKEXEC));
        cleanup_install_tmpdir(ctx);
        return;
    }

    argv[0] = (char *)POWERGOV_PKEXEC;
    argv[1] = helper_run;
    argv[2] = staging_run;
    argv[3] = NULL;

    if (!g_spawn_async(NULL, argv, NULL,
                       G_SPAWN_DO_NOT_REAP_CHILD,
                       NULL, NULL, &pid, &err))
    {
        show_feedback(ctx, 0, _(PG_TR_ERR_INSTALL_FAILED));
        cleanup_install_tmpdir(ctx);
        g_clear_error(&err);
        return;
    }

    ctx->install_busy = 1;
    update_service_buttons(ctx, ctx->daemon_up);
    show_feedback(ctx, 1, _(PG_TR_FB_INSTALL_RUNNING));
    append_action_log(ctx, _(PG_TR_LOG_INSTALL_STARTED));
    g_child_watch_add(pid, install_child_exit, ctx);
}

static int prompt_install_service(AppCtx *ctx)
{
    GtkWidget *dlg;
    int response;

    dlg = gtk_message_dialog_new(
        GTK_WINDOW(ctx->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_NONE,
        "%s",
        _(PG_TR_INSTALL_DIALOG_TITLE));
    gtk_message_dialog_format_secondary_text(
        GTK_MESSAGE_DIALOG(dlg), "%s", _(PG_TR_INSTALL_DIALOG_BODY));
    gtk_dialog_add_button(GTK_DIALOG(dlg), _(PG_TR_INSTALL_DIALOG_NO),
                          GTK_RESPONSE_NO);
    gtk_dialog_add_button(GTK_DIALOG(dlg), _(PG_TR_INSTALL_DIALOG_YES),
                          GTK_RESPONSE_YES);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_YES);

    response = gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);

    if (response == GTK_RESPONSE_YES)
    {
        start_install_async(ctx);
        return 1;
    }

    return 0;
}

static int ensure_daemon_for_action(AppCtx *ctx)
{
    if (ctx->daemon_up)
        return 1;

    if (ctx->service_installed)
    {
        show_feedback(ctx, 0, _(PG_TR_ERR_SERVICE_NOT_RUNNING));
        return 0;
    }

    return prompt_install_service(ctx);
}

static gboolean startup_install_prompt_idle(gpointer data)
{
    AppCtx *ctx = data;

    if (!ctx || ctx->daemon_up || ctx->service_installed)
        return G_SOURCE_REMOVE;

    prompt_install_service(ctx);
    return G_SOURCE_REMOVE;
}

static void run_polkit_service(AppCtx *ctx, const char *action)
{
    char cmd[256];
    GError *err = NULL;

    (void)POLKIT_MANAGE_SVC;
    snprintf(cmd, sizeof(cmd), "%s systemctl %s powergov.service",
             POWERGOV_PKEXEC, action);
    if (!g_spawn_command_line_async(cmd, &err))
    {
        show_feedback(ctx, 0, _(PG_TR_ERR_MANAGE_SERVICE));
        g_clear_error(&err);
    }
    else
    {
        char msg[96];
        snprintf(msg, sizeof(msg), _(PG_TR_LOG_SERVICE_REQUESTED),
                 pg_service_action_label(action));
        append_action_log(ctx, msg);
    }
}

static void set_mode_sensitive(AppCtx *ctx, int sensitive)
{
    int i;
    for (i = 0; i < 3; i++)
        gtk_widget_set_sensitive(ctx->mode_buttons[i], sensitive);
    gtk_widget_set_sensitive(ctx->battery_switch, sensitive);
    gtk_widget_set_sensitive(ctx->battery_scale, sensitive);
}

static void update_service_buttons(AppCtx *ctx, int systemd_active)
{
    int dev = dev_mode_active(ctx);
    const char *primary = ctx->service_installed
                              ? _(PG_TR_BTN_START_SERVICE)
                              : _(PG_TR_BTN_INSTALL_SERVICE);

    gtk_widget_set_visible(ctx->service_box, dev || !ctx->daemon_up);
    gtk_button_set_label(GTK_BUTTON(ctx->start_btn), primary);
    gtk_widget_set_visible(ctx->stop_btn, dev);
    gtk_widget_set_visible(ctx->restart_btn, dev);
    if (ctx->service_installed)
        gtk_widget_set_sensitive(ctx->start_btn, !systemd_active);
    else
        gtk_widget_set_sensitive(ctx->start_btn, !ctx->install_busy);
    gtk_widget_set_sensitive(ctx->stop_btn, systemd_active);
    gtk_widget_set_sensitive(ctx->restart_btn, systemd_active);
    if (ctx->uninstall_btn)
    {
        gtk_widget_set_visible(ctx->uninstall_btn, ctx->service_installed);
        gtk_widget_set_sensitive(ctx->uninstall_btn,
                                 ctx->service_installed &&
                                 !ctx->install_busy && !ctx->uninstall_busy);
    }
}

static void apply_dev_panels(AppCtx *ctx, const UiSnapshot *s)
{
    char line[512];
    char block[4096];
    int i;

    if (!dev_mode_active(ctx) || !s->daemon_up)
        return;

    if (s->sys_ok)
    {
        snprintf(block, sizeof(block), _(PG_TR_SYS_FMT),
                 s->sys.pretty_name, s->sys.kernel, s->sys.powergov_version,
                 s->sys.systemd_active ? _(PG_TR_ACTIVE) : _(PG_TR_INACTIVE),
                 s->sys.ppd_detected ? _(PG_TR_YES) : _(PG_TR_NO));
        fill_text_view(ctx->sys_view, block);
    }

    if (s->cpu_ok)
    {
        snprintf(block, sizeof(block), _(PG_TR_CPU_FMT),
                 s->cpu.model, s->cpu.cpu_count, s->cpu.scaling_driver,
                 s->cpu.governor, s->cpu.governors_avail,
                 s->cpu.epp, s->cpu.epp_available ? "ok" : "n/a",
                 s->cpu.turbo_on == 1 ? "on" : (s->cpu.turbo_on == 0 ? "off" : "?"),
                 s->cpu.freq_hw_max, s->cpu.freq_scaling_max,
                 s->cpu.platform_profile[0] ? s->cpu.platform_profile : "—",
                 s->cpu.rapl_available ? _(PG_TR_YES) : _(PG_TR_NO));
        fill_text_view(ctx->cpu_view, block);
    }

    if (s->compat_ok)
    {
        char summary[128];
        int supported = 0;
        int partial = 0;

        block[0] = '\0';
        for (i = 0; i < POWERGOV_FEATURE_COUNT; i++)
        {
            if (s->compat.rows[i].state == POWERGOV_COMPAT_SUPPORTED)
                supported++;
            else if (s->compat.rows[i].state == POWERGOV_COMPAT_PARTIAL)
                partial++;
        }

        pg_compat_format_summary(supported, partial, POWERGOV_FEATURE_COUNT,
                                 summary, sizeof(summary));
        snprintf(line, sizeof(line), _(PG_TR_COMPAT_SCORE_FMT),
                 s->compat.adaptability_score, summary);
        strncat(block, line, sizeof(block) - strlen(block) - 1);
        for (i = 0; i < POWERGOV_FEATURE_COUNT; i++)
        {
            snprintf(line, sizeof(line), _(PG_TR_COMPAT_ROW_FMT),
                     s->compat.rows[i].name,
                     pg_compat_state_label(s->compat.rows[i].state),
                     s->compat.rows[i].hw_available,
                     s->compat.rows[i].enabled,
                     pg_compat_detail_tr(s->compat.rows[i].detail));
            strncat(block, line, sizeof(block) - strlen(block) - 1);
        }
        fill_text_view(ctx->compat_view, block);
    }

    if (s->metrics_ok)
    {
        update_panel_text_view(ctx->metrics_view,
                               pg_core_dev_text_tr(s->metrics.text),
                               ctx->metrics_last_snapshot,
                               sizeof(ctx->metrics_last_snapshot),
                               &ctx->metrics_initialized);
    }

    if (s->log_ok)
    {
        if (s->log.ok)
            update_dev_log_view(ctx, s->log.text);
        else if (!ctx->dev_log_initialized)
        {
            const char *msg = s->log.text[0]
                                    ? pg_core_dev_text_tr(s->log.text)
                                    : _(PG_TR_NO_LOG);

            update_dev_log_view(ctx, msg);
        }
    }
}

static void apply_snapshot(AppCtx *ctx, UiSnapshot *s)
{
    char line[512];
    int i;

    ctx->daemon_up = s->daemon_up;
    ctx->service_installed = s->service_installed;
    ctx->refresh_busy = 0;

    if (!s->daemon_up)
    {
        const char *status = _(PG_TR_STATUS_NOT_RUNNING);

        if (!s->service_installed)
            status = _(PG_TR_STATUS_NOT_INSTALLED);
        else if (s->systemd_active)
            status = _(PG_TR_STATUS_NO_RESPOND);

        gtk_label_set_text(GTK_LABEL(ctx->status_label), status);
        gtk_label_set_text(GTK_LABEL(ctx->power_label), "");
        set_mode_sensitive(ctx, FALSE);
        update_service_buttons(ctx, s->systemd_active);
        if (!dev_mode_active(ctx))
            clear_dev_views(ctx);

        if (!s->service_installed && !ctx->startup_prompt_done)
        {
            ctx->startup_prompt_done = 1;
            g_idle_add(startup_install_prompt_idle, ctx);
        }

        g_free(s);
        return;
    }

    if (!s->st_ok)
    {
        gtk_label_set_text(GTK_LABEL(ctx->status_label),
                           _(PG_TR_STATUS_READ_ERROR));
        g_free(s);
        return;
    }

    set_mode_sensitive(ctx, TRUE);
    update_service_buttons(ctx, 1);

    snprintf(line, sizeof(line), _(PG_TR_STATUS_ACTIVE_PROFILE),
             pg_user_mode_title((powergov_user_mode_t)s->st.user_mode));
    gtk_label_set_text(GTK_LABEL(ctx->status_label), line);

    if (s->st.battery_pct >= 0)
    {
        const char *src = (s->st.power_source == POWERGOV_POWER_AC)
                              ? _(PG_TR_POWER_PLUGGED)
                              : _(PG_TR_POWER_BATTERY);
        snprintf(line, sizeof(line), "%s — %d%%", src, s->st.battery_pct);
    }
    else
        line[0] = '\0';
    gtk_label_set_text(GTK_LABEL(ctx->power_label), line);

    ctx->ui_sync = 1;
    for (i = 0; i < 3; i++)
        g_signal_handler_block(ctx->mode_buttons[i], ctx->mode_handlers[i]);

    for (i = 0; i < 3; i++)
    {
        gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(ctx->mode_buttons[i]), s->st.user_mode == i);
    }

    for (i = 0; i < 3; i++)
        g_signal_handler_unblock(ctx->mode_buttons[i], ctx->mode_handlers[i]);

    g_signal_handler_block(ctx->battery_switch, ctx->battery_sw_handler);
    gtk_switch_set_active(GTK_SWITCH(ctx->battery_switch),
                          s->st.battery_safe_enabled);
    g_signal_handler_unblock(ctx->battery_switch, ctx->battery_sw_handler);

    if (s->st.battery_threshold > 0)
    {
        double cur = gtk_range_get_value(GTK_RANGE(ctx->battery_scale));
        if ((int)cur != s->st.battery_threshold)
            gtk_range_set_value(GTK_RANGE(ctx->battery_scale),
                               s->st.battery_threshold);
    }
    ctx->ui_sync = 0;

    if (dev_mode_active(ctx))
        apply_dev_panels(ctx, s);

    g_free(s);
}

static void snapshot_worker(GTask *task, gpointer source, gpointer data,
                            GCancellable *cancel)
{
    UiSnapshot *s = g_task_get_task_data(task);

    (void)source;
    (void)data;
    (void)cancel;

    s->service_installed = powergov_service_installed();

    if (s->fetch_dev)
    {
        powergov_reply_bundle_t bundle;

        if (powergov_client_query_bundle(POWERGOV_BUNDLE_DEV_ALL, 80, &bundle) == 0)
        {
            s->st = bundle.status;
            s->sys = bundle.system;
            s->cpu = bundle.cpu;
            s->compat = bundle.compat;
            s->metrics = bundle.metrics;
            s->log = bundle.log;
            s->st_ok = 1;
            s->sys_ok = 1;
            s->cpu_ok = 1;
            s->compat_ok = 1;
            s->metrics_ok = 1;
            s->log_ok = 1;
            s->daemon_up = 1;
            s->systemd_active = 1;
        }
        else
        {
            s->daemon_up = 0;
            s->systemd_active = pidfile_daemon_alive();
        }

        g_task_return_pointer(task, s, NULL);
        return;
    }

    s->st_ok = (powergov_client_query_status(&s->st) == 0);
    s->daemon_up = s->st_ok;
    if (!s->daemon_up)
        s->systemd_active = pidfile_daemon_alive();
    else
        s->systemd_active = 1;

    g_task_return_pointer(task, s, NULL);
}

static void snapshot_done(GObject *src, GAsyncResult *res, gpointer data)
{
    AppCtx *ctx = data;
    UiSnapshot *s;
    GError *err = NULL;

    (void)src;
    s = g_task_propagate_pointer(G_TASK(res), &err);
    if (!s)
    {
        ctx->refresh_busy = 0;
        g_clear_error(&err);
        return;
    }
    apply_snapshot(ctx, s);
}

static void refresh_async(AppCtx *ctx)
{
    UiSnapshot *s;
    GTask *task;

    if (ctx->refresh_busy)
        return;

    ctx->refresh_busy = 1;
    s = g_new0(UiSnapshot, 1);
    s->ctx = ctx;
    s->fetch_dev = dev_mode_active(ctx);

    task = g_task_new(NULL, NULL, snapshot_done, ctx);
    g_task_set_task_data(task, s, NULL);
    g_task_run_in_thread(task, snapshot_worker);
    g_object_unref(task);
}

static void user_action_worker(GTask *task, gpointer source, gpointer data,
                               GCancellable *cancel)
{
    UiActionResult *r = g_task_get_task_data(task);

    (void)source;
    (void)data;
    (void)cancel;

    if (r->kind == UI_ACT_SET_MODE)
    {
        r->send_ok = (powergov_client_set_user_mode(
                          (powergov_user_mode_t)r->mode) == 0);
        if (r->send_ok)
        {
            r->verify_ok = (powergov_client_query_status(&r->st) == 0 &&
                            r->st.user_mode == r->mode);
        }
    }
    else if (r->kind == UI_ACT_SET_BATTERY)
    {
        int thr = r->battery_on ? r->battery_thr : 0;

        r->send_ok = (powergov_client_set_battery_threshold(thr) == 0);
        if (r->send_ok)
        {
            r->verify_ok = (powergov_client_query_status(&r->st) == 0 &&
                            !!r->st.battery_safe_enabled == !!r->battery_on &&
                            (!r->battery_on ||
                             r->st.battery_threshold == r->battery_thr));
        }
    }

    g_task_return_pointer(task, r, NULL);
}

static void user_action_done(GObject *src, GAsyncResult *res, gpointer data)
{
    AppCtx *ctx = data;
    UiActionResult *r;
    GError *err = NULL;
    char logline[256];
    char feedback[160];

    (void)src;
    r = g_task_propagate_pointer(G_TASK(res), &err);
    if (!r)
    {
        g_clear_error(&err);
        return;
    }

    if (r->kind == UI_ACT_SET_MODE)
    {
        const char *title = pg_user_mode_title((powergov_user_mode_t)r->mode);

        if (!r->send_ok)
        {
            snprintf(logline, sizeof(logline), _(PG_TR_LOG_PROFILE_FAIL), title);
            show_feedback(ctx, 0, _(PG_TR_ERR_PROFILE_CHANGE));
        }
        else if (!r->verify_ok)
        {
            snprintf(logline, sizeof(logline),
                     _(PG_TR_LOG_PROFILE_UNCONFIRMED), title);
            show_feedback(ctx, 0, _(PG_TR_ERR_PROFILE_VERIFY));
        }
        else
        {
            snprintf(logline, sizeof(logline), _(PG_TR_LOG_PROFILE_OK), title);
            snprintf(feedback, sizeof(feedback), _(PG_TR_FB_PROFILE_ACTIVE), title);
            show_feedback(ctx, 1, feedback);
        }
    }
    else if (r->kind == UI_ACT_SET_BATTERY)
    {
        if (!r->send_ok)
        {
            snprintf(logline, sizeof(logline), "%s", _(PG_TR_LOG_BATTERY_FAIL));
            show_feedback(ctx, 0, _(PG_TR_ERR_BATTERY_UPDATE));
        }
        else if (!r->verify_ok)
        {
            snprintf(logline, sizeof(logline), "%s",
                     _(PG_TR_LOG_BATTERY_UNCONFIRMED));
            show_feedback(ctx, 0, _(PG_TR_ERR_BATTERY_VERIFY));
        }
        else if (r->battery_on)
        {
            snprintf(logline, sizeof(logline), _(PG_TR_LOG_BATTERY_ON),
                     r->battery_thr);
            snprintf(feedback, sizeof(feedback), _(PG_TR_FB_BATTERY_ON),
                     r->battery_thr);
            show_feedback(ctx, 1, feedback);
        }
        else
        {
            snprintf(logline, sizeof(logline), "%s", _(PG_TR_LOG_BATTERY_OFF));
            show_feedback(ctx, 1, _(PG_TR_FB_BATTERY_OFF));
        }
    }

    append_action_log(ctx, logline);
    refresh_async(ctx);
    g_free(r);
}

static void run_user_action_async(AppCtx *ctx, UiActionResult *r)
{
    GTask *task;

    task = g_task_new(NULL, NULL, user_action_done, ctx);
    g_task_set_task_data(task, r, NULL);
    g_task_run_in_thread(task, user_action_worker);
    g_object_unref(task);
}

static void enter_dev_mode(AppCtx *ctx)
{
    ctx->dev_unlocked = 1;
    update_dev_tab_access(ctx);
    update_service_buttons(ctx, ctx->daemon_up);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(ctx->main_notebook), 1);
    append_action_log(ctx, _(PG_TR_LOG_DEV_ON));
    show_feedback(ctx, 1, _(PG_TR_FB_DEV_ON));
    refresh_async(ctx);
}

static void exit_dev_mode(AppCtx *ctx)
{
    ctx->dev_unlocked = 0;
    update_dev_tab_access(ctx);
    update_service_buttons(ctx, ctx->daemon_up);
    clear_dev_views(ctx);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(ctx->main_notebook), 0);
    append_action_log(ctx, _(PG_TR_LOG_DEV_OFF));
    show_feedback(ctx, 1, _(PG_TR_FB_DEV_OFF));
    refresh_async(ctx);
}

static void dev_auth_child_exit(GPid pid, gint status, gpointer data)
{
    AppCtx *ctx = data;
    GError *err = NULL;

    g_spawn_close_pid(pid);

    if (g_spawn_check_wait_status(status, &err))
    {
        enter_dev_mode(ctx);
        g_clear_error(&err);
        return;
    }

    show_feedback(ctx, 0, _(PG_TR_ERR_DEV_DENIED));
    append_action_log(ctx, _(PG_TR_LOG_DEV_DENIED));
    g_clear_error(&err);
}

static void start_dev_auth_async(AppCtx *ctx)
{
    char *argv[] = {
        (char *)POWERGOV_PKEXEC,
        (char *)POWERGOV_DEV_AUTH,
        NULL
    };
    GError *err = NULL;
    GPid pid;

    if (access(POWERGOV_DEV_AUTH, X_OK) != 0)
    {
        show_feedback(ctx, 0, _(PG_TR_ERR_DEV_INCOMPLETE));
        append_action_log(ctx, _(PG_TR_LOG_DEV_INCOMPLETE));
        return;
    }

    if (access(POWERGOV_PKEXEC, X_OK) != 0)
    {
        show_feedback(ctx, 0, _(PG_TR_ERR_DEV_NO_PKEXEC));
        append_action_log(ctx, _(PG_TR_LOG_DEV_UNAVAILABLE));
        return;
    }

    if (!g_spawn_async(NULL, argv, NULL,
                       G_SPAWN_DO_NOT_REAP_CHILD,
                       NULL, NULL, &pid, &err))
    {
        show_feedback(ctx, 0, _(PG_TR_ERR_DEV_DIALOG));
        append_action_log(ctx, _(PG_TR_LOG_DEV_PERM_ERROR));
        g_clear_error(&err);
        return;
    }

    g_child_watch_add(pid, dev_auth_child_exit, ctx);
}

static gboolean on_timer(gpointer data)
{
    refresh_async((AppCtx *)data);
    return G_SOURCE_CONTINUE;
}

static void on_mode_toggled(GtkToggleButton *btn, gpointer data)
{
    AppCtx *ctx = data;
    UiActionResult *r;

    if (ctx->ui_sync || !gtk_toggle_button_get_active(btn))
        return;

    if (!ensure_daemon_for_action(ctx))
    {
        refresh_async(ctx);
        return;
    }

    r = g_new0(UiActionResult, 1);
    r->kind = UI_ACT_SET_MODE;
    r->mode = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "mode"));
    run_user_action_async(ctx, r);
}

static void apply_battery_setting(AppCtx *ctx)
{
    UiActionResult *r;
    int thr;
    int on;

    if (ctx->ui_sync)
        return;

    if (!ensure_daemon_for_action(ctx))
    {
        refresh_async(ctx);
        return;
    }

    thr = (int)gtk_range_get_value(GTK_RANGE(ctx->battery_scale));
    on = gtk_switch_get_active(GTK_SWITCH(ctx->battery_switch));

    r = g_new0(UiActionResult, 1);
    r->kind = UI_ACT_SET_BATTERY;
    r->battery_thr = thr;
    r->battery_on = on;
    run_user_action_async(ctx, r);
}

static gboolean on_battery_switch(GtkSwitch *sw, gboolean state, gpointer data)
{
    AppCtx *ctx = data;

    (void)sw;
    (void)state;
    if (ctx->ui_sync)
        return FALSE;

    apply_battery_setting(ctx);
    return FALSE;
}

static gboolean on_battery_scale_release(GtkWidget *widget, GdkEventButton *ev,
                                       gpointer data)
{
    AppCtx *ctx = data;

    (void)widget;
    (void)ev;
    if (ctx->ui_sync || !gtk_switch_get_active(GTK_SWITCH(ctx->battery_switch)))
        return FALSE;

    apply_battery_setting(ctx);
    return FALSE;
}

static void on_dev_btn_clicked(GtkButton *btn, gpointer data)
{
    AppCtx *ctx = data;

    (void)btn;

    if (ctx->dev_unlocked)
    {
        exit_dev_mode(ctx);
        return;
    }

    if (!ensure_daemon_for_action(ctx))
        return;

    start_dev_auth_async(ctx);
}

static void on_service_start(GtkButton *b, gpointer data)
{
    AppCtx *ctx = data;

    (void)b;

    if (!ctx->service_installed)
    {
        prompt_install_service(ctx);
        return;
    }

    run_polkit_service(ctx, "start");
}

static void on_service_stop(GtkButton *b, gpointer data)
{
    (void)b;
    run_polkit_service(data, "stop");
}

static void on_service_restart(GtkButton *b, gpointer data)
{
    (void)b;
    run_polkit_service(data, "restart");
}

static GtkWidget *make_scrolled_text(GtkWidget **out_view)
{
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD_CHAR);
    gtk_container_add(GTK_CONTAINER(scroll), view);
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_widget_set_vexpand(scroll, TRUE);
    if (out_view)
        *out_view = view;
    return scroll;
}

static GtkWidget *make_action_log_scrolled(void)
{
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *view = gtk_text_view_new();

    gtk_widget_set_size_request(scroll, -1, 88);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_NONE);
    gtk_container_add(GTK_CONTAINER(scroll), view);
    return scroll;
}

static GtkWidget *mode_button(AppCtx *ctx, powergov_user_mode_t mode,
                              GtkWidget *group)
{
    GtkWidget *btn;
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *title = gtk_label_new(NULL);
    GtkWidget *sub = gtk_label_new(NULL);

    gtk_label_set_markup(GTK_LABEL(title),
                         g_markup_printf_escaped("<b>%s</b>",
                                                   pg_user_mode_title(mode)));
    gtk_label_set_text(GTK_LABEL(sub), pg_user_mode_subtitle(mode));
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_widget_set_halign(sub, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), sub, FALSE, FALSE, 0);

    if (group)
        btn = gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(group));
    else
        btn = gtk_radio_button_new(NULL);

    gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
    gtk_widget_set_halign(btn, GTK_ALIGN_START);
    if (mode == POWERGOV_USER_MAX_BATTERY)
    {
        gtk_style_context_add_class(
            gtk_widget_get_style_context(btn), "pg-mode-smart");
    }
    gtk_container_add(GTK_CONTAINER(btn), box);
    g_object_set_data(G_OBJECT(btn), "mode", GINT_TO_POINTER((int)mode));
    g_object_set_data(G_OBJECT(btn), "mode_title", title);
    g_object_set_data(G_OBJECT(btn), "mode_sub", sub);
    ctx->mode_handlers[(int)mode] = g_signal_connect(
        btn, "toggled", G_CALLBACK(on_mode_toggled), ctx);
    ctx->mode_buttons[(int)mode] = btn;
    return btn;
}

static void notebook_tab_set_text(GtkNotebook *nb, int page, const char *text)
{
    GtkWidget *child;
    GtkWidget *tab;

    if (!nb || !text)
        return;

    child = gtk_notebook_get_nth_page(nb, page);
    if (!child)
        return;

    tab = gtk_notebook_get_tab_label(nb, child);
    if (GTK_IS_LABEL(tab))
        gtk_label_set_text(GTK_LABEL(tab), text);
}

static void refresh_mode_button_labels(AppCtx *ctx)
{
    int i;

    for (i = 0; i < 3; i++)
    {
        GtkWidget *btn = ctx->mode_buttons[i];
        GtkWidget *title;
        GtkWidget *sub;
        powergov_user_mode_t mode;

        if (!btn)
            continue;

        mode = (powergov_user_mode_t)GPOINTER_TO_INT(
            g_object_get_data(G_OBJECT(btn), "mode"));
        title = (GtkWidget *)g_object_get_data(G_OBJECT(btn), "mode_title");
        sub = (GtkWidget *)g_object_get_data(G_OBJECT(btn), "mode_sub");
        if (!title || !sub)
            continue;

        gtk_label_set_markup(GTK_LABEL(title),
                             g_markup_printf_escaped("<b>%s</b>",
                                                       pg_user_mode_title(mode)));
        gtk_label_set_text(GTK_LABEL(sub), pg_user_mode_subtitle(mode));
    }
}

static void invalidate_dev_panel_cache(AppCtx *ctx)
{
    ctx->dev_log_initialized = 0;
    ctx->dev_log_last_snapshot[0] = '\0';
    ctx->metrics_initialized = 0;
    ctx->metrics_last_snapshot[0] = '\0';
}

static void refresh_ui_language(AppCtx *ctx)
{
    PgTr dev_tab_ids[] = {
        PG_TR_TAB_SYSTEM, PG_TR_TAB_CPU, PG_TR_TAB_COMPAT,
        PG_TR_TAB_METRICS, PG_TR_TAB_LOG
    };
    int i;

    if (!ctx)
        return;

    if (ctx->lang_btn)
    {
        gtk_button_set_label(GTK_BUTTON(ctx->lang_btn),
                             pg_lang_button_label());
        gtk_widget_set_tooltip_text(ctx->lang_btn, pg_lang_button_tooltip());
    }

    gtk_button_set_label(GTK_BUTTON(ctx->start_btn),
                         ctx->service_installed
                             ? _(PG_TR_BTN_START_SERVICE)
                             : _(PG_TR_BTN_INSTALL_SERVICE));
    gtk_button_set_label(GTK_BUTTON(ctx->stop_btn), _(PG_TR_BTN_STOP));
    gtk_button_set_label(GTK_BUTTON(ctx->restart_btn), _(PG_TR_BTN_RESTART));
    if (ctx->uninstall_btn)
        gtk_button_set_label(GTK_BUTTON(ctx->uninstall_btn),
                             _(PG_TR_BTN_UNINSTALL));
    update_dev_tab_access(ctx);

    if (ctx->battery_label)
        gtk_label_set_text(GTK_LABEL(ctx->battery_label),
                           _(PG_TR_LABEL_BATTERY_PROTECT));
    if (ctx->profile_hint_label)
    {
        gtk_label_set_text(GTK_LABEL(ctx->profile_hint_label),
                           _(PG_TR_LABEL_PROFILE_HINT));
    }
    if (ctx->advanced_modes_label)
    {
        gtk_label_set_text(GTK_LABEL(ctx->advanced_modes_label),
                           _(PG_TR_LABEL_ADVANCED_MODES));
    }
    if (ctx->activity_label)
        gtk_label_set_text(GTK_LABEL(ctx->activity_label),
                           _(PG_TR_LABEL_RECENT_ACTIVITY));

    gtk_label_set_text(GTK_LABEL(ctx->dev_lock_banner), _(PG_TR_DEV_LOCK_BANNER));

    notebook_tab_set_text(GTK_NOTEBOOK(ctx->main_notebook), 0,
                          _(PG_TR_TAB_PROFILE));
    notebook_tab_set_text(GTK_NOTEBOOK(ctx->main_notebook), 1,
                          _(PG_TR_TAB_DIAGNOSTIC));

    for (i = 0; i < 5; i++)
    {
        notebook_tab_set_text(GTK_NOTEBOOK(ctx->dev_notebook), i,
                            _(dev_tab_ids[i]));
    }

    refresh_mode_button_labels(ctx);
    invalidate_dev_panel_cache(ctx);
    refresh_async(ctx);
}

static void on_lang_btn_clicked(GtkButton *btn, gpointer data)
{
    AppCtx *ctx = data;

    (void)btn;
    pg_i18n_toggle();
    refresh_ui_language(ctx);
}

static void build_ui(AppCtx *ctx)
{
    GtkWidget *vbox;
    GtkWidget *header;
    GtkWidget *profile_page;
    GtkWidget *dev_tab_page;
    GtkWidget *bat_box;
    GtkWidget *log_box;
    int i;
    PgTr dev_tab_ids[] = {
        PG_TR_TAB_SYSTEM, PG_TR_TAB_CPU, PG_TR_TAB_COMPAT,
        PG_TR_TAB_METRICS, PG_TR_TAB_LOG
    };
    GtkWidget *dev_pages[5];

    ctx->sys_view = NULL;
    ctx->cpu_view = NULL;
    ctx->compat_view = NULL;
    ctx->metrics_view = NULL;
    ctx->log_view = NULL;
    dev_pages[0] = make_scrolled_text(&ctx->sys_view);
    dev_pages[1] = make_scrolled_text(&ctx->cpu_view);
    dev_pages[2] = make_scrolled_text(&ctx->compat_view);
    dev_pages[3] = make_scrolled_text(&ctx->metrics_view);
    dev_pages[4] = make_scrolled_text(&ctx->log_view);

    ctx->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(ctx->window), "PowerGov");
    gtk_window_set_default_size(GTK_WINDOW(ctx->window), 720, 560);
    apply_window_branding(GTK_WINDOW(ctx->window));
    g_signal_connect(ctx->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);
    gtk_container_add(GTK_CONTAINER(ctx->window), vbox);

    {
        GtkWidget *title_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        GtkWidget *logo_img = NULL;
        GdkPixbuf *logo_pb = load_brand_icon(48);

        if (logo_pb)
        {
            logo_img = gtk_image_new_from_pixbuf(logo_pb);
            g_object_unref(logo_pb);
            gtk_box_pack_start(GTK_BOX(title_row), logo_img, FALSE, FALSE, 0);
        }

        {
            GtkWidget *title = gtk_label_new("PowerGov");
            gtk_widget_set_halign(title, GTK_ALIGN_START);
            gtk_style_context_add_class(
                gtk_widget_get_style_context(title), "pg-title");
            gtk_box_pack_start(GTK_BOX(title_row), title, FALSE, FALSE, 0);
        }

        ctx->lang_btn = gtk_button_new_with_label(pg_lang_button_label());
        gtk_widget_set_tooltip_text(ctx->lang_btn, pg_lang_button_tooltip());
        gtk_widget_set_halign(ctx->lang_btn, GTK_ALIGN_END);
        gtk_widget_set_valign(ctx->lang_btn, GTK_ALIGN_CENTER);
        g_signal_connect(ctx->lang_btn, "clicked",
                         G_CALLBACK(on_lang_btn_clicked), ctx);
        gtk_box_pack_end(GTK_BOX(title_row), ctx->lang_btn, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(vbox), title_row, FALSE, FALSE, 0);
    }

    header = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    ctx->status_label = gtk_label_new(_(PG_TR_CONNECTING));
    ctx->power_label = gtk_label_new("");
    ctx->feedback_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(ctx->status_label), 0.0);
    gtk_label_set_xalign(GTK_LABEL(ctx->power_label), 0.0);
    gtk_label_set_xalign(GTK_LABEL(ctx->feedback_label), 0.0);
    gtk_box_pack_start(GTK_BOX(header), ctx->status_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header), ctx->power_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header), ctx->feedback_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), header, FALSE, FALSE, 0);

    ctx->service_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    ctx->start_btn = gtk_button_new_with_label(_(PG_TR_BTN_START_SERVICE));
    ctx->stop_btn = gtk_button_new_with_label(_(PG_TR_BTN_STOP));
    ctx->restart_btn = gtk_button_new_with_label(_(PG_TR_BTN_RESTART));
    g_signal_connect(ctx->start_btn, "clicked", G_CALLBACK(on_service_start), ctx);
    g_signal_connect(ctx->stop_btn, "clicked", G_CALLBACK(on_service_stop), ctx);
    g_signal_connect(ctx->restart_btn, "clicked", G_CALLBACK(on_service_restart), ctx);
    gtk_box_pack_start(GTK_BOX(ctx->service_box), ctx->start_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ctx->service_box), ctx->stop_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ctx->service_box), ctx->restart_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), ctx->service_box, FALSE, FALSE, 0);

    ctx->main_notebook = gtk_notebook_new();

    ctx->mode_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

    ctx->profile_hint_label = gtk_label_new(_(PG_TR_LABEL_PROFILE_HINT));
    gtk_label_set_line_wrap(GTK_LABEL(ctx->profile_hint_label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(ctx->profile_hint_label), 0.0);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(ctx->profile_hint_label), "pg-profile-hint");
    gtk_box_pack_start(GTK_BOX(ctx->mode_box), ctx->profile_hint_label,
                       FALSE, FALSE, 0);

    {
        GtkWidget *smart_btn = mode_button(ctx, POWERGOV_USER_MAX_BATTERY, NULL);
        gtk_box_pack_start(GTK_BOX(ctx->mode_box), smart_btn, FALSE, FALSE, 0);
    }

    ctx->advanced_modes_label = gtk_label_new(_(PG_TR_LABEL_ADVANCED_MODES));
    gtk_label_set_xalign(GTK_LABEL(ctx->advanced_modes_label), 0.0);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(ctx->advanced_modes_label),
        "pg-advanced-label");
    gtk_box_pack_start(GTK_BOX(ctx->mode_box), ctx->advanced_modes_label,
                       FALSE, FALSE, 0);

    for (i = 1; i < 3; i++)
    {
        GtkWidget *btn = mode_button(ctx, (powergov_user_mode_t)i,
                                     ctx->mode_buttons[0]);
        gtk_box_pack_start(GTK_BOX(ctx->mode_box), btn, FALSE, FALSE, 0);
    }

    bat_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    ctx->battery_switch = gtk_switch_new();
    ctx->battery_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 5, 95, 5);
    gtk_range_set_value(GTK_RANGE(ctx->battery_scale), 80);
    ctx->battery_label = gtk_label_new(_(PG_TR_LABEL_BATTERY_PROTECT));
    gtk_box_pack_start(GTK_BOX(bat_box), ctx->battery_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bat_box), ctx->battery_switch, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bat_box), ctx->battery_scale, TRUE, TRUE, 0);
    ctx->battery_sw_handler = g_signal_connect(ctx->battery_switch, "state-set",
                                               G_CALLBACK(on_battery_switch), ctx);
    g_signal_connect(ctx->battery_scale, "button-release-event",
                     G_CALLBACK(on_battery_scale_release), ctx);

    log_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    ctx->activity_label = gtk_label_new(_(PG_TR_LABEL_RECENT_ACTIVITY));
    gtk_box_pack_start(GTK_BOX(log_box), ctx->activity_label, FALSE, FALSE, 0);
    {
        GtkWidget *log_scroll = make_action_log_scrolled();
        ctx->action_log_view = gtk_bin_get_child(GTK_BIN(log_scroll));
        gtk_box_pack_start(GTK_BOX(log_box), log_scroll, FALSE, FALSE, 0);
    }

    profile_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_box_pack_start(GTK_BOX(profile_page), ctx->mode_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(profile_page), bat_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(profile_page), log_box, FALSE, FALSE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(ctx->main_notebook), profile_page,
                             gtk_label_new(_(PG_TR_TAB_PROFILE)));

    ctx->dev_notebook = gtk_notebook_new();
    for (i = 0; i < 5; i++)
    {
        gtk_notebook_append_page(GTK_NOTEBOOK(ctx->dev_notebook), dev_pages[i],
                                 gtk_label_new(_(dev_tab_ids[i])));
    }

    dev_tab_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    ctx->dev_lock_banner = gtk_label_new(_(PG_TR_DEV_LOCK_BANNER));
    gtk_label_set_line_wrap(GTK_LABEL(ctx->dev_lock_banner), TRUE);
    gtk_label_set_xalign(GTK_LABEL(ctx->dev_lock_banner), 0.0);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(ctx->dev_lock_banner), "pg-dev-lock");
    gtk_box_pack_start(GTK_BOX(dev_tab_page), ctx->dev_lock_banner,
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(dev_tab_page), ctx->dev_notebook, TRUE, TRUE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(ctx->main_notebook), dev_tab_page,
                             gtk_label_new(_(PG_TR_TAB_DIAGNOSTIC)));

    gtk_box_pack_start(GTK_BOX(vbox), ctx->main_notebook, TRUE, TRUE, 0);

    ctx->uninstall_btn = gtk_button_new_with_label(_(PG_TR_BTN_UNINSTALL));
    g_signal_connect(ctx->uninstall_btn, "clicked",
                     G_CALLBACK(on_uninstall_clicked), ctx);
    gtk_widget_set_halign(ctx->uninstall_btn, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), ctx->uninstall_btn, FALSE, FALSE, 0);

    ctx->dev_btn = gtk_button_new_with_label(_(PG_TR_BTN_DEV_MODE));
    g_signal_connect(ctx->dev_btn, "clicked", G_CALLBACK(on_dev_btn_clicked), ctx);
    gtk_box_pack_start(GTK_BOX(vbox), ctx->dev_btn, FALSE, FALSE, 0);

    update_dev_tab_access(ctx);
}

int main(int argc, char **argv)
{
    AppCtx ctx;

    (void)argv;
    memset(&ctx, 0, sizeof(ctx));
    pg_i18n_init();
    gtk_init(&argc, &argv);
    apply_ui_theme();
    build_ui(&ctx);
    {
        char lang_log[160];

        pg_i18n_format_startup_log(lang_log, sizeof(lang_log));
        append_action_log(&ctx, lang_log);
        fprintf(stderr, "powergov-ui: %s\n", lang_log);
    }
    refresh_async(&ctx);
    ctx.timer_id = g_timeout_add(REFRESH_MS, on_timer, &ctx);
    gtk_widget_show_all(ctx.window);
    gtk_main();
    if (ctx.timer_id)
        g_source_remove(ctx.timer_id);
    if (ctx.feedback_timeout_id)
        g_source_remove(ctx.feedback_timeout_id);
    return 0;
}
