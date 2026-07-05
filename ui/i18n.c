/*
 * i18n.c - UI strings (English default; Spanish if system locale is es*)
 * Copyright (C) 2026 Iván Ezequiel Rodriguez
 * License: GPLv3+
 */
#include "i18n.h"
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_lang_en;
static char g_locale_detected[96];

static const char *const g_es[PG_TR_COUNT] =
{
    [PG_TR_CONNECTING] = "Conectando…",
    [PG_TR_TAB_PROFILE] = "Perfil",
    [PG_TR_TAB_DIAGNOSTIC] = "Diagnóstico",
    [PG_TR_TAB_SYSTEM] = "Sistema",
    [PG_TR_TAB_CPU] = "CPU",
    [PG_TR_TAB_COMPAT] = "Compat",
    [PG_TR_TAB_METRICS] = "Métricas",
    [PG_TR_TAB_LOG] = "Log",
    [PG_TR_BTN_DEV_MODE] = "Modo desarrollador",
    [PG_TR_BTN_USER_MODE] = "Volver a modo usuario",
    [PG_TR_BTN_START_SERVICE] = "Iniciar servicio",
    [PG_TR_BTN_INSTALL_SERVICE] = "Instalar servicio",
    [PG_TR_BTN_STOP] = "Detener",
    [PG_TR_BTN_RESTART] = "Reiniciar",
    [PG_TR_LABEL_BATTERY_PROTECT] = "Protección batería (%)",
    [PG_TR_LABEL_PROFILE_HINT] =
        "Seleccione cómo gestionar la energía de su equipo. El modo inteligente "
        "se ajusta automáticamente según la batería, la alimentación de CA y "
        "la carga del sistema. Perfil recomendado para la mayoría de los usuarios.",
    [PG_TR_LABEL_ADVANCED_MODES] = "Perfiles manuales",
    [PG_TR_LABEL_RECENT_ACTIVITY] = "Actividad reciente",
    [PG_TR_DEV_LOCK_BANNER] =
        "El diagnóstico técnico requiere permisos de administrador.\n"
        "Usá el botón «Modo desarrollador» para desbloquearlo.",
    [PG_TR_STATUS_NO_RESPOND] =
        "PowerGov no responde — prueba reiniciar el servicio",
    [PG_TR_STATUS_NOT_RUNNING] = "PowerGov no está en ejecución",
    [PG_TR_STATUS_NOT_INSTALLED] =
        "PowerGov no está instalado en este equipo",
    [PG_TR_STATUS_READ_ERROR] = "Error leyendo estado del daemon",
    [PG_TR_STATUS_ACTIVE_PROFILE] = "Perfil activo: %s",
    [PG_TR_POWER_PLUGGED] = "Enchufado",
    [PG_TR_POWER_BATTERY] = "Batería",
    [PG_TR_ERR_MANAGE_SERVICE] = "No se pudo gestionar el servicio",
    [PG_TR_LOG_SERVICE_REQUESTED] = "Se solicitó %s el servicio",
    [PG_TR_SVC_ACTION_START] = "iniciar",
    [PG_TR_SVC_ACTION_STOP] = "detener",
    [PG_TR_SVC_ACTION_RESTART] = "reiniciar",
    [PG_TR_ERR_PROFILE_CHANGE] =
        "No se pudo cambiar el perfil. ¿Está activo PowerGov?",
    [PG_TR_ERR_PROFILE_VERIFY] =
        "El cambio de perfil no se confirmó. Intentá de nuevo.",
    [PG_TR_LOG_PROFILE_FAIL] = "No se pudo cambiar a «%s»",
    [PG_TR_LOG_PROFILE_UNCONFIRMED] = "«%s» no quedó confirmado",
    [PG_TR_LOG_PROFILE_OK] = "Perfil «%s» activado",
    [PG_TR_FB_PROFILE_ACTIVE] = "Perfil activo: %s",
    [PG_TR_ERR_BATTERY_UPDATE] =
        "No se pudo actualizar la protección de batería",
    [PG_TR_ERR_BATTERY_VERIFY] = "El cambio de batería no se confirmó",
    [PG_TR_LOG_BATTERY_FAIL] = "No se pudo cambiar la protección de batería",
    [PG_TR_LOG_BATTERY_UNCONFIRMED] = "Protección de batería no confirmada",
    [PG_TR_LOG_BATTERY_ON] = "Protección de batería activa al %d%%",
    [PG_TR_FB_BATTERY_ON] = "Protección activa al %d%%",
    [PG_TR_LOG_BATTERY_OFF] = "Protección de batería desactivada",
    [PG_TR_FB_BATTERY_OFF] = "Protección de batería desactivada",
    [PG_TR_LOG_DEV_ON] = "Modo desarrollador activado",
    [PG_TR_FB_DEV_ON] = "Modo desarrollador activo",
    [PG_TR_LOG_DEV_OFF] = "Modo usuario restaurado",
    [PG_TR_FB_DEV_OFF] = "Volviste al modo usuario",
    [PG_TR_ERR_DEV_DENIED] = "Acceso al diagnóstico cancelado o denegado",
    [PG_TR_LOG_DEV_DENIED] = "Diagnóstico no desbloqueado",
    [PG_TR_ERR_DEV_INCOMPLETE] =
        "PowerGov no está instalado por completo en este equipo",
    [PG_TR_LOG_DEV_INCOMPLETE] =
        "Diagnóstico no disponible (instalación incompleta)",
    [PG_TR_ERR_DEV_NO_PKEXEC] =
        "No se pueden solicitar permisos de administrador",
    [PG_TR_LOG_DEV_UNAVAILABLE] = "Diagnóstico no disponible",
    [PG_TR_ERR_DEV_DIALOG] = "No se pudo abrir el diálogo de permisos",
    [PG_TR_LOG_DEV_PERM_ERROR] = "Error al solicitar permisos",
    [PG_TR_NO_LOG] = "(sin log)",
    [PG_TR_SYS_FMT] =
        "SO: %s\nKernel: %s\npowergov: %s\nsystemd: %s\nPPD activo: %s",
    [PG_TR_CPU_FMT] =
        "Modelo: %s\nCPUs: %d\nDriver: %s\nGovernor: %s\n"
        "Governors: %s\nEPP: %s (%s)\nTurbo: %s\n"
        "Freq max HW: %s kHz\nFreq max scaling: %s kHz\n"
        "Platform profile: %s\nRAPL: %s",
    [PG_TR_COMPAT_SCORE_FMT] = "Score: %d — %s\n\n",
    [PG_TR_COMPAT_ROW_FMT] = "%-12s [%s] hw=%d en=%d — %s\n",
    [PG_TR_COMPAT_SUMMARY_FMT] =
        "%d subsistemas soportados, %d parciales, de %d totales.",
    [PG_TR_COMPAT_ST_UNSUPPORTED] = "no soportado",
    [PG_TR_COMPAT_ST_SUPPORTED] = "soportado",
    [PG_TR_COMPAT_ST_PARTIAL] = "parcial",
    [PG_TR_COMPAT_ST_CONFLICT] = "conflicto",
    [PG_TR_CORE_NO_METRICS] = "(sin métricas)",
    [PG_TR_CORE_LOG_READ_FAIL] = "No se pudo leer %s",
    [PG_TR_YES] = "sí",
    [PG_TR_NO] = "no",
    [PG_TR_ACTIVE] = "activo",
    [PG_TR_INACTIVE] = "inactivo",
    [PG_TR_LANG_TOOLTIP] = "Cambiar a inglés",
    [PG_TR_INSTALL_DIALOG_TITLE] = "Instalar servicio PowerGov",
    [PG_TR_INSTALL_DIALOG_BODY] =
        "PowerGov necesita un servicio en segundo plano en tu PC para:\n"
        "• Cambiar perfiles de energía (CPU, batería, rendimiento)\n"
        "• Mantener la protección de batería mientras usás el portátil\n"
        "• Aplicar ajustes de forma continua sin abrir la aplicación\n\n"
        "La instalación pide contraseña de administrador una sola vez.\n"
        "¿Querés instalarlo ahora?",
    [PG_TR_INSTALL_DIALOG_YES] = "Instalar",
    [PG_TR_INSTALL_DIALOG_NO] = "Ahora no",
    [PG_TR_ERR_SERVICE_NOT_RUNNING] =
        "El servicio no está activo. Usá «Iniciar servicio».",
    [PG_TR_ERR_INSTALL_UNAVAILABLE] =
        "No se encontraron archivos para instalar el servicio",
    [PG_TR_ERR_INSTALL_FAILED] = "No se pudo instalar el servicio",
    [PG_TR_ERR_INSTALL_DENIED] = "Instalación cancelada o denegada",
    [PG_TR_LOG_INSTALL_STARTED] = "Instalación del servicio solicitada",
    [PG_TR_LOG_INSTALL_OK] = "Servicio PowerGov instalado",
    [PG_TR_FB_INSTALL_OK] = "Servicio instalado y en ejecución",
    [PG_TR_FB_INSTALL_PREPARING] = "Preparando instalación del servicio…",
    [PG_TR_FB_INSTALL_RUNNING] = "Instalando servicio (puede pedir contraseña de admin)…",
    [PG_TR_BTN_UNINSTALL] = "Desinstalar",
    [PG_TR_UNINSTALL_DIALOG_TITLE] = "Desinstalar PowerGov",
    [PG_TR_UNINSTALL_DIALOG_BODY] =
        "Se detendrá el servicio, se eliminarán los archivos del sistema "
        "y los accesos directos de usuario.\n\n"
        "¿Desea continuar?",
    [PG_TR_UNINSTALL_DIALOG_YES] = "Sí, desinstalar",
    [PG_TR_UNINSTALL_DIALOG_NO] = "Cancelar",
    [PG_TR_ERR_UNINSTALL_UNAVAILABLE] =
        "No se encontró el script de desinstalación",
    [PG_TR_ERR_UNINSTALL_FAILED] = "No se pudo iniciar la desinstalación",
    [PG_TR_ERR_UNINSTALL_DENIED] = "Desinstalación cancelada o denegada",
    [PG_TR_LOG_UNINSTALL_STARTED] = "Desinstalación solicitada",
    [PG_TR_LOG_UNINSTALL_OK] = "PowerGov desinstalado",
    [PG_TR_FB_UNINSTALL_OK] = "Desinstalación completada. Cierre la aplicación.",
    [PG_TR_FB_UNINSTALL_RUNNING] =
        "Desinstalando (puede pedir contraseña de administrador)…",
    [PG_TR_UPDATE_AVAILABLE_TITLE] = "Hay una versión nueva de PowerGov",
    [PG_TR_UPDATE_AVAILABLE_BODY] =
        "Versión %s disponible en GitHub (instalada: %s).\n\n"
        "Abrí la página de descarga para obtener el AppImage o el tarball.",
    [PG_TR_UPDATE_BTN_LATER] = "Ahora no",
    [PG_TR_UPDATE_BTN_OPEN] = "Abrir descarga",
    [PG_TR_UPDATE_BTN_INSTALL] = "Instalar",
    [PG_TR_UPDATE_FB_INSTALL_STARTED] =
        "Descargando e instalando en segundo plano. "
        "Log: ~/.config/powergov/update-install.log",
    [PG_TR_LOG_LANG_AUTO] =
        "Idioma: español (detectado del sistema: %s)",
};

