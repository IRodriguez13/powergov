# PowerGov UI (GTK 3)

Interfaz de escritorio nativa. Usa `libpowergov.so` y el socket Unix del daemon (`/run/powergov/powergov.sock`).

## Build y ejecución

```bash
make libpowergov.so powergov-ui
./powergov-ui
powergov-ui -v    # misma plantilla que pack/extract -v
```

Requiere `pkg-config gtk+-3.0`. El daemon debe estar en ejecución (`systemd` o `sudo powergov on`).

## AppImage (usuario final)

```bash
make appimage
# dist/PowerGov-<version>-x86_64.AppImage
```

El AppImage incluye la **UI** y scripts de instalación del servicio. Primer arranque: acceso en menú/escritorio (rutas XDG), detección de idioma, instalación opcional del backend con contraseña de admin.

Instalación estable: `~/.local/share/powergov/PowerGov.AppImage`. Actualizaciones desde GitHub sobrescriben ese path (sin duplicados en `$XDG_DOWNLOAD_DIR`).

## Instalación desde código

```bash
sudo make install-ui install-ui-policy install-ui-helper
make install-ui-shortcut   # acceso directo en $XDG_DESKTOP_DIR
```

Incluye icono, `.desktop`, Polkit (`org.powergov.policy`) y helper `powergov-dev-auth`.

## Pestañas

| Área | Contenido |
|------|-----------|
| **Usuario** | Modos max-battery / balanced / performance, reserva batería (slider), estado, instalar/desinstalar servicio |
| **Acerca de** | Versión UI/daemon (`powergov-ui -v` / `powergov -v`), licencia GPLv3, enlace al repo |
| **Dev** (Polkit) | Métricas, log, features, umbrales, periféricos (modo custom, a demanda) |

Modo **custom** y tuning avanzado solo en Dev. Los checkboxes de periféricos esperan la primera sincronización con el daemon antes de aceptar clics (evita desalineación UI/config).

## Bandeja del sistema

Cerrar la ventana con **X** oculta a la bandeja (icono rayo). Clic en el icono restaura la ventana. Menú contextual: mostrar / salir.

Desactivar bandeja: `POWERGOV_NO_TRAY=1 powergov-ui` (X cierra la aplicación).

## Rendimiento UI (v1.10)

- Refresco adaptativo: ~8 s con ventana enfocada; pausa sin foco o en bandeja
- Carga perezosa en pestañas Dev (bundle parcial por pestaña)
- Comprobación de actualización GitHub una vez al arrancar (diferida ~5 s)

## Idioma

Inglés por defecto. Español si `LANG`/`LC_MESSAGES`/`LANGUAGE` empiezan por `es`, o botón **EN** / **ES** en la barra de título.

Término UI: periféricos en modo custom = **«a demanda»** (ES) / **«on demand»** (EN).

```bash
LANG=es_ES.UTF-8 powergov-ui
```

## Variables de entorno útiles

| Variable | Efecto |
|----------|--------|
| `POWERGOV_NO_TRAY=1` | Sin icono de bandeja; X termina el proceso |
| `PG_UPDATE_CHECK_DELAY_MS` | Retardo ms antes del check de release (default 5000) |
