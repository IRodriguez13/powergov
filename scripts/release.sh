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

NOTES="$(cat <<EOF
## powergov ${VERSION}

- UI GTK: pestañas Perfil/Diagnóstico, log y métricas sin reset de scroll
- Modo dev: botón «Volver a modo usuario»; acceso directo en el escritorio al instalar
- i18n EN/ES (inglés por defecto) con botón EN/ES en la barra de título
- Icono, \`.desktop\`, Polkit y \`libpowergov.so\`

### Instalación

\`\`\`bash
tar -xzf powergov-${VERSION}.tar.gz
cd powergov-${VERSION}
make
sudo make install-service
make powergov-ui
sudo make install-ui install-ui-policy install-ui-helper
\`\`\`

El acceso directo en el escritorio se crea automáticamente (\`sudo make install-ui\` usa \`\$SUDO_USER\`).
EOF
)"

if "$GH" auth status >/dev/null 2>&1; then
    if "$GH" release view "$TAG" >/dev/null 2>&1; then
        "$GH" release upload "$TAG" "$TAR" --clobber
        echo "uploaded asset to existing release $TAG"
    else
        "$GH" release create "$TAG" "$TAR" \
            --title "powergov ${VERSION}" \
            --notes "$NOTES"
        echo "created release $TAG"
    fi
    exit 0
fi

echo "gh not authenticated. Run once: gh auth login" >&2
echo "Then: make release" >&2
exit 1
