# BitMusic

Un editor de música para juegos. Las canciones suenan con el sintetizador de [BitMotor](https://github.com/JuanAntonioMorenoMoguel/BitMotor) y se guardan en su formato `.bitmusic` (descrito en `docs/BitMusic.md` de BitMotor), así que un juego las reproduce con `Music::Play("musica/tema.bitmusic")` sin nada más.

**Versión:** 0.1.0 · **Plataformas:** Windows 10/11 (64 bits), Linux · **Idiomas:** español, inglés

## El editor

La ventana sigue la forma de trabajar de un DAW de patrones (ver [docs/Editor.md](docs/Editor.md)), con el aspecto de `UI::JFxStyle` de BitMotor en tema oscuro:

- **Transporte**: reproducir, parar y grabar; modo patrón o canción (`L`); BPM, filas por pulso y pulsos por compás; posición; patrón que se edita, octava del teclado y paso de edición.
- **Explorador** (izquierda): instrumentos y patrones; se arrastran a la lista de reproducción o a una pista.
- **Lista de reproducción** (arriba): el `Order` de la canción como bloques; arrastrar para reordenar (con Ctrl duplica), clic derecho para el bucle, duplicar o quitar.
- **Patrón** (abajo), tres vistas del mismo patrón: **Pasos** (rack de canales con silencio y solo), **Piano roll** (una pista, con carril de volumen) y **Tracker** (teclado de tracker, bloques, transponer).
- **Instrumento y Mezclador** (derecha): todos los campos de `Instrument` con su onda y su envolvente, y una tira por instrumento con vúmetro.
- Deshacer y rehacer sin límite, recientes, autoguardado cada minuto en la carpeta de guardado y exportación a WAV.

Ayuda > Atajos de teclado tiene la lista completa. Importar y exportar MIDI, XM, IT y MOD está en los menús pero todavía no funciona.

## Línea de órdenes

El mismo programa sirve para scripts y builds:

```bash
bitmusic                                   # el editor
bitmusic cancion.bitmusic                  # el editor con esa canción
bitmusic export cancion.bitmusic --wav salida.wav [--passes 2]
bitmusic info cancion.bitmusic
bitmusic help
```

La exportación es determinista: la misma canción da siempre los mismos bytes (WAV de 16 bits, estéreo, 48000 Hz).

## Compilar

Requisitos: CMake 3.24+, un compilador de C++20, [vcpkg](https://vcpkg.io) (variable `VCPKG_ROOT`) y BitMotor en la carpeta `../BitMotor`, junto a este repositorio (o su ruta en `BITMOTOR_PATH`).

```bash
./build.sh            # Linux        (.\build.ps1 en Windows)
./run.sh              # ejecuta build/bin/BitMusic
ctest --test-dir build -C Debug --output-on-failure
```

Mientras se desarrolla, el programa lee `assets/` y `lang/` directamente de la carpeta del código. Opciones de BitMotor útiles: `--debug` (panel de depuración, también F1), `--frames 120 --screenshot captura.png`.

## Estructura

| Carpeta | Contenido |
|---|---|
| `include/BitMusic/`, `src/core/` | `BitMusicCore`, sin ventana ni sonido: ediciones de canciones (`Edit.h`), deshacer (`History.h`), WAV (`Wav.h`), presets (`Presets.h`) y la línea de órdenes (`Cli.h`) |
| `src/editor/` | El editor: `Editor.cpp` (menús, transporte, diálogos, archivos, reproducción), `Explorer.cpp`, `PatternViews.cpp`, `InstrumentViews.cpp` |
| `assets/` | La fuente Inter y una canción de ejemplo |
| `lang/` | Textos en `es` y `en` |
| `tests/` | Pruebas unitarias (`BitMusicTests`) |
| `docs/` | El diseño del editor |

El formato no tiene modelo propio en BitMusic: se usan `Song`, `Pattern`, `Instrument` y `Song::Load/Save` de BitMotor. Lo que es solo del editor (selección, deshacer, recientes) no va en el archivo.

## Legal

- [Licencia](LICENSE): todos los derechos reservados.
- [Avisos de terceros](THIRD_PARTY_NOTICES.md).
