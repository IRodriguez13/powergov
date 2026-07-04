# PowerGov UI (GTK)

Interfaz de escritorio nativa en GTK 3. Usa `libpowergov.so` y el socket Unix del daemon (`/run/powergov/powergov.sock`).

## Build

```bash
make libpowergov.so powergov-ui
./powergov-ui
```

Requiere `pkg-config gtk+-3.0`.

## AppImage (UI portable)

```bash
make appimage
# dist/PowerGov-<version>-x86_64.AppImage
```

El AppImage incluye solo la **UI**. El daemon hay que instalarlo una vez con `./install-powergov.sh`. Sin servicio, la UI indica que PowerGov no está en ejecución.

## Instalación

```bash
sudo make install-ui install-ui-policy install-ui-helper
make install-ui-shortcut   # acceso directo en el escritorio (como tu usuario)
```

Incluye icono, entrada `.desktop`, Polkit y acceso directo opcional en el escritorio.

## Idioma

**Inglés por defecto.** Español si `LANG`/`LC_MESSAGES`/`LANGUAGE` empiezan por `es`, o con el botón **EN** / **ES** en la barra del título (muestra el idioma actual; clic alterna).

```bash
LANG=es_ES.UTF-8 powergov-ui   # español al iniciar
powergov-ui                    # inglés al iniciar
```
