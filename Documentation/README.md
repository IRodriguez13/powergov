# powergov — Documentación

> **Última verificación:** 2026-07-05  
> **Fuente de verdad:** `include/powergov/types.h`, `core/loop.c`, `config/config.c`, `devices/disk_pm.c`, `devices/pcie_aspm.c`, `devices/bluetooth_pm.c`, `devices/peripheral_pm.c`, `power/profile.c`, `platform/tlp_compat.c`, `doc/powergov.1`, `doc/powergov.8`

Índice de documentación operativa y de diseño del daemon **powergov** (gestor modular de energía para portátiles Linux, userspace + sysfs).

## Documentos

| Documento | Contenido |
|-----------|-----------|
| [arquitectura.md](arquitectura.md) | Módulos, flujo del daemon, máquina de estados, determinismo |
| [configuracion.md](configuracion.md) | `/etc/powergov.conf`, modos de usuario, features, periféricos, CLI |
| [operacion.md](operacion.md) | Build, install, systemd, socket en caliente, TLP/ppd |
| [modulos-sysfs.md](modulos-sysfs.md) | Subsistemas (CPU, plataforma, runtime PM, disk_pm, ASPM, BT, peripheral PM) |
| [metricas-logging-dev.md](metricas-logging-dev.md) | Métricas apply/verify, RAPL, `dev-log`, `dev-metrics` |
| [impacto-bateria-teorico.md](impacto-bateria-teorico.md) | Análisis estático de autonomía esperada |
| [../ui/README.md](../ui/README.md) | UI GTK (usuario + modo Dev + bandeja) |
| [../README.md](../README.md) | Visión general del proyecto (inglés) |

## Referencia rápida

```bash
sudo make install-service          # binario + unit + conf
sudo powergov mode max-battery     # máxima protección (default)
powergov status
sudo powergov feature peripheral_pm off   # desactivar ahorro periféricos
powergov dev-metrics               # contadores (desarrollo)
sudo powergov dev-log -f           # log en vivo

# UI GTK
make powergov-ui && ./powergov-ui
make appimage                      # release AppImage
```

## Mandocs

- Usuario: `doc/powergov.1` → `man powergov`
- Servicio: `doc/powergov.8` → `man 8 powergov`

Tras `sudo make install`, las páginas quedan en `/usr/local/share/man/man1` y `man8`.

## Fuera de alcance (v1.10)

- Backlight / brillo de pantalla
- GPU discreta (NVIDIA Optimus, etc.)
- Wayland compositor / gestión de ventanas

**Incluido desde v1.11:** disk APM/ALPM/NVMe (`disk_pm`), PCIe ASPM, Bluetooth PM, política reactiva `device_aggression` vs reglas fijas TLP.