static const char *const g_en[PG_TR_COUNT] =
{
    [PG_TR_CONNECTING] = "Connecting…",
    [PG_TR_TAB_PROFILE] = "Profile",
    [PG_TR_TAB_DIAGNOSTIC] = "Diagnostics",
    [PG_TR_TAB_SYSTEM] = "System",
    [PG_TR_TAB_CPU] = "CPU",
    [PG_TR_TAB_COMPAT] = "Compat",
    [PG_TR_TAB_METRICS] = "Metrics",
    [PG_TR_TAB_LOG] = "Log",
    [PG_TR_BTN_DEV_MODE] = "Developer mode",
    [PG_TR_BTN_USER_MODE] = "Back to user mode",
    [PG_TR_BTN_START_SERVICE] = "Start service",
    [PG_TR_BTN_INSTALL_SERVICE] = "Install service",
    [PG_TR_BTN_STOP] = "Stop",
    [PG_TR_BTN_RESTART] = "Restart",
    [PG_TR_LABEL_BATTERY_PROTECT] = "Battery protection (%)",
    [PG_TR_LABEL_PROFILE_HINT] =
        "Select how to manage power on your device. Smart mode adjusts "
        "automatically based on battery status, AC power, and system load. "
        "Recommended default profile.",
    [PG_TR_LABEL_ADVANCED_MODES] = "Manual profiles",
    [PG_TR_LABEL_RECENT_ACTIVITY] = "Recent activity",
    [PG_TR_DEV_LOCK_BANNER] =
        "Technical diagnostics require administrator privileges.\n"
        "Use the «Developer mode» button to unlock.",
    [PG_TR_STATUS_NO_RESPOND] =
        "PowerGov is not responding — try restarting the service",
    [PG_TR_STATUS_NOT_RUNNING] = "PowerGov is not running",
    [PG_TR_STATUS_NOT_INSTALLED] =
        "PowerGov is not installed on this system",
    [PG_TR_STATUS_READ_ERROR] = "Error reading daemon status",
    [PG_TR_STATUS_ACTIVE_PROFILE] = "Active profile: %s",
    [PG_TR_POWER_PLUGGED] = "Plugged in",
    [PG_TR_POWER_BATTERY] = "Battery",
    [PG_TR_ERR_MANAGE_SERVICE] = "Could not manage the service",
    [PG_TR_LOG_SERVICE_REQUESTED] = "Requested to %s the service",
    [PG_TR_SVC_ACTION_START] = "start",
    [PG_TR_SVC_ACTION_STOP] = "stop",
    [PG_TR_SVC_ACTION_RESTART] = "restart",
    [PG_TR_ERR_PROFILE_CHANGE] =
        "Could not change profile. Is PowerGov running?",
    [PG_TR_ERR_PROFILE_VERIFY] =
        "Profile change was not confirmed. Try again.",
    [PG_TR_LOG_PROFILE_FAIL] = "Could not switch to «%s»",
    [PG_TR_LOG_PROFILE_UNCONFIRMED] = "«%s» was not confirmed",
    [PG_TR_LOG_PROFILE_OK] = "Profile «%s» activated",
    [PG_TR_FB_PROFILE_ACTIVE] = "Active profile: %s",
    [PG_TR_ERR_BATTERY_UPDATE] = "Could not update battery protection",
    [PG_TR_ERR_BATTERY_VERIFY] = "Battery change was not confirmed",
    [PG_TR_LOG_BATTERY_FAIL] = "Could not change battery protection",
    [PG_TR_LOG_BATTERY_UNCONFIRMED] = "Battery protection not confirmed",
    [PG_TR_LOG_BATTERY_ON] = "Battery protection active at %d%%",
    [PG_TR_FB_BATTERY_ON] = "Protection active at %d%%",
    [PG_TR_LOG_BATTERY_OFF] = "Battery protection disabled",
    [PG_TR_FB_BATTERY_OFF] = "Battery protection disabled",
    [PG_TR_LOG_DEV_ON] = "Developer mode enabled",
    [PG_TR_FB_DEV_ON] = "Developer mode active",
    [PG_TR_LOG_DEV_OFF] = "User mode restored",
    [PG_TR_FB_DEV_OFF] = "Back to user mode",
    [PG_TR_ERR_DEV_DENIED] = "Diagnostics access cancelled or denied",
    [PG_TR_LOG_DEV_DENIED] = "Diagnostics not unlocked",
    [PG_TR_ERR_DEV_INCOMPLETE] =
        "PowerGov is not fully installed on this system",
    [PG_TR_LOG_DEV_INCOMPLETE] =
        "Diagnostics unavailable (incomplete install)",
    [PG_TR_ERR_DEV_NO_PKEXEC] = "Cannot request administrator privileges",
    [PG_TR_LOG_DEV_UNAVAILABLE] = "Diagnostics unavailable",
    [PG_TR_ERR_DEV_DIALOG] = "Could not open permissions dialog",
    [PG_TR_LOG_DEV_PERM_ERROR] = "Error requesting permissions",
    [PG_TR_NO_LOG] = "(no log)",
    [PG_TR_SYS_FMT] =
        "OS: %s\nKernel: %s\npowergov: %s\nsystemd: %s\nPPD active: %s",
    [PG_TR_CPU_FMT] =
        "Model: %s\nCPUs: %d\nDriver: %s\nGovernor: %s\n"
        "Governors: %s\nEPP: %s (%s)\nTurbo: %s\n"
        "HW max freq: %s kHz\nScaling max freq: %s kHz\n"
        "Platform profile: %s\nRAPL: %s",
    [PG_TR_COMPAT_SCORE_FMT] = "Score: %d — %s\n\n",
    [PG_TR_COMPAT_ROW_FMT] = "%-12s [%s] hw=%d en=%d — %s\n",
    [PG_TR_COMPAT_SUMMARY_FMT] =
        "%d subsystems supported, %d partial, of %d total.",
    [PG_TR_COMPAT_ST_UNSUPPORTED] = "unsupported",
    [PG_TR_COMPAT_ST_SUPPORTED] = "supported",
    [PG_TR_COMPAT_ST_PARTIAL] = "partial",
    [PG_TR_COMPAT_ST_CONFLICT] = "conflict",
    [PG_TR_CORE_NO_METRICS] = "(no metrics)",
    [PG_TR_CORE_LOG_READ_FAIL] = "Could not read %s",
    [PG_TR_YES] = "yes",
    [PG_TR_NO] = "no",
    [PG_TR_ACTIVE] = "active",
    [PG_TR_INACTIVE] = "inactive",
    [PG_TR_LANG_TOOLTIP] = "Switch to Spanish",
    [PG_TR_INSTALL_DIALOG_TITLE] = "Install PowerGov service",
    [PG_TR_INSTALL_DIALOG_BODY] =
        "PowerGov needs a background service on your PC to:\n"
        "• Change power profiles (CPU, battery, performance)\n"
        "• Keep battery protection active while you use the laptop\n"
        "• Apply settings continuously without keeping the app open\n\n"
        "Installation asks for your administrator password once.\n"
        "Do you want to install it now?",
    [PG_TR_INSTALL_DIALOG_YES] = "Install",
    [PG_TR_INSTALL_DIALOG_NO] = "Not now",
    [PG_TR_ERR_SERVICE_NOT_RUNNING] =
        "The service is not running. Use «Start service».",
    [PG_TR_ERR_INSTALL_UNAVAILABLE] =
        "Could not find files to install the service",
    [PG_TR_ERR_INSTALL_FAILED] = "Could not install the service",
    [PG_TR_ERR_INSTALL_DENIED] = "Installation cancelled or denied",
    [PG_TR_LOG_INSTALL_STARTED] = "Service installation requested",
    [PG_TR_LOG_INSTALL_OK] = "PowerGov service installed",
    [PG_TR_FB_INSTALL_OK] = "Service installed and running",
    [PG_TR_FB_INSTALL_PREPARING] = "Preparing service installation…",
    [PG_TR_FB_INSTALL_RUNNING] = "Installing service (admin password may be required)…",
    [PG_TR_BTN_UNINSTALL] = "Uninstall",
    [PG_TR_UNINSTALL_DIALOG_TITLE] = "Uninstall PowerGov",
    [PG_TR_UNINSTALL_DIALOG_BODY] =
        "The service will stop, system files will be removed, "
        "and user shortcuts will be cleared.\n\n"
        "Do you want to continue?",
    [PG_TR_UNINSTALL_DIALOG_YES] = "Yes, uninstall",
    [PG_TR_UNINSTALL_DIALOG_NO] = "Cancel",
    [PG_TR_ERR_UNINSTALL_UNAVAILABLE] = "Uninstall script not found",
    [PG_TR_ERR_UNINSTALL_FAILED] = "Could not start uninstall",
    [PG_TR_ERR_UNINSTALL_DENIED] = "Uninstall cancelled or denied",
    [PG_TR_LOG_UNINSTALL_STARTED] = "Uninstall requested",
    [PG_TR_LOG_UNINSTALL_OK] = "PowerGov uninstalled",
    [PG_TR_FB_UNINSTALL_OK] = "Uninstall complete. You may close the app.",
    [PG_TR_FB_UNINSTALL_RUNNING] =
        "Uninstalling (admin password may be required)…",
    [PG_TR_UPDATE_AVAILABLE_TITLE] = "A new PowerGov release is available",
    [PG_TR_UPDATE_AVAILABLE_BODY] =
        "Version %s is on GitHub (installed: %s).\n\n"
        "Open the download page to get the AppImage or tarball.",
    [PG_TR_UPDATE_BTN_LATER] = "Not now",
    [PG_TR_UPDATE_BTN_OPEN] = "Open download",
    [PG_TR_UPDATE_BTN_INSTALL] = "Install",
    [PG_TR_UPDATE_FB_INSTALL_STARTED] =
        "Downloading and installing in the background. "
        "Log: ~/.config/powergov/update-install.log",
    [PG_TR_LOG_LANG_AUTO] =
        "Language: English (detected from system: %s)",
};

