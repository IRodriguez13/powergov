# Configuración

> **Última verificación:** 2026-07-04  
> **Fuente de verdad:** `config/config.c`, `config/powergov.conf`, `main.c`, `include/powergov/types.h`

## Archivo persistente

Ruta: **`/etc/powergov.conf`** (`POWERGOV_CONF_PATH`).

Plantilla en repo: `config/powergov.conf`.

| Clave | Valores | Default | Descripción |
|-------|---------|---------|-------------|
| `USER_MODE` | `max-battery`, `balanced`, `performance` | `max-battery` | Agresividad de ahorro vs rendimiento |
| `FEATURES` | lista separada por comas | todos | Subsistemas activos |
| `BATTERY_SAFE_THRESHOLD` | 0–100 | 0 | 0 = off; >0 bloquea governor performance bajo umbral |
| `LOG_LEVEL` | 0–4 | 3 (INFO) | Ver `powergov_log_level_t` en types.h |
| `DEV_LOG` | 0, 1 | 1 | Log dev en `/var/log/powergov/powergov.log` |
| `THRESHOLD_LOW` | 0.0–1.0 | 0.25 | Umbral POWERSAVE ↔ BALANCED |
| `THRESHOLD_MID` | 0.0–1.0 | 0.60 | Salida de PERFORMANCE |
| `THRESHOLD_HIGH` | 0.0–1.0 | 0.75 | Entrada a PERFORMANCE |
| `FREQ_CAP_BATTERY` | 50–100 | 80 | % de cpuinfo_max_freq en batería |
| `LOW_BATTERY` | 5–50 | 20 | SOC % para perfil de supervivencia (cap 60%) |

### Features válidas en `FEATURES`

| Nombre en conf | ID | Módulo |
|----------------|-----|--------|
| `governor` | CPU governor | `cpu/governor.c` |
| `epp` | Energy Performance Preference | `cpu/epp.c` |
| `freq_cap` | Techo scaling_max_freq | `cpu/freq_cap.c` |
| `turbo` | boost / no_turbo | `cpu/turbo.c` |
| `platform` | ACPI platform_profile | `platform/platform_profile.c` |
| `runtime_pm` | PCI/USB auto suspend | `devices/runtime_pm.c` |

## Modos de usuario (`USER_MODE`)

| Modo | Comportamiento en batería |
|------|---------------------------|
| **max-battery** | Máxima protección: sin performance, turbo off, EPP bajo, cap freq, runtime PM, platform low-power |
| **balanced** | Ahorro moderado; cap freq; performance bloqueado salvo override battery-safe |
| **performance** | Permite governor performance y turbo en batería (decisión explícita del usuario) |

Default del proyecto: **`max-battery`** — proteger autonomía salvo que el usuario elija lo contrario.

## CLI

### Usuario

```bash
powergov on | off | status
powergov mode max-battery|balanced|performance
powergov feature <nombre> on|off
powergov features list
powergov --battery-safe <0-100>
powergov -v | --version
powergov --help
```

### Desarrollo (no orientado a usuario final)

```bash
powergov dev-log [--tail N]    # default N=80
powergov dev-metrics
```

## Cambios en caliente

Socket Unix: **`/run/powergov/powergov.sock`**

Protocolo extendido (`powergov_socket_msg_t` en `types.h`):

| cmd | Efecto |
|-----|--------|
| `SET_BATTERY_THRESHOLD` | battery-safe |
| `SET_USER_MODE` | modo usuario |
| `SET_FEATURE` | toggle feature (value=id, value2=on) |
| `QUERY_FULL_CONFIG` | respuesta `powergov_socket_status_t` |

Legacy: enviar solo un `int` threshold sigue funcionando.

Si el daemon no corre, `mode` / `feature` / `--battery-safe` persisten en `/etc/powergov.conf` para el próximo arranque.

## Ejemplos

```bash
# Máxima autonomía (default)
sudo powergov mode max-battery

# Rendimiento aunque quede poca batería
sudo powergov mode performance

# Solo governor + EPP, sin tocar dispositivos
sudo powergov feature runtime_pm off
sudo powergov feature platform off

# Bloquear performance bajo 30% (independiente del modo)
sudo powergov --battery-safe 30
sudo powergov --battery-safe 0    # desactivar
```
