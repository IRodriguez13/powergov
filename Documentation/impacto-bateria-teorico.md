# Impacto teórico en autonomía

> **Última verificación:** 2026-07-08  
> **Fuente de verdad:** módulos en `cpu/`, `power/profile.c`, `devices/*.c` — análisis estático, no benchmark publicado del proyecto

## Alcance de powergov

powergov actúa sobre **CPU (governor, EPP, turbo, techo de frecuencia)**, **perfil de plataforma ACPI** (si no hay ppd), **runtime PM PCI/USB**, **discos (APM/ALPM/NVMe)**, **PCIe ASPM**, **Bluetooth** y **periféricos WiFi/audio** (modo custom).

**No controla:** backlight directamente, GPU discreta, procesos userspace.

Por tanto el techo de mejora está acotado por la fracción del consumo total que representan esos subsistemas (típicamente **~25–50%** del total en uso mixto, variable por hardware).

## Ahorro por palanca (orden de magnitud)

Estimaciones estáticas vs Linux con `schedutil` pero sin política agresiva en batería:

| Palanca | Ahorro sobre consumo **total** del sistema |
|---------|---------------------------------------------|
| Evitar `performance` + state machine | 2–8% en uso mixto |
| EPP bajo en batería | 3–10% |
| Turbo off | 5–12% si hay picos frecuentes |
| Freq cap ~80% | 3–8% en carga media |
| platform_profile low-power | 0–8% (OEM) |
| runtime PM PCI/USB | 2–5% |
| disk APM / SATA ALPM / NVMe | 1–4% |
| PCIe ASPM powersave | 0–3% |
| Context boost (tapa + idle, v1.13) | 1–5% incremental en escenarios idle/cerrado |

Los efectos **no se suman linealmente** (solapamiento en la curva CPU).

## Escenarios agregados

| Uso | Ganancia teórica `max-battery` vs baseline permisivo |
|-----|------------------------------------------------------|
| Idle / lectura (tapa cerrada o sesión idle) | 5–12% |
| Navegación / ofimática | 8–18% |
| Desarrollo (picos, builds cortos) | 10–22% |
| Carga sostenida pesada | 3–10% |
| Sistema ya optimizado (ppd + TLP) | 0–8% incremental |

**Rango realista honesto:** **~8–20%** en portátil moderno con uso mixto y baseline no muy optimizado; **~3–8%** si el distro ya aplica perfiles agresivos.

## Ejemplo numérico

Batería **50 Wh**, consumo medio **12 W** → **~4,2 h**.

| Mejora total | Autonomía aprox. | Ganancia |
|--------------|------------------|----------|
| 10% | ~4,6 h | ~25 min |
| 15% | ~4,9 h | ~40 min |
| 20% | ~5,2 h | ~1 h |

Requiere misma carga de trabajo, mismo brillo de pantalla y misma conectividad para comparar.

## Validación empírica recomendada

1. Fijar brillo y tareas (reproducibles).
2. Sesión A: `mode performance` en batería; sesión B: `mode max-battery`.
3. Comparar `dev-metrics` (`rapl_watts_est`, `verify_*`) y/o `energy_now` de la batería si el driver lo expone.
4. Repetir al menos dos veces; reportar media y desviación.

Los números de este documento son **orientativos** hasta contar con benchmarks publicados del proyecto.