static int locale_is_spanish(const char *lang)
{
    if (!lang || !lang[0])
        return 0;
    return (lang[0] == 'e' && lang[1] == 's' &&
            (lang[2] == '\0' || lang[2] == '_' || lang[2] == '-'));
}

static void copy_locale_token(const char *lang, char *out, size_t outsz)
{
    const char *end;
    size_t n;

    if (!out || outsz == 0)
        return;

    out[0] = '\0';
    if (!lang || !lang[0])
        return;

    end = lang;
    while (*end && *end != ':' && *end != '@' && *end != '.')
        end++;

    n = (size_t)(end - lang);
    if (n >= outsz)
        n = outsz - 1;

    memcpy(out, lang, n);
    out[n] = '\0';
}

static const char *locale_env_source(void)
{
    const char *lang;

    lang = getenv("LANGUAGE");
    if (lang && lang[0])
        return lang;

    lang = getenv("LC_MESSAGES");
    if (lang && lang[0])
        return lang;

    lang = getenv("LANG");
    if (lang && lang[0])
        return lang;

    return NULL;
}

static void detect_system_language(void)
{
    const char *raw;
    char token[32];
    const char *resolved;

    g_lang_en = 1;
    g_locale_detected[0] = '\0';

    setlocale(LC_ALL, "");

    raw = locale_env_source();
    if (raw)
    {
        copy_locale_token(raw, token, sizeof(token));
        snprintf(g_locale_detected, sizeof(g_locale_detected), "%s", raw);
    }
    else
    {
        resolved = setlocale(LC_MESSAGES, NULL);
        if (resolved && resolved[0] && strcmp(resolved, "C") != 0 &&
            strcmp(resolved, "POSIX") != 0)
        {
            copy_locale_token(resolved, token, sizeof(token));
            snprintf(g_locale_detected, sizeof(g_locale_detected),
                     "%s", resolved);
        }
        else
        {
            snprintf(g_locale_detected, sizeof(g_locale_detected), "%s",
                     "C/POSIX→en");
            token[0] = '\0';
        }
    }

    if (locale_is_spanish(token))
        g_lang_en = 0;
}

