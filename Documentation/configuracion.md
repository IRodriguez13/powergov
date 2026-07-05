# Configuración

> **Última verificación:** 2026-07-05  
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
| `LOW_BATTERY` | 5–50 | 15 | SOC % para perfil de supervivencia (cap 60%) |
| `PERIPHERAL_WIFI` | 0, 1 | 1 | Ahorro WiFi (`iw set power_save`) en modo custom |
| `PERIPHERAL_SATA` | 0, 1 | 1 | Link power management en dispositivos SATA |
| `PERIPHERAL_AUDIO` | 0, 1 | 1 | `power_save` en codecs HDA |
| `CUSTOM_ALLOW_PERFORMANCE` | 0, 1 | 0 | Modo custom: permitir governor performance en batería |
| `CUSTOM_RUNTIME_AGGRESSIVE` | 0, 1 | 1 | Modo custom: runtime PM agresivo |

### Features válidas en `FEATURES`

| Nombre en conf | ID | Módulo |
|----------------|-----|--------|
| `governor` | CPU governor | `cpu/governor.c` |
| `epp` | Energy Performance Preference | `cpu/epp.c` |
| `freq_cap` | Techo scaling_max_freq | `cpu/freq_cap.c` |
| `turbo` | boost / no_turbo | `cpu/turbo.c` |
| `platform` | ACPI platform_profile | `platform/platform_profile.c` |
| `runtime_pm` | PCI/USB auto suspend | `devices/runtime_pm.c` |
| `peripheral_pm` | WiFi / SATA / audio on demand | `devices/peripheral_pm.c` |

## Modos de usuario (`USER_MODE`)

| Modo | Comportamiento en batería |
|------|---------------------------|
| **max-battery** | Máxima protección: sin performance, turbo off, EPP bajo, cap freq, runtime PM, platform low-power |
| **balanced** | Ahorro moderado; cap freq; performance bloqueado salvo override battery-safe |
| **performance** | Permite governor performance y turbo en batería (decisión explícita del usuario) |
| **custom** | Política definida por flags `CUSTOM_*` y `PERIPHERAL_*`; expuesto en UI Dev → Características |

Default del proyecto: **`max-battery`** — proteger autonomía salvo que el usuario elija lo contrario.

### Periféricos (modo custom, a demanda)

Con `USER_MODE=custom` y `peripheral_pm` en `FEATURES`, el daemon aplica ahorro por dispositivo según `PERIPHERAL_WIFI`, `PERIPHERAL_SATA`, `PERIPHERAL_AUDIO`. La UI GTK expone tres checkboxes (sincronizados con el daemon tras la primera lectura de tuning).

Si **TLP** está activo, `runtime_pm` y `peripheral_pm` no se aplican (detección en `platform/tlp_compat.c`).

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
