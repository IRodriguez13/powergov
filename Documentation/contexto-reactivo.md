# Contexto reactivo (tapa, sesión idle, carga baja)

> **Última verificación:** 2026-07-08  
> **Fuente de verdad:** `power/profile.c` (`apply_context_aggression_boost`), `power/lid_state.c`, `power/session_idle.c`, `power/power_supply.c`, `include/powergov/types.h`

Desde **v1.13**, powergov puede subir temporalmente la agresividad de device PM cuando el portátil está en batería y el contexto de uso lo permite — sin esperar a que la máquina de estados baje a POWERSAVE.

## Cuándo aplica el boost

Función: `apply_context_aggression_boost()` en `power/profile.c`.

Condiciones **todas** necesarias:

1. Fuente **batería** (`on_battery`).
2. Al menos un **disparador** activo:
   - `LID_AGGRESSIVE=1` y tapa **cerrada** (`lid_closed == 1`).
   - `DISPLAY_AGGRESSIVE=1` y sesión **idle** (`session_idle == 1`, logind `IdleHint`).
3. Si `CONTEXT_REQUIRE_LOW_LOAD=1` (default): carga CPU ≤ `CONTEXT_LOW_LOAD_PCT` % (default **15**).

Efecto si se cumple:

- `device_aggression` → mínimo **3** (máximo).
- `runtime_pm_aggressive` → **1**.
- Recalcula `peripheral_pm_level` según flags `PERIPHERAL_*`.

No modifica governor, EPP ni turbo directamente; actúa sobre la capa de dispositivos (runtime PM, disk_pm, pcie_aspm, bluetooth_pm, peripheral_pm) que ya dependen de `device_aggression`.

## Detección de tapa (`lid_state`)

`powergov_lid_poll()` prueba en orden:

| Origen | Ruta |
|--------|------|
| ACPI legacy | `/proc/acpi/button/lid/*/state` |
| ACPI sysfs | `/sys/bus/acpi/devices/LID*/state` |

Valores: `1` = cerrada, `0` = abierta, `-1` = desconocido (sin boost por tapa).

## Detección de sesión idle (`session_idle`)

`powergov_session_idle_poll()`:

1. `busctl get-property org.freedesktop.login1 … IdleHint --value`
2. Fallback: `loginctl show-session self -p IdleHint --value`

Valores: `1` = idle, `0` = activa, `-1` = no disponible (sin boost por pantalla).

Requiere **systemd-logind** y sesión de usuario gráfica o de consola con hint válido. En entornos sin logind el boost por `DISPLAY_AGGRESSIVE` no se activa.

Integración: `powergov_power_supply_poll()` actualiza `lid_closed` y `session_idle` en `powergov_power_info_t` cada ciclo de refresco de batería (ver poll adaptativo).

## Poll adaptativo de batería

Constantes en `types.h`:

| Constante | Ticks (× ~2 s) | Intervalo aprox. |
|-----------|----------------|------------------|
| `POWERGOV_BATTERY_REFRESH` (BAT) | 5 | ~10 s |
| `POWERGOV_BATTERY_REFRESH_AC` | 30 | ~60 s |

En AC el daemon relee batería/tapa/idle con menos frecuencia; en batería más a menudo para reaccionar al cerrar tapa o al bloquear pantalla.

## Configuración

Claves en `/etc/powergov.conf` (también vía socket `SET_TUNING` / UI):

| Clave | Default | Descripción |
|-------|---------|-------------|
| `LID_AGGRESSIVE` | 1 | Boost con tapa cerrada en batería |
| `DISPLAY_AGGRESSIVE` | 1 | Boost con sesión idle (pantalla apagada / bloqueo) |
| `CONTEXT_REQUIRE_LOW_LOAD` | 1 | Exigir carga baja para el boost |
| `CONTEXT_LOW_LOAD_PCT` | 15 | Umbral de carga CPU (%) |

Tuning IDs: `POWERGOV_TUNING_LID_AGGRESSIVE`, `POWERGOV_TUNING_DISPLAY_AGGRESSIVE`.

## UI GTK

En **Información → Sistema**: estado de tapa y sesión idle (texto).

En **Información → Funciones**: checkboxes *Agresividad con tapa cerrada* y *Agresividad con pantalla apagada / sesión idle*.

Cache **optimista pending**: al togglear, la UI muestra el estado elegido de inmediato (clase `pg-periph-pending`) hasta que el daemon confirma vía socket.

## Ejemplos de uso

```bash
# Desactivar boost por tapa (solo conf)
sudo sed -i 's/^LID_AGGRESSIVE=.*/LID_AGGRESSIVE=0/' /etc/powergov.conf
sudo systemctl restart powergov

# Boost aunque haya carga media (no recomendado en uso interactivo)
# CONTEXT_REQUIRE_LOW_LOAD=0 en powergov.conf
```

## Limitaciones

- No controla backlight ni DPMS directamente; usa el hint de logind.
- Con **TLP** activo, las capas device PM siguen diferidas; el boost de `device_aggression` no se materializa en sysfs de discos/PCIe si TLP es dueño.
- Con **presión de memoria** activa (`MEMORY_AWARE=1`), el boost contextual no se aplica; ver [memory-aware.md](memory-aware.md).
- Tapa o idle desconocidos (`-1`) no disparan boost.
