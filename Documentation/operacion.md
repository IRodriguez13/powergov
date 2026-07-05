# Operación

> **Última verificación:** 2026-07-05  
> **Fuente de verdad:** `Makefile`, `service/powergov.service`, `main.c`, `core/loop.c`

## Requisitos

- Linux con `CONFIG_CPU_FREQ`
- Acceso root para escribir sysfs (daemon y cambios de governor)
- Opcional pero recomendado en portátil: batería en `/sys/class/power_supply`

## Build e instalación

```bash
make                    # genera ./powergov
sudo make install       # /usr/local/bin/powergov + man + completions
sudo make install-service   # + /etc/powergov.conf + systemd unit + enable + start
```

Desinstalar:

```bash
sudo make uninstall         # binario + docs
sudo make uninstall-service # unit systemd
```

## Servicio systemd

Unit: **`/etc/systemd/system/powergov.service`**

| Directiva | Valor | Notas |
|-----------|-------|-------|
| ExecStart | `/usr/local/bin/powergov on` | Foreground bajo systemd |
| Nice | 10 | Baja prioridad del daemon |
| RuntimeDirectory | powergov | `/run/powergov` |
| LogsDirectory | powergov | `/var/log/powergov` |
| PIDFile | `/run/powergov/powergov.pid` | |

Comandos habituales:

```bash
sudo systemctl start|stop|restart|status powergov
sudo systemctl enable|disable powergov
make service-status
```

## Arranque manual (debug)

```bash
sudo ./powergov on
# Ctrl+C o en otra terminal:
sudo powergov off
```

## Verificación rápida post-instalación

```bash
powergov status
# Esperado: State, CPU load, Power (AC/battery), Battery %, EPP, Turbo, User mode, Features

sudo powergov on &
sleep 6
powergov dev-metrics
sudo powergov dev-log --tail 20
sudo powergov off
```

En `dev-metrics`, comprobar que los subsistemas habilitados muestran `verify_ok` creciendo tras cambios de carga o modo.

## Archivos en runtime

| Ruta | Uso |
|------|-----|
| `/run/powergov/powergov.pid` | PID del daemon |
| `/run/powergov/powergov.sock` | Control en caliente |
| `/run/powergov/metrics` | Snapshot de métricas (daemon) |
| `/var/log/powergov/powergov.log` | Log dev (si DEV_LOG=1) |
| `/etc/powergov.conf` | Config persistente |

## Integración con el escritorio

- **GNOME/KDE** suelen usar `power-profiles-daemon`. powergov detecta ppd y no toca `platform_profile` para evitar conflictos.
- Si usás ppd para el perfil de plataforma, podés dejar `feature platform off` y confiar en powergov para CPU/EPP/turbo.
- **TLP:** si el servicio o stack TLP está activo (`platform/tlp_compat.c`), powergov **omite** `runtime_pm` y `peripheral_pm`. Desactivá uno de los dos para esas áreas o desactivá TLP si querés que powergov gestione dispositivos.

## AppImage y actualizaciones

- Build: `make appimage` → `dist/PowerGov-<version>-x86_64.AppImage`
- Instalación usuario: `~/.local/share/powergov/PowerGov.AppImage` (sobrescribe; sin copias versionadas duplicadas en Descargas)
- Atajos: `$XDG_DESKTOP_DIR` vía `scripts/powergov-xdg-paths.sh`
- Release: `make release` (tarball + AppImage a GitHub Releases con `gh`)

## Permisos de log

`LogsDirectory=powergov` crea `/var/log/powergov` con dueño del servicio. El log dev requiere que el daemon pueda escribir ahí; `dev-log` puede necesitar `sudo` para leer según permisos locales.
