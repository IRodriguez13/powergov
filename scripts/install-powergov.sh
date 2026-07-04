#!/usr/bin/env bash
# One-click installer: build + systemd service + GTK UI + desktop shortcut.
set -euo pipefail

ROOT="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")" && pwd)"
cd "$ROOT"

log() { echo "[powergov-install] $*" >&2; }

ui_lang_es() {
    case "${LANG:-}${LC_MESSAGES:-}" in es*|ES*) return 0 ;; esac
    return 1
}

msg() {
    if ui_lang_es; then
        case "$1" in
            welcome) echo "Se instalará PowerGov (servicio en segundo plano + aplicación de escritorio)." ;;
            confirm) echo "¿Continuar con la instalación?" ;;
            title) echo "Instalar PowerGov" ;;
            deps) echo "Faltan dependencias. Instalá paquetes de desarrollo e intentá de nuevo." ;;
            building) echo "Compilando PowerGov…" ;;
            installing) echo "Instalando (se pedirá contraseña de administrador)…" ;;
            done_ok) echo "PowerGov instalado correctamente." ;;
            done_body) echo "El servicio está activo y el acceso directo debería estar en el escritorio." ;;
            launch) echo "¿Abrir PowerGov ahora?" ;;
            fail) echo "La instalación falló." ;;
            need_make) echo "make" ;;
            need_gcc) echo "gcc (build-essential)" ;;
            need_pkg) echo "pkg-config" ;;
            need_gtk) echo "gtk+-3.0 (libgtk-3-dev)" ;;
            need_zenity) echo "zenity (recomendado para diálogos)" ;;
        esac
    else
        case "$1" in
            welcome) echo "PowerGov will be installed (background service + desktop app)." ;;
            confirm) echo "Continue with installation?" ;;
            title) echo "Install PowerGov" ;;
            deps) echo "Missing dependencies. Install development packages and try again." ;;
            building) echo "Building PowerGov…" ;;
            installing) echo "Installing (administrator password required)…" ;;
            done_ok) echo "PowerGov installed successfully." ;;
            done_body) echo "The service is running and a desktop shortcut should be available." ;;
            launch) echo "Open PowerGov now?" ;;
            fail) echo "Installation failed." ;;
            need_make) echo "make" ;;
            need_gcc) echo "gcc (build-essential)" ;;
            need_pkg) echo "pkg-config" ;;
            need_gtk) echo "gtk+-3.0 (libgtk-3-dev)" ;;
            need_zenity) echo "zenity (recommended for dialogs)" ;;
        esac
    fi
}

have_cmd() { command -v "$1" >/dev/null 2>&1; }

check_deps() {
    local missing=()
    have_cmd make || missing+=("$(msg need_make)")
    have_cmd gcc || missing+=("$(msg need_gcc)")
    have_cmd pkg-config || missing+=("$(msg need_pkg)")
    pkg-config --exists gtk+-3.0 2>/dev/null || missing+=("$(msg need_gtk)")
    if ((${#missing[@]})); then
        if have_cmd zenity; then
            zenity --error --width=420 --title="$(msg title)" \
                --text="$(msg deps)\n\n${missing[*]}"
        else
            log "$(msg deps): ${missing[*]}"
        fi
        return 1
    fi
    return 0
}

sudo_with_ui() {
    local askpass
    askpass="$(mktemp)"
    trap 'rm -f "$askpass"' RETURN
    cat > "$askpass" <<'EOF'
#!/bin/sh
exec zenity --password --title="PowerGov"
EOF
    chmod 700 "$askpass"
    export SUDO_ASKPASS="$askpass"
    sudo -A "$@"
}

run_step() {
    local label="$1"
    shift
    if have_cmd zenity; then
        (
            echo "10"; echo "# $label"
            "$@"
            echo "100"; echo "# OK"
        ) 2>/dev/null | zenity --progress --auto-close --no-cancel --title="$(msg title)" \
            --text="$label" 2>/dev/null || "$@"
    else
        log "$label"
        "$@"
    fi
}

main() {
    if have_cmd zenity; then
        zenity --question --width=420 --title="$(msg title)" \
            --text="$(msg welcome)\n\n$(msg confirm)" || exit 0
    else
        log "$(msg welcome)"
        read -r -p "$(msg confirm) [y/N] " ans
        [[ "${ans,,}" == y* ]] || exit 0
    fi

    check_deps || return 1

    run_step "$(msg building)" make -j"$(nproc 2>/dev/null || echo 2)"
    run_step "$(msg building)" make powergov-ui

    log "$(msg installing)"
    sudo_with_ui make install-service
    sudo_with_ui make install-ui install-ui-policy install-ui-helper

    if have_cmd zenity; then
        zenity --info --width=420 --title="$(msg title)" \
            --text="$(msg done_ok)\n\n$(msg done_body)" 2>/dev/null || true
        if zenity --question --width=420 --title="$(msg title)" \
            --text="$(msg launch)" 2>/dev/null; then
            powergov-ui >/dev/null 2>&1 &
        fi
    else
        log "$(msg done_ok)"
        log "$(msg done_body)"
    fi
    return 0
}

if ! main; then
    if have_cmd zenity; then
        zenity --error --title="$(msg title)" --text="$(msg fail)" 2>/dev/null || true
    fi
    exit 1
fi
