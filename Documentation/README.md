# powergov — Documentación

> **Última verificación:** 2026-07-04  
> **Fuente de verdad:** `include/powergov/types.h`, `core/loop.c`, `config/config.c`, `main.c`, `doc/powergov.1`, `doc/powergov.8`

Índice de documentación operativa y de diseño del daemon **powergov** (gestor modular de energía para portátiles Linux, userspace + sysfs).

## Documentos

| Documento | Contenido |
|-----------|-----------|
| [arquitectura.md](arquitectura.md) | Módulos, flujo del daemon, máquina de estados, determinismo |
| [configuracion.md](configuracion.md) | `/etc/powergov.conf`, modos de usuario, features, CLI |
| [operacion.md](operacion.md) | Build, install, systemd, socket en caliente |
| [modulos-sysfs.md](modulos-sysfs.md) | Subsistemas (CPU, plataforma, runtime PM) e interfaces kernel |
| [metricas-logging-dev.md](metricas-logging-dev.md) | Métricas apply/verify, RAPL, `dev-log`, `dev-metrics` |
| [impacto-bateria-teorico.md](impacto-bateria-teorico.md) | Análisis estático de autonomía esperada |
| [../ui/README.md](../ui/README.md) | UI desktop (usuario + modo Dev) |

## Referencia rápida

```bash
sudo make install-service          # binario + unit + conf
sudo powergov mode max-battery     # máxima protección (default)
sudo powergov status
sudo powergov feature turbo off    # desactivar un subsistema
powergov dev-metrics               # contadores (desarrollo)
sudo powergov dev-log -f           # log en vivo

# UI web local
make ui                            # navegador
make ui-desktop                    # ventana desktop (pywebview)
```

## Mandocs

- Usuario: `doc/powergov.1` → `man powergov`
- Servicio: `doc/powergov.8` → `man 8 powergov`

## Fuera de alcance (v1.7.x)

- Backlight / brillo de pantalla
- WiFi power save (`iw`/nl80211)
- GPU discreta (NVIDIA Optimus, etc.)
- UI gráfica (planeada a futuro)