void pg_i18n_init(void)
{
    detect_system_language();
}

void pg_i18n_format_startup_log(char *buf, size_t bufsz)
{
    if (!buf || bufsz == 0)
        return;

    snprintf(buf, bufsz, pg_tr(PG_TR_LOG_LANG_AUTO), g_locale_detected);
}

void pg_i18n_set_english(int english)
{
    g_lang_en = english ? 1 : 0;
}

void pg_i18n_toggle(void)
{
    g_lang_en = !g_lang_en;
}

const char *pg_lang_button_label(void)
{
    return g_lang_en ? "EN" : "ES";
}

const char *pg_lang_button_tooltip(void)
{
    return pg_tr(PG_TR_LANG_TOOLTIP);
}

int pg_i18n_is_english(void)
{
    return g_lang_en;
}

const char *pg_tr(PgTr id)
{
    if (id < 0 || id >= PG_TR_COUNT)
        return "";
    return g_lang_en ? g_en[id] : g_es[id];
}

const char *pg_user_mode_title(powergov_user_mode_t mode)
{
    if (g_lang_en)
    {
        switch (mode)
        {
        case POWERGOV_USER_MAX_BATTERY: return "Smart mode";
        case POWERGOV_USER_BALANCED:    return "Balanced";
        case POWERGOV_USER_PERFORMANCE: return "Max performance";
        default:                        return "Unknown";
        }
    }

    switch (mode)
    {
    case POWERGOV_USER_MAX_BATTERY: return "Modo inteligente";
    case POWERGOV_USER_BALANCED:    return "Equilibrado";
    case POWERGOV_USER_PERFORMANCE: return "Máximo rendimiento";
    default:                        return "Desconocido";
    }
}

