# Módulos e interfaces sysfs

> **Última verificación:** 2026-07-08  
> **Fuente de verdad:** `cpu/*.c`, `platform/platform_profile.c`, `devices/*.c`, `power/profile.c`, `power/lid_state.c`, `power/session_idle.c`, `core/sysfs.c`

Cada módulo implementa **apply** (escribir si difiere), **verify** (releer y comparar) y registra métricas `apply_ok|fail|skip` y `verify_ok|fail`.

## power_supply

| Lectura | Ruta |
|---------|------|
| Tipo batería | `/sys/class/power_supply/*/type` → `Battery` |
| SOC | `.../capacity` |
| Fuente | `.../status` → `Discharging`, `Charging`, `Full`, `Not charging` |

Código: `power/power_supply.c` (integra `lid_state` y `session_idle` en cada poll).

## power/lid_state

| Origen | Ruta |
|--------|------|
| ACPI proc | `/proc/acpi/button/lid/*/state` |
| ACPI sysfs | `/sys/bus/acpi/devices/LID*/state` |

Salida: `lid_closed` 0 = abierta, 1 = cerrada, -1 = desconocido.

## power/session_idle

| Mecanismo | Comando / API |
|-----------|----------------|
| Primario | `busctl` → `org.freedesktop.login1.Manager.IdleHint` |
| Fallback | `loginctl show-session self -p IdleHint` |

Salida: `session_idle` 0 = activa, 1 = idle, -1 = no disponible.

## cpu/governor

| Operación | Ruta |
|-----------|------|
| Leer/escribir | `/sys/devices/system/cpu/cpuN/cpufreq/scaling_governor` |

Valores usados: `powersave`, `schedutil`, `performance`.

## cpu/epp

| Operación | Ruta |
|-----------|------|
| Leer/escribir | `/sys/devices/system/cpu/cpu0/cpufreq/energy_performance_preference` |

Valores usados: `power`, `balance_power`, `balance_performance`, `performance`, `default`.

Disponible en Intel/AMD recientes con driver adecuado; si no existe, el módulo no aplica (fail silencioso en métricas).

## cpu/freq_cap

| Operación | Ruta |
|-----------|------|
| Máximo hardware | `.../cpuinfo_max_freq` |
| Techo activo | `.../scaling_max_freq` |

Guarda el `scaling_max_freq` original antes del primer cap; `cpu_freq_cap_restore()` lo restaura al shutdown.

## cpu/turbo

| Driver | Ruta | Semántica |
|--------|------|-----------|
| Genérico | `/sys/devices/system/cpu/cpufreq/boost` | 1 = turbo on |
| Intel pstate | `/sys/devices/system/cpu/intel_pstate/no_turbo` | 0 = turbo on |

Prioridad: path genérico si existe, si no Intel.

## platform/platform_profile

| Operación | Ruta |
|-----------|------|
| Perfil actual | `/sys/firmware/acpi/platform_profile` |
| Opciones | `/sys/firmware/acpi/platform_profile_choices` |

Valores usados: `low-power`, `balanced`, `performance`.

**Skip automático** si `platform_ppd_active()` detecta power-profiles-daemon.

## devices/runtime_pm

| Bus | Ruta por dispositivo |
|-----|----------------------|
| PCI | `/sys/bus/pci/devices/*/power/control` |
| USB | `/sys/bus/usb/devices/*/power/control` |

En modo agresivo escribe `auto` donde el valor previo no era ya `auto`. Guarda path + valor anterior para restore.

## devices/peripheral_pm

Feature: `peripheral_pm`. Solo aplica en modo usuario **custom** con opciones individuales en config (`PERIPHERAL_*`). **No aplica si TLP está activo.**

| Subsistema | Mecanismo | Notas |
|------------|-----------|-------|
| WiFi | `iw dev <iface> set power_save on\|off` | Requiere `iw` en PATH |
| Audio | `power_save` en codecs HDA | Best-effort según hardware |

Código: `devices/peripheral_pm.c`. Métricas: `apply_ok|fail|skip`, `verify_ok|fail` como el resto de módulos.

## devices/disk_pm

Feature: `disk_pm`. Aplica según **`device_aggression`** (0–3) derivado de carga/modo/SOC en [`power/profile.c`](../power/profile.c). **No aplica si TLP está activo.**

| Subsistema | Mecanismo | Niveles |
|------------|-----------|---------|
| Disk APM | `hdparm -B` en `/dev/sd*` (excluye USB) | 1→192, 2→128, 3→127 |
| SATA ALPM | `link_power_management_policy` en `scsi_host` | med / min_power |
| NVMe runtime | `.../device/power/control` → `auto` | nivel ≥2 |

Opción `PERIPHERAL_SATA` en conf controla la parte SATA. Restore al shutdown.

## devices/pcie_aspm

Feature: `pcie_aspm`. Escribe `/sys/module/pcie_aspm/parameters/policy`:

- agresión 0–1 → `default`
- agresión ≥2 en batería → `powersave`

## devices/bluetooth_pm

Feature: `bluetooth_pm`. Nivel ≥2 en batería: `power/control` → `auto` en HCI (`/sys/class/bluetooth/*/device/power/control`).

## Política reactiva (`device_aggression`)

| Nivel | Cuándo (batería) | Capas típicas |
|-------|------------------|---------------|
| 0 | AC o performance permitido | restore device PM |
| 1 | BALANCED | WiFi PS, ASPM default |
| 2 | POWERSAVE / max-battery | + disk APM, ASPM powersave, BT, runtime PM |
| 3 | SOC ≤ LOW_BATTERY **o context boost** | + APM agresivo, SATA min_power |

**Context boost (v1.13):** con `LID_AGGRESSIVE` / `DISPLAY_AGGRESSIVE` y carga baja, `apply_context_aggression_boost()` fuerza nivel 3 aunque el governor esté en BALANCED. Ver [contexto-reactivo.md](contexto-reactivo.md).

TLP activo → powergov **no aplica** runtime_pm, peripheral_pm, disk_pm, pcie_aspm, bluetooth_pm (`platform/tlp_compat.c`).

## metrics / RAPL

| Lectura | Ruta |
|---------|------|
| Energía package | `/sys/class/powercap/intel-rapl:0/energy_uj` |

Estimación de vatios: delta energía / intervalo (~2 s). Solo informativo; no todos los sistemas exponen RAPL.

## core/sysfs

Helpers compartidos en `core/sysfs.c`:

- `sysfs_read_file`, `sysfs_write_file`, `sysfs_read_int`
- `sysfs_read_first_cpu_leaf`, `sysfs_write_all_cpu_leaf`

Política: buffers en stack, sin alloc en hot path de apply masivo.
