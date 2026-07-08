# Memoria consciente (PSI / swap / iowait)

> **Última verificación:** 2026-07-08  
> **Fuente de verdad:** `power/memory_pressure.c`, `cpu/cpu_load.c`, `core/state_machine.c`, `power/profile.c`, `include/powergov/types.h`

Desde **v1.14**, powergov evita **apretar más** la política cuando el kernel ya está bajo presión de memoria o I/O — sin usar el porcentaje de RAM ocupada como señal (demasiados falsos positivos).

## Principio

| Acción | Cuándo |
|--------|--------|
| **No bajar** a POWERSAVE | PSI/swap indican estrés |
| **Subir** desde POWERSAVE a BALANCED | Ya en powersave y aparece presión |
| **Bloquear** context boost (tapa/idle) | Presión memoria activa |
| **Limitar** `device_aggression` | stressed ≤2, severe ≤1, menos runtime PM |
| **Contar iowait como carga** | Swap/thrashing ya no parece «CPU idle» |

No sube a `performance` por presión memoria: solo **suelta el freno** para no empeorar latencia al cambiar de app.

## Señales

| Fuente | Ruta / mecanismo | Uso |
|--------|------------------|-----|
| PSI some | `/proc/pressure/memory` → `some avg10` | Presión leve/media |
| PSI full | `full avg10` | Presión severa |
| Swap | `/proc/vmstat` → delta `pswpin`+`pswpout` por tick (~2 s) | Thrashing |
| I/O wait | `/proc/stat` — **ya no** se cuenta como idle en `get_cpu_usage()` | Carga efectiva para umbrales |

## Clasificación (defaults)

| Nivel | Condición (cualquiera) |
|-------|------------------------|
| **stressed** | `psi_some_avg10` ≥ 10 **o** swap ≥ 128 páginas/tick |
| **severe** | `psi_full_avg10` ≥ 2 **o** swap ≥ 512 páginas/tick |

`severe` implica `stressed`.

## Efecto en política

### Máquina de estados (`state_machine.c`)

- En **POWERSAVE** con presión → transición a **BALANCED**.
- Desde **BALANCED** no se baja a POWERSAVE mientras haya presión.

### Perfil (`profile.c`)

- `apply_context_aggression_boost`: retorno inmediato si `memory_stressed`.
- `apply_memory_pressure_limits`: cap de agresividad device y runtime PM.

## Configuración (`/etc/powergov.conf`)

| Clave | Default | Descripción |
|-------|---------|-------------|
| `MEMORY_AWARE` | 1 | Activar lógica memory-aware |
| `MEMORY_PSI_SOME_PCT` | 10 | Umbral PSI some (avg10, %) |
| `MEMORY_PSI_FULL_PCT` | 2 | Umbral PSI full (avg10, %) |
| `MEMORY_SWAP_PAGES_TICK` | 128 | Páginas swap/tick → stressed |
| `MEMORY_SWAP_PAGES_SEVERE` | 512 | Páginas swap/tick → severe |

`MEMORY_AWARE=0` restaura comportamiento pre-v1.14 (solo CPU load clásico, iowait como idle).

## UI

**Información → Sistema** muestra línea de presión de memoria (sin / con presión / presión alta).

## Requisitos

- Kernel con **PSI** (`CONFIG_PSI=y`) recomendado; sin PSI solo swap cuenta.
- No requiere cgroup ni systemd-oomd.

## Tradeoff

- **A favor:** mejor respuesta con muchas apps + swap; no castiga con device PM agresivo bajo estrés.
- **En contra:** algo menos ahorro en batería mientras hay presión leve (p. ej. compilación en background con swap moderado).

Ajustá umbrales si en tu hardware el default es demasiado sensible.