const char *pg_user_mode_subtitle(powergov_user_mode_t mode)
{
    if (g_lang_en)
    {
        switch (mode)
        {
        case POWERGOV_USER_MAX_BATTERY:
            return "Automatic: efficiency on battery, higher performance on AC";
        case POWERGOV_USER_BALANCED:
            return "Manual middle ground between battery and speed";
        case POWERGOV_USER_PERFORMANCE:
            return "Manual — prioritize speed even on battery";
        default:
            return "";
        }
    }

    switch (mode)
    {
    case POWERGOV_USER_MAX_BATTERY:
        return "Automático: eficiencia con batería, mayor rendimiento con CA";
    case POWERGOV_USER_BALANCED:
        return "Intermedio manual entre autonomía y velocidad";
    case POWERGOV_USER_PERFORMANCE:
        return "Manual — prioriza velocidad incluso en batería";
    default:
        return "";
    }
}

const char *pg_service_action_label(const char *systemctl_action)
{
    if (!systemctl_action)
        return "";

    if (strcmp(systemctl_action, "start") == 0)
        return pg_tr(PG_TR_SVC_ACTION_START);
    if (strcmp(systemctl_action, "stop") == 0)
        return pg_tr(PG_TR_SVC_ACTION_STOP);
    if (strcmp(systemctl_action, "restart") == 0)
        return pg_tr(PG_TR_SVC_ACTION_RESTART);

    return systemctl_action;
}

