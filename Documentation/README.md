# powergov — Documentación

> **Última verificación:** 2026-07-08  
> **Fuente de verdad:** `include/powergov/types.h`, `core/loop.c`, `config/config.c`, `power/profile.c`, `power/session_idle.c`, `power/lid_state.c`, `devices/*.c`, `platform/tlp_compat.c`, `doc/powergov.1`, `doc/powergov.8`, `doc/powergov-ui.1`, `doc/powergov.conf.5`

Índice de documentación operativa y de diseño del daemon **powergov** (gestor modular de energía para portátiles Linux, userspace + sysfs).

## Documentos

| Documento | Contenido |
|-----------|-----------|
| [arquitectura.md](arquitectura.md) | Módulos, flujo del daemon, máquina de estados, poll adaptativo |
| [configuracion.md](configuracion.md) | `/etc/powergov.conf`, modos, features, tuning, contexto |
| [contexto-reactivo.md](contexto-reactivo.md) | Boost por tapa cerrada, sesión idle y carga baja (v1.13) |
| [memory-aware.md](memory-aware.md) | PSI, swap, iowait — no apretar bajo presión (v1.14) |
| [operacion.md](operacion.md) | Build, install, systemd, socket, TLP/ppd, AppImage |
| [modulos-sysfs.md](modulos-sysfs.md) | CPU, plataforma, runtime PM, disk_pm, ASPM, BT, lid, idle |
| [metricas-logging-dev.md](metricas-logging-dev.md) | Métricas apply/verify, RAPL, `dev-log`, `dev-metrics` |
| [impacto-bateria-teorico.md](impacto-bateria-teorico.md) | Análisis estático de autonomía esperada |
| [../ui/README.md](../ui/README.md) | UI GTK, bandeja, pending optimista, AppImage |
| [../README.md](../README.md) | Visión general del proyecto (inglés) |

## Referencia rápida

```bash
sudo make install-service          # binario + unit + conf + man
sudo powergov mode max-battery     # máxima protección (default)
powergov status
sudo powergov feature disk_pm off
powergov dev-metrics               # diagnóstico avanzado (CLI)

# UI GTK
make powergov-ui && ./powergov-ui
make appimage                      # release AppImage
```

## Mandocs

| Página | Comando |
|--------|---------|
| `doc/powergov.1` | `man powergov` |
| `doc/powergov-ui.1` | `man powergov-ui` |
| `doc/powergov.conf.5` | `man 5 powergov.conf` |
| `doc/powergov.8` | `man 8 powergov` |

Tras `sudo make install`, las páginas quedan en `/usr/local/share/man/man{1,5,8}`.

## Versiones recientes

| Versión | Hitos |
|---------|-------|
| **1.14** | Memory-aware: PSI/swap/iowait, suelo BALANCED, límites device PM |
| **1.13** | Context boost (tapa/idle), poll adaptativo AC/BAT, UI pending optimista |
| **1.12** | Modo custom en UI, tuning tapa/pantalla, pestaña Acerca de, sin modo Dev en GTK |
| **1.11** | disk_pm, pcie_aspm, bluetooth_pm, `device_aggression` reactivo vs TLP |

## Fuera de alcance

- Backlight / brillo de pantalla (solo hint logind para idle)
- GPU discreta (NVIDIA Optimus, etc.)
- Wayland compositor / gestión de ventanas
