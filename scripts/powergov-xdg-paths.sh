#!/usr/bin/env bash
# Shared XDG / locale-safe path helpers for PowerGov user scripts.
# Source from other scripts: . "$(dirname "$0")/powergov-xdg-paths.sh"
# shellcheck shell=bash

pg_canonical_path() {
    local d=$1
    if [[ -d "${d}" ]] && command -v realpath >/dev/null 2>&1; then
        realpath "${d}"
    else
        printf '%s' "${d}"
    fi
}

# Prints one existing desktop directory per line (deduplicated).
pg_desktop_dirs() {
    local -a candidates=()
    local primary="" d canon
    declare -A seen=()

    if command -v xdg-user-dir >/dev/null 2>&1; then
        primary="$(xdg-user-dir DESKTOP 2>/dev/null || true)"
        [[ -n "${primary}" ]] && candidates+=("${primary}")
    fi

    [[ -n "${XDG_DESKTOP_DIR:-}" ]] && candidates+=("${XDG_DESKTOP_DIR}")

    if [[ -n "${HOME:-}" ]]; then
        candidates+=("${HOME}/Desktop" "${HOME}/Escritorio")
    fi

    for d in "${candidates[@]}"; do
        [[ -z "${d}" || "${d}" == "/" ]] && continue
        [[ -d "${d}" ]] || continue
        canon="$(pg_canonical_path "${d}")"
        [[ -n "${canon}" && -z "${seen[${canon}]+x}" ]] || continue
        seen["${canon}"]=1
        printf '%s\n' "${d}"
    done
}

# Prints one existing download directory per line (deduplicated).
pg_download_dirs() {
    local -a candidates=()
    local d canon
    declare -A seen=()

    if command -v xdg-user-dir >/dev/null 2>&1; then
        d="$(xdg-user-dir DOWNLOAD 2>/dev/null || true)"
        [[ -n "${d}" ]] && candidates+=("${d}")
    fi

    [[ -n "${XDG_DOWNLOAD_DIR:-}" ]] && candidates+=("${XDG_DOWNLOAD_DIR}")

    if [[ -n "${HOME:-}" ]]; then
        candidates+=("${HOME}/Downloads" "${HOME}/Descargas")
    fi

    for d in "${candidates[@]}"; do
        [[ -z "${d}" || "${d}" == "/" ]] && continue
        [[ -d "${d}" ]] || continue
        canon="$(pg_canonical_path "${d}")"
        [[ -n "${canon}" && -z "${seen[${canon}]+x}" ]] || continue
        seen["${canon}"]=1
        printf '%s\n' "${d}"
    done
}

# Default desktop dir for mkdir/install when none exists yet.
pg_default_desktop_dir() {
    local d

    while IFS= read -r d; do
        [[ -n "${d}" ]] || continue
        printf '%s' "${d}"
        return 0
    done < <(pg_desktop_dirs)

    d="${XDG_DESKTOP_DIR:-}"
    if [[ -z "${d}" ]] && command -v xdg-user-dir >/dev/null 2>&1; then
        d="$(xdg-user-dir DESKTOP 2>/dev/null || true)"
    fi
    if [[ -z "${d}" && -n "${HOME:-}" ]]; then
        d="${HOME}/Desktop"
    fi
    [[ -n "${d}" ]] || return 1
    mkdir -p "${d}"
    printf '%s' "${d}"
}
