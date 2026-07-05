# Módulos e interfaces sysfs

> **Última verificación:** 2026-07-05  
> **Fuente de verdad:** `cpu/*.c`, `platform/platform_profile.c`, `devices/runtime_pm.c`, `power/power_supply.c`, `core/sysfs.c`

Cada módulo implementa **apply** (escribir si difiere), **verify** (releer y comparar) y registra métricas `apply_ok|fail|skip` y `verify_ok|fail`.

## power_supply

| Lectura | Ruta |
|---------|------|
| Tipo batería | `/sys/class/power_supply/*/type` → `Battery` |
| SOC | `.../capacity` |
| Fuente | `.../status` → `Discharging`, `Charging`, `Full`, `Not charging` |

Código: `power/power_supply.c`.

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
| WiFi | `iw dev <iface> set power_save on\|off` | Requiere `iw` en PATH; interfaces wireless vía `/sys/class/net/*/wireless` |
| SATA | Escritura `link_power_management_policy` en `/sys/class/scsi_host/*/link_power_management_policy` o dispositivo block asociado | Restaura valor guardado al desactivar |
| Audio | `power_save` en `/sys/class/sound/card*/device/.../power/control` o codec HDA | Best-effort según hardware |

Código: `devices/peripheral_pm.c`. Métricas: `apply_ok|fail|skip`, `verify_ok|fail` como el resto de módulos.

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
