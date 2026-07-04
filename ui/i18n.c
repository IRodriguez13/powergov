/*
 * i18n.c - UI strings (Spanish default, English optional)
 * Copyright (C) 2026 Iván Ezequiel Rodriguez
 * License: GPLv3+
 */
#include "i18n.h"
#include <locale.h>
#include <stdlib.h>
#include <string.h>

static int g_lang_en;

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
    [PG_TR_BTN_STOP] = "Detener",
    [PG_TR_BTN_RESTART] = "Reiniciar",
    [PG_TR_LABEL_BATTERY_PROTECT] = "Protección batería (%)",
    [PG_TR_LABEL_RECENT_ACTIVITY] = "Actividad reciente",
    [PG_TR_DEV_LOCK_BANNER] =
        "El diagnóstico técnico requiere permisos de administrador.\n"
        "Usá el botón «Modo desarrollador» para desbloquearlo.",
    [PG_TR_STATUS_NO_RESPOND] =
        "PowerGov no responde — prueba reiniciar el servicio",
    [PG_TR_STATUS_NOT_RUNNING] = "PowerGov no está en ejecución",
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
    [PG_TR_YES] = "sí",
    [PG_TR_NO] = "no",
    [PG_TR_ACTIVE] = "activo",
    [PG_TR_INACTIVE] = "inactivo",
    [PG_TR_LANG_TOOLTIP] = "Cambiar a inglés",
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
    [PG_TR_BTN_STOP] = "Stop",
    [PG_TR_BTN_RESTART] = "Restart",
    [PG_TR_LABEL_BATTERY_PROTECT] = "Battery protection (%)",
    [PG_TR_LABEL_RECENT_ACTIVITY] = "Recent activity",
    [PG_TR_DEV_LOCK_BANNER] =
        "Technical diagnostics require administrator privileges.\n"
        "Use the «Developer mode» button to unlock.",
    [PG_TR_STATUS_NO_RESPOND] =
        "PowerGov is not responding — try restarting the service",
    [PG_TR_STATUS_NOT_RUNNING] = "PowerGov is not running",
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
    [PG_TR_YES] = "yes",
    [PG_TR_NO] = "no",
    [PG_TR_ACTIVE] = "active",
    [PG_TR_INACTIVE] = "inactive",
    [PG_TR_LANG_TOOLTIP] = "Switch to Spanish",
};

static int locale_is_spanish(const char *lang)
{
    if (!lang || !lang[0])
        return 0;
    return (lang[0] == 'e' && lang[1] == 's' &&
            (lang[2] == '\0' || lang[2] == '_' || lang[2] == '-'));
}

static int locale_is_english(const char *lang)
{
    if (!lang || !lang[0])
        return 0;
    return (lang[0] == 'e' && lang[1] == 'n' &&
            (lang[2] == '\0' || lang[2] == '_' || lang[2] == '-'));
}

void pg_i18n_init(void)
{
    const char *lang;

    setlocale(LC_ALL, "");
    g_lang_en = 1;

    lang = getenv("LANGUAGE");
    if (!lang || !lang[0])
        lang = getenv("LC_MESSAGES");
    if (!lang || !lang[0])
        lang = getenv("LANG");

    if (locale_is_spanish(lang))
        g_lang_en = 0;
    else if (locale_is_english(lang))
        g_lang_en = 1;
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
        case POWERGOV_USER_MAX_BATTERY: return "Max battery life";
        case POWERGOV_USER_BALANCED:    return "Balanced";
        case POWERGOV_USER_PERFORMANCE: return "Max performance";
        default:                        return "Unknown";
        }
    }

    switch (mode)
    {
    case POWERGOV_USER_MAX_BATTERY: return "Máxima autonomía";
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
            return "Recommended on battery (Smart mode)";
        case POWERGOV_USER_BALANCED:
            return "Balance performance and battery";
        case POWERGOV_USER_PERFORMANCE:
            return "Prioritize performance even on battery";
        default:
            return "";
        }
    }

    switch (mode)
    {
    case POWERGOV_USER_MAX_BATTERY:
        return "Recomendado en batería (modo Smart)";
    case POWERGOV_USER_BALANCED:
        return "Balance rendimiento y batería";
    case POWERGOV_USER_PERFORMANCE:
        return "Prioriza rendimiento incluso en batería";
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
