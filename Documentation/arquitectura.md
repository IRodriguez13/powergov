# Arquitectura

> **Última verificación:** 2026-07-04  
> **Fuente de verdad:** `core/loop.c`, `core/state_machine.c`, `power/profile.c`, `cpu/policy.c`, `Makefile`

## Resumen

powergov es un daemon en C que cada **2 segundos** evalúa carga CPU y contexto de alimentación, calcula una **política efectiva** y aplica cambios en sysfs **solo cuando la política cambia** (idempotente). Al recibir SIGTERM restaura freq cap y runtime PM.

No modifica el kernel: consume interfaces estándar de Linux (`cpufreq`, EPP, `powercap`, ACPI platform profile, runtime PM).

## Árbol de módulos

```
include/powergov/types.h     Tipos, config, constantes
config/                      Carga/guardado /etc/powergov.conf
core/
  sysfs.c                    Lectura/escritura sysfs (buffers fijos)
  state_machine.c            POWERSAVE ↔ BALANCED ↔ PERFORMANCE
  loop.c                     Orquestación del daemon + socket
power/
  power_supply.c             AC/batería, SOC (/sys/class/power_supply)
  profile.c                  Modo usuario + fuente → política efectiva
cpu/
  cpu_load.c                 Carga vía /proc/stat (ventana 200 ms)
  governor.c, epp.c, freq_cap.c, turbo.c
  policy.c                   Aplica subsistemas CPU habilitados
platform/platform_profile.c  /sys/firmware/acpi/platform_profile
devices/runtime_pm.c         PCI/USB power/control → auto en batería
metrics/metrics.c            Contadores apply/verify + RAPL
log/log.c                    Log dev → /var/log/powergov/powergov.log
main.c                       CLI
```

## Flujo por tick (determinista)

```mermaid
flowchart TD
    A[Tick 2s] --> B[Socket: config en caliente?]
    B --> C[get_cpu_usage 200ms]
    C --> D[power_supply_poll]
    D --> E[battery_safe / battery_limited]
    E --> F[state_machine_step + histéresis 3 muestras]
    F --> G[profile_compute]
    G --> H{Política cambió?}
    H -->|sí| I[cpu_policy + platform + runtime_pm]
    H -->|no| J[skip apply]
    I --> K[metrics + RAPL sample]
    J --> K
    K --> A
```

## Máquina de estados (governor lógico)

Umbrales por defecto (`include/powergov/types.h`):

| Transición | Condición |
|------------|-----------|
| POWERSAVE → BALANCED | carga > 25% durante 3 ticks |
| BALANCED → PERFORMANCE | carga > 75%, AC o modo performance, no battery_limited |
| PERFORMANCE → BALANCED | carga < 60%, o battery_limited, o modo max-battery en batería |
| BALANCED → POWERSAVE | carga < 25% durante 3 ticks |

**Histéresis:** `POWERGOV_HYSTERESIS_SAMPLES` = 3 ticks consecutivos antes de subir/bajar de estado (evita thrashing).

**battery_limited:** activo si `BATTERY_SAFE_THRESHOLD` > 0 y SOC ≤ umbral; bloquea estado PERFORMANCE.

## Política efectiva (`power/profile.c`)

La máquina de estados elige el **estado lógico**; el perfil de usuario y la fuente de alimentación mapean a valores concretos:

| Campo | Ejemplo en batería + max-battery |
|-------|----------------------------------|
| governor | powersave / schedutil |
| epp | balance_power / power |
| turbo_on | 0 |
| freq_cap_pct | 80 (60 si SOC ≤ LOW_BATTERY) |
| platform_profile | low-power |
| runtime_pm_aggressive | 1 |
| allow_performance | 0 |

Modo `performance` del usuario fuerza `allow_performance = 1` incluso en batería.

## Conflictos con otros daemons

- Si existe **power-profiles-daemon** (ppd), `platform_profile` se deshabilita al arrancar (`platform_ppd_active()` en `platform/platform_profile.c`).
- TLP, cpupower u otros scripts que escriban los mismos nodos sysfs pueden competir; conviene que powergov sea el dueño de CPU o desactivar features solapadas.

## Restore al apagar

En `core/loop.c` al salir del loop:

1. `cpu_policy_restore()` — restaura `scaling_max_freq` guardado
2. `runtime_pm_restore()` — revierte PCI/USB tocados
3. Cierre de socket y pidfile

## Rendimiento del propio daemon

- Sin `malloc` en el hot path del loop principal
- Apply condicionado a diff de política
- Escritura sysfs skip si el valor ya coincide (`apply_skip` en métricas)
- `Nice=10` en `service/powergov.service` (baja prioridad CPU)
