# PowerGov UI (GTK 3)

Interfaz de escritorio nativa. Usa `libpowergov.so` y el socket Unix del daemon (`/run/powergov/powergov.sock`).

## Build y ejecución

```bash
make libpowergov.so powergov-ui
./powergov-ui
powergov-ui -v    # misma plantilla que pack/extract -v
man powergov-ui   # tras make install
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

Incluye icono, `.desktop`, Polkit (`org.powergov.policy`) y helper para acciones privilegiadas.

## Pestañas

| Área | Contenido |
|------|-----------|
| **Usuario** | Modos max-battery / balanced / performance, reserva batería (slider), estado, instalar/desinstalar servicio |
| **Información** | Sistema (fuente, SOC, tapa, sesión idle), compatibilidad TLP/ppd, métricas, log, funciones y tuning |
| **Acerca de** | Versión UI/daemon (`powergov-ui -v` / `powergov -v`), licencia GPLv3, enlace al repo |

Modo **custom** y checkboxes de features/tuning en **Información → Funciones**. Opciones de contexto: agresividad con tapa cerrada y con sesión idle/pantalla apagada.

## Cache optimista (pending)

Al activar o desactivar features opcionales, tuning de periféricos o checkboxes de tapa/pantalla:

1. La UI refleja el estado elegido **de inmediato**.
2. El control queda atenuado (clase CSS `pg-periph-pending`) y deshabilitado hasta confirmación.
3. Si el daemon falla, se revierte al valor confirmado por query.

Evita parpadeos cuando el refresco periódico del snapshot llega antes que la escritura en el socket.

## Bandeja del sistema

Cerrar la ventana con **X** oculta a la bandeja (icono rayo). Clic en el icono restaura la ventana. Menú contextual: mostrar / salir.

Desactivar bandeja: `POWERGOV_NO_TRAY=1 powergov-ui` (X cierra la aplicación).

## Rendimiento UI

- Refresco adaptativo: ~8 s con ventana enfocada; pausa sin foco o en bandeja
- Carga perezosa en pestañas de Información (métricas/log bajo demanda)
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
| `POWERGOV_FG=1` | Primer plano (AppImage / `.desktop`) |
| `PG_UPDATE_CHECK_DELAY_MS` | Retardo ms antes del check de release (default 5000) |
