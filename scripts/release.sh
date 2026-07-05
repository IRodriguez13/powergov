#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

VERSION="$(tr -d '[:space:]' < VERSION)"
TAG="v${VERSION}"
TAR="dist/powergov-${VERSION}.tar.gz"
GH="${GH:-gh}"

if ! command -v "$GH" >/dev/null 2>&1; then
    if [[ -x /tmp/gh_2.67.0_linux_amd64/bin/gh ]]; then
        GH=/tmp/gh_2.67.0_linux_amd64/bin/gh
    else
        echo "error: gh CLI not found (install: sudo apt install gh)" >&2
        exit 1
    fi
fi

make pack

APPIMAGE="dist/PowerGov-${VERSION}-x86_64.AppImage"
if make appimage 2>/dev/null; then
    APPIMAGE_OK=1
else
    APPIMAGE_OK=0
    echo "warning: make appimage failed; release will ship tarball only" >&2
fi

NOTES="$(cat <<EOF
## powergov ${VERSION}

### Usuario final (recomendado): AppImage precompilado

1. Descargá \`PowerGov-${VERSION}-x86_64.AppImage\`
2. \`chmod +x PowerGov-${VERSION}-x86_64.AppImage && ./PowerGov-${VERSION}-x86_64.AppImage\`
3. En el **primer arranque**, se crea acceso en el menú y en el escritorio (\`\$XDG_DESKTOP_DIR\`).
4. La UI detecta el idioma del sistema (es → español; resto → inglés) y pregunta si instalar el servicio en segundo plano.
5. Si aceptás, se pide contraseña de administrador **una vez** y queda el backend (systemd + daemon) listo.
6. **Desinstalar:** botón al pie de la ventana → confirmación → contraseña de admin.

**No hace falta compilar nada** para usar el AppImage.

### Cambios en ${VERSION} — TLP-parity reactivo

#### Daemon y política
- **device_aggression** (0–3): política reactiva según carga CPU, modo usuario y SOC (no perfiles AC/BAT fijos como TLP).
- Nuevas features: **disk_pm** (hdparm APM, SATA ALPM, NVMe), **pcie_aspm**, **bluetooth_pm**.
- **peripheral_pm** refactorizado: WiFi + audio; SATA migrado a disk_pm.
- Convivencia **TLP**: si TLP está activo, powergov difiere runtime_pm, peripheral_pm, disk_pm, pcie_aspm y bluetooth_pm.
- Con TLP desactivado, powergov es **alternativa viable** con el stack device PM completo.

#### UI GTK
- Pestaña **Acerca de**: versión (misma plantilla que \`pack -v\`), licencia, enlace al repo, versión del servicio.
- \`powergov-ui -v\` / \`powergov -v\`: salida unificada con metadatos GPLv3.
- Features Dev: checkboxes disk_pm, pcie_aspm, bluetooth_pm (bloqueados si TLP activo).

#### Herramientas
- \`scripts/bench-battery-session.sh\`: benchmark comparativo powergov vs TLP en batería (host).

#### Empaquetado
- AppImage: instalación estable en \`~/.local/share/powergov/PowerGov.AppImage\`.

### Instalación desde código fuente (tarball)

\`\`\`bash
tar -xzf powergov-${VERSION}.tar.gz
cd powergov-${VERSION}
./install-powergov.sh
\`\`\`

Requiere: \`build-essential pkg-config libgtk-3-dev zenity\`.

### Documentación

- README, \`Documentation/\` y mandocs actualizados (TLP, device_aggression, nuevas features).
- \`man powergov\` / \`man 8 powergov\`.

### Notas

- UI GTK con idioma automático (LANG/LC_MESSAGES) + botón EN/ES.
- \`make appimage\` es para quien publica releases (~2 min); los usuarios descargan el binario empaquetado.
- WWAN, GPU dGPU y parseo de \`tlp.conf\` siguen fuera de alcance (backlog).
EOF
)"

RELEASE_ASSETS=("$TAR")
if [[ "${APPIMAGE_OK}" -eq 1 && -f "${APPIMAGE}" ]]; then
    RELEASE_ASSETS+=("${APPIMAGE}")
fi

if "$GH" auth status >/dev/null 2>&1; then
    if "$GH" release view "$TAG" >/dev/null 2>&1; then
        "$GH" release upload "$TAG" "${RELEASE_ASSETS[@]}" --clobber
        echo "uploaded assets to existing release $TAG"
    else
        "$GH" release create "$TAG" "${RELEASE_ASSETS[@]}" \
            --title "powergov ${VERSION}" \
            --notes "$NOTES"
        echo "created release $TAG"
    fi
    exit 0
fi

echo "gh not authenticated. Run once: gh auth login" >&2
echo "Then: make release" >&2
exit 1
