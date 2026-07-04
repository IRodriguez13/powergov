# PowerGov UI (GTK)

Interfaz de escritorio nativa en GTK 3. Usa `libpowergov.so` y el socket Unix del daemon (`/run/powergov/powergov.sock`).

## Build

```bash
make libpowergov.so powergov-ui
./powergov-ui
```

Requiere `pkg-config gtk+-3.0`.

## Instalación

```bash
sudo make install-ui install-ui-policy install-ui-helper
```

Incluye icono, entrada `.desktop` y helper Polkit para modo diagnóstico.
