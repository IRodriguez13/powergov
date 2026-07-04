# Métricas y logging (desarrollo)

> **Última verificación:** 2026-07-04  
> **Fuente de verdad:** `metrics/metrics.c`, `log/log.c`, `main.c` (comandos `dev-*`)

## Objetivo

Garantizar que powergov **solo toca lo que debe** y que las escrituras sysfs **coinciden** con lo pedido. Orientado a desarrollo y validación en hardware real; no es UI de usuario final.

## Métricas en runtime

Archivo escrito cada tick del daemon:

**`/run/powergov/metrics`**

Formato clave=valor (texto plano), por ejemplo:

```text
loop_ticks=42
state_transitions=3
governor_apply_ok=2
governor_apply_skip=40
governor_verify_ok=2
governor_verify_fail=0
epp_apply_ok=1
...
rapl_available=1
rapl_energy_uj=12345678901
rapl_watts_est=8.450
```

### Contadores por feature

| Contador | Significado |
|----------|-------------|
| `*_apply_ok` | Escritura sysfs exitosa |
| `*_apply_fail` | Escritura fallida (permiso, path inexistente) |
| `*_apply_skip` | Valor ya correcto, no se escribió |
| `*_verify_ok` | Relectura coincide con objetivo |
| `*_verify_fail` | Relectura difiere (investigar driver/ppd/TLP) |

Features: `governor`, `epp`, `freq_cap`, `turbo`, `platform`, `runtime_pm`.

### Comando CLI

```bash
powergov dev-metrics
```

Carga `/run/powergov/metrics` si el daemon está activo; si no, muestra contadores locales vacíos.

## Logging dev

Ruta: **`/var/log/powergov/powergov.log`**

Habilitado con `DEV_LOG=1` en conf (default). Nivel con `LOG_LEVEL` (0=off … 4=debug).

Formato de línea:

```text
YYYY-MM-DD HH:MM:SS [LEVEL] module: message
```

Módulos típicos: `loop`, `governor`, `epp`, `freq_cap`, `turbo`, `platform`, `runtime_pm`, `cpu_policy`, `power`.

### Comando CLI

```bash
sudo powergov dev-log --tail 100
sudo powergov dev-log -f                  # en vivo (Ctrl+C)
sudo powergov dev-log --tail 50 -f        # últimas 50 + en vivo
```

Muestra las últimas N líneas (default 80).

## Criterios de validación en prueba de hardware

1. **`verify_fail` = 0** en features habilitadas tras unos minutos de operación normal.
2. Tras `powergov mode max-battery` en batería: log muestra EPP/turbo/cap coherentes; `status` refleja turbo off si el hardware lo soporta.
3. Tras `powergov off`: freq cap restaurado (`scaling_max_freq` vuelve al valor previo).
4. Si RAPL disponible: `rapl_watts_est` baja al pasar de `performance` a `max-battery` bajo carga moderada comparable.

## Qué no miden las métricas actuales

- Consumo total del sistema (solo estimación RAPL package si existe)
- Horas de autonomía restantes
- Ahorro acumulado en Wh (roadmap posible: integración con `energy_now` de power_supply)
