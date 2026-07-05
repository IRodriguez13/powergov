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
3. En el **primer arranque**, se crea acceso en el menú de aplicaciones y en el escritorio (Escritorio o Desktop según tu sistema).
4. La UI detecta el idioma del sistema (es → español; resto → inglés) y pregunta si instalar el servicio en segundo plano.
5. Si aceptás, se pide contraseña de administrador **una vez** y queda todo el backend (systemd + daemon) listo.
6. Si no aceptás, cualquier acción que requiera el servicio vuelve a preguntar.
7. **Desinstalar:** botón al pie de la ventana (servicio instalado) → confirmación → contraseña de admin.

**No hace falta compilar nada** para usar el AppImage.

### Cambios en ${VERSION}

- Desinstalación completa desde la UI (servicio, archivos del sistema y accesos directos).
- Modo inteligente más fluido con batería > 30 % (sin techo de frecuencia ni perfil low-power hasta reserva baja).

### Instalación desde código fuente (tarball)

\`\`\`bash
tar -xzf powergov-${VERSION}.tar.gz
cd powergov-${VERSION}
./install-powergov.sh
\`\`\`

Requiere: \`build-essential pkg-config libgtk-3-dev zenity\`.

### Notas

- UI GTK con idioma automático (LANG/LC_MESSAGES del sistema) + botón EN/ES.
- \`make appimage\` es solo para quien publica releases (tarda ~2 min); los usuarios descargan el binario ya empaquetado.
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
