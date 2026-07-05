#!/usr/bin/env bash
# Populate a directory with files needed by install-service-resident.sh.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:?output staging directory}"

cd "$ROOT"
mkdir -p "$OUT"

make -s powergov libpowergov.so

install -m 755 "$ROOT/powergov" "$OUT/powergov"
install -m 755 "$ROOT/libpowergov.so" "$OUT/libpowergov.so"
install -m 644 "$ROOT/config/powergov.conf" "$OUT/powergov.conf"
install -m 644 "$ROOT/service/powergov.service" "$OUT/powergov.service"
install -m 644 "$ROOT/data/org.powergov.policy" "$OUT/org.powergov.policy"
install -m 755 "$ROOT/scripts/powergov-dev-auth" "$OUT/dev-auth"
install -m 755 "$ROOT/scripts/powergov-uninstall.sh" "$OUT/powergov-uninstall.sh"
install -m 755 "$ROOT/scripts/remove-appimage-user-files.sh" \
    "$OUT/remove-appimage-user-files.sh"