const char *pg_compat_state_label(int state)
{
    switch (state)
    {
    case POWERGOV_COMPAT_SUPPORTED:
        return pg_tr(PG_TR_COMPAT_ST_SUPPORTED);
    case POWERGOV_COMPAT_PARTIAL:
        return pg_tr(PG_TR_COMPAT_ST_PARTIAL);
    case POWERGOV_COMPAT_CONFLICT:
        return pg_tr(PG_TR_COMPAT_ST_CONFLICT);
    default:
        return pg_tr(PG_TR_COMPAT_ST_UNSUPPORTED);
    }
}

const char *pg_compat_detail_tr(const char *detail_en)
{
    if (!detail_en || !detail_en[0])
        return "";

    if (pg_i18n_is_english())
        return detail_en;

    if (strcmp(detail_en, "Not available on this hardware/kernel.") == 0)
        return "No disponible en este hardware/kernel.";
    if (strcmp(detail_en,
               "power-profiles-daemon active; powergov skips platform_profile.") == 0)
        return "power-profiles-daemon activo; powergov omite platform_profile.";
    if (strcmp(detail_en, "Available; driver may impose a frequency floor.") == 0)
        return "Disponible; el driver puede imponer piso de frecuencia.";
    if (strcmp(detail_en, "PCI/USB; effect depends on each device.") == 0)
        return "PCI/USB; efecto depende de cada dispositivo.";
    if (strcmp(detail_en, "Sysfs present.") == 0)
        return "Sysfs presente.";

    return detail_en;
}

void pg_compat_format_summary(int supported, int partial, int total,
                              char *buf, size_t bufsz)
{
    if (!buf || bufsz == 0)
        return;

    snprintf(buf, bufsz, pg_tr(PG_TR_COMPAT_SUMMARY_FMT),
             supported, partial, total);
}

const char *pg_core_dev_text_tr(const char *text_en)
{
    char expect[128];
    const char *path;

    if (!text_en || !text_en[0])
        return text_en;

    if (pg_i18n_is_english())
        return text_en;

    if (strcmp(text_en, "(no metrics)") == 0)
        return pg_tr(PG_TR_CORE_NO_METRICS);

    path = POWERGOV_LOG_PATH;
    snprintf(expect, sizeof(expect), "Could not read %s", path);
    if (strcmp(text_en, expect) == 0)
    {
        static char log_fail[160];
        snprintf(log_fail, sizeof(log_fail), pg_tr(PG_TR_CORE_LOG_READ_FAIL),
                 path);
        return log_fail;
    }

    return text_en;
}
