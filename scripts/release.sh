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

- **Instalador de doble clic**: extraé el tarball y abrí \`Install-PowerGov.desktop\` (o ejecutá \`./install-powergov.sh\`).
- UI GTK EN/ES, acceso directo en el escritorio, log/métricas sin saltos de scroll.
- Servicio systemd + \`libpowergov.so\` + Polkit para diagnóstico.

### Instalación rápida (usuario)

\`\`\`bash
tar -xzf powergov-${VERSION}.tar.gz
cd powergov-${VERSION}
# Doble clic en «Install PowerGov» / «Instalar PowerGov»
# GNOME: clic derecho → «Permitir lanzar» la primera vez
./install-powergov.sh
\`\`\`

Requiere: \`build-essential\`, \`pkg-config\`, \`libgtk-3-dev\`, \`zenity\` (Ubuntu/Debian: \`sudo apt install build-essential pkg-config libgtk-3-dev zenity\`).

### Instalación manual

\`\`\`bash
make && make powergov-ui
sudo make install-service
sudo make install-ui install-ui-policy install-ui-helper
\`\`\`
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
