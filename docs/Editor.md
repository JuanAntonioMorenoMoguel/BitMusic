# Editor de BitMusic (escritorio) — borrador

> Borrador de `docs/Editor.md` para el repo `BitMusic`. Todavía no hay código.
> Toma de FL Studio **las ideas de flujo de trabajo y de distribución** (rack de canales con secuenciador de pasos,
> piano roll, lista de reproducción, mezclador, explorador, barra de transporte, trabajo por patrones). No copia
> su aspecto, sus iconos, su marca ni sus recursos: el aspecto es el de `UI::JFxStyle` (Modena) de BitMotor.

## 1. Principios

- **El fichero manda.** Todo lo que se edita es un `BitMotor::Song` (`docs/BitMusic.md` en BitMotor). BitMusic no
  tiene modelo propio ni sintetizador propio: suena con `Music::Play(song)` de BitMotor.
- **Tres tipos de cosa** en este documento, para cada idea de FL Studio:
  - **Cabe tal cual**: se guarda con el formato actual.
  - **Cambio en BitMotor**: hace falta tocar el formato o la API de BitMotor; se propone y se hace allí primero.
  - **Solo editor**: estado del editor (selección, zoom, deshacer, silencio/solo...). No va en el `.bitmusic`.
- **Interfaz inmediata.** Cada vista es una función `DrawX(EditorState&, Rect)` llamada en `Draw`. Sin gestor de
  ventanas, sin nodos, sin ventanas flotantes: paneles fijos con `Fx::SplitPane` y `Fx::TabPane`.
- **Las ediciones son funciones puras** sobre `Song` (`SetStep(song, pattern, track, row, note)`...), probadas
  sin ventana. La interfaz solo decide cuál llamar.
- **Determinismo**: misma entrada, misma salida byte a byte; `Song::Save` con orden de claves fijo.

## 2. Distribución de la ventana

Resolución lógica 1280×720, tema oscuro por defecto (`Fx::Modena(#3C3F41, acento)`) y claro en *Ver*.

```
┌──────────────────────────────────────────────────────────────────────────────────────────┐
│ Archivo  Editar  Ver  Canción  Instrumento  Ayuda                          (menú, 28 px) │
├──────────────────────────────────────────────────────────────────────────────────────────┤
│ ▶ ■ ●  [Patrón|Canción]  BPM [120▾]  Filas/pulso [4▾]  003:02:01  Patrón [intro ▾]  Oct 4 │  transporte (40 px)
├────────────┬───────────────────────────────────────────────────────────┬─────────────────┤
│ EXPLORADOR │ LISTA DE REPRODUCCIÓN (Order)                             │ INSTRUMENTO     │
│ ▸ Instrum. │ ⚑loop                                                     │ bass            │
│   bass     │ [intro──][intro──][verso────][verso────][coro──────]      │ Onda [triangle▾]│
│   kick     │                                                           │ Vol ───●──      │
│ ▸ Patrones │───────────────────────────────────────────────────────────│ ADSR  /‾‾\__    │
│   intro    │ [ Pasos | Piano roll | Tracker ]   patrón: intro (16)     │ A ●  D ●  S ● R●│
│   verso    │ 0 kick  ■□□□■□□□■□□□■□□□  M S                             │ LowPass ──●     │
│ ▸ Presets  │ 1 snare □□□□■□□□□□□□■□□□  M S                             │ Crush  Downs.   │
│ ▸ Recientes│ 2 bass  C2 ·  ·  ·  G1 · off ...  M S                     │ Pan  Sweep ...  │
│            │ 3 lead  ...                                               │ [Mezclador]     │
├────────────┴───────────────────────────────────────────────────────────┴─────────────────┤
│ Pista 2 · bass · fila 4: G1  — Espacio: reproducir · Ctrl+Z: deshacer        (estado)   │
└──────────────────────────────────────────────────────────────────────────────────────────┘
```

- Izquierda: **Explorador**. Centro arriba: **Lista de reproducción**. Centro abajo: **editor de patrón** con tres
  vistas del mismo patrón. Derecha: **Instrumento** (o **Mezclador**, en pestaña). Abajo: **barra de estado**
  (la "barra de pistas" de FL: explica lo que hay bajo el ratón).
- Los tres divisores son `Fx::SplitPane`; sus proporciones se guardan en `Settings` (solo editor).
- Atajos para enfocar vistas, al estilo de FL pero sin F1 (es la depuración de BitMotor): F5 lista, F6 pasos,
  F7 piano roll, F8 explorador, F9 mezclador, F10 tracker.

## 3. Mapa de conceptos de FL Studio

| Concepto (FL Studio) | En BitMusic | ¿Dónde vive? |
|---|---|---|
| Patrón | `Pattern` con nombre en `Patterns` | **Cabe tal cual** |
| Canal del rack | Pista N de los patrones (= canal N de la canción) con su `Instrument` | **Cabe tal cual** (ver 4.2) |
| Instrumento/plugin del canal | `Instrument` (onda, ADSR, filtro, crush, pan, sweep, transpose) | **Cabe tal cual** |
| Secuenciador de pasos | Una entrada de `Notes` por paso | **Cabe tal cual**; la nota por defecto de cada canal es **solo editor** |
| Piano roll | Notas de una pista: inicio = nota, fin = `"off"` o la nota siguiente | **Cabe tal cual** (monofónico por pista) |
| Velocidad de nota | `":vol"` (`"C4:0.5"`) | **Cabe tal cual** |
| Pan / filtro por nota, *slides*, portamento | — | **Cambio en BitMotor** (futuro, no en v1) |
| Acordes | Repartir las notas en pistas seguidas con el mismo instrumento | **Cabe tal cual** con una ayuda del editor |
| Lista de reproducción (playlist) | `Order`, una sola fila de bloques | **Cabe tal cual** (una pista); varias pistas en paralelo = **cambio en BitMotor** (pregunta 1) |
| Marcador de bucle | `Loop` | **Cabe tal cual** |
| Modo patrón / modo canción | Reproducir el patrón actual en bucle o `Order` | **Solo editor** (necesita API, ver 5) |
| Tempo | `Bpm` | **Cabe tal cual** |
| Resolución de la rejilla | `Rows` (filas por pulso) | **Cabe tal cual** |
| Compás (4/4, 3/4) | Pulsos por compás | **Cambio en BitMotor** recomendado: `Beats` (pregunta 2) |
| Swing | — | Fuera de v1 (las filas duran todas igual) |
| Mezclador: fader, pan | `Volume`, `Pan` del instrumento; `Volume` de la canción como maestro | **Cabe tal cual** |
| Mezclador: efectos | `LowPass`, `Crush`, `Downsample` como "ranuras" fijas | **Cabe tal cual**; eco/reverb serían **cambio en BitMotor** (futuro) |
| Vúmetros | Nivel por canal mientras suena | **Cambio en BitMotor** (API) |
| Silencio / solo | Máscara de canales al reproducir | **Solo editor** + **cambio en BitMotor** (API) |
| Grabación desde teclado | Escribe notas cuantizadas a filas | **Solo editor** |
| Clips de automatización | — | **Cambio en BitMotor** (futuro, no en v1) |
| Explorador | Listas de instrumentos, patrones, presets y recientes | **Solo editor** (los presets son ficheros de BitMusic) |
| Colores de patrón y de canal, nombres de canal | — | **Solo editor** (pregunta 2) |
| Historial de deshacer | Copias del `Song` | **Solo editor** |
| Ventanas flotantes | Paneles fijos | No se copia: choca con "sin gestor de ventanas" |

## 4. Vistas

### 4.1 Barra de transporte

- **▶ / ■ / ●**: reproducir, parar, grabar. Espacio reproduce/para; Ctrl+Espacio reproduce desde la fila del cursor.
- **Patrón | Canción** (`Fx::ToggleButton` pareados, tecla L como en FL): patrón actual en bucle o `Order` entero.
- **BPM** y **Filas/pulso**: necesitan un `Spinner` (falta en JFxStyle). Editan `Bpm` y `Rows`.
- **Posición** `compás:pulso:fila` (o `orden:fila` en modo tracker) mientras suena.
- **Patrón actual** (`Fx::ComboBox`), **octava** del teclado (+/- en teclado numérico) y **paso de edición**
  (cuántas filas baja el cursor al escribir una nota en el tracker).

### 4.2 Rack de canales + secuenciador de pasos (pestaña *Pasos*)

- Una fila por pista del patrón actual: número, instrumento (`Fx::ComboBox` con `Instruments`), botones **M**
  (silencio) y **S** (solo), y los pasos.
- Un paso = una fila del patrón. Columnas agrupadas por pulso (`Rows`) y por compás (`Beats`, si se acepta).
- Clic en un paso vacío escribe la **nota por defecto del canal** (C4 o la última escrita; solo editor); clic en uno
  lleno lo vacía. Arrastrar pinta o borra varios. Clic derecho: menú contextual (nota, volumen, `off`).
- Un paso con una nota distinta de la por defecto muestra su nombre pequeño encima ("G1").
- Las pistas son canales de **toda la canción**: el instrumento se elige por patrón (es lo que guarda el formato),
  y el menú del canal ofrece "Usar este instrumento en todos los patrones". Un patrón nuevo copia los instrumentos
  de las pistas del patrón actual.
- Añadir/quitar/reordenar pistas: quitar la pista N mueve las siguientes en **todos** los patrones, para que el
  canal N siga siendo el mismo en toda la canción (función pura `RemoveChannel(song, n)`).

### 4.3 Piano roll (pestaña *Piano roll*)

- Edita **una pista** (canal) del patrón actual; las de las otras pistas se ven en gris como notas fantasma.
- Teclado vertical a la izquierda (clic = escuchar la nota), filas de tiempo en horizontal, zoom con Ctrl+rueda.
- Una nota es un bloque desde su fila hasta el `"off"` o hasta la nota siguiente. Dibujar de la fila r a r+n escribe
  la nota en r y `"off"` en r+n (si no empieza otra justo ahí). Como la pista suena de una en una, una nota nueva
  que se solapa recorta a la anterior: el piano roll lo enseña al momento.
- Carril de velocidad abajo: barras que editan el `":vol"` de cada nota.
- **Acordes**: la herramienta "Acorde" (de la biblioteca de ayudas, característica 8) escribe cada nota del acorde en
  pistas seguidas con el mismo instrumento, creándolas si faltan.

### 4.4 Tracker (pestaña *Tracker*)

La vista clásica de la característica 1: columnas por pista con nota y volumen, filas resaltadas por pulso y compás,
teclado de tracker (Z…M y Q…P como teclas de piano, `1` para `off`, Supr para vaciar), bloques para copiar y pegar,
transponer (Ctrl+↑/↓ octava, Shift+↑/↓ semitono). Las tres pestañas editan el mismo `Pattern`; la selección se
conserva al cambiar de pestaña cuando tiene sentido (filas × pistas).

### 4.5 Lista de reproducción (`Order`)

- Una fila de bloques, uno por entrada de `Order`, de ancho proporcional a `Length` y con el color del patrón;
  dentro, un dibujo en miniatura de sus notas.
- Arrastrar desde el explorador añade; arrastrar un bloque reordena; Ctrl+arrastrar duplica; Supr quita.
- El **marcador de bucle** (bandera sobre la regla) es `Loop`; arrastrarlo fuera a la derecha lo pone a -1 (para).
- Clic en la regla mueve el punto de reproducción; doble clic en un bloque abre ese patrón abajo.
- Una entrada de `Order` que no existe en `Patterns` se dibuja rayada en rojo (el sintetizador la salta).

### 4.6 Instrumento y Mezclador (panel derecho)

- **Instrumento** (característica 3): todos los campos de `Instrument`, gráfico ADSR que se arrastra con el ratón,
  dibujo de un ciclo de la onda, botón "Probar" y el teclado del ordenador para tocarlo.
- **Mezclador** (pestaña): una tira por instrumento (fader `Volume`, perilla `Pan`, ranuras fijas LowPass / Crush /
  Downsample, vúmetro) y una tira maestra (`Volume` de la canción). Es otra vista de los mismos campos.
  Las tiras son por instrumento y no por canal porque es lo que guarda el formato.

### 4.7 Explorador (panel izquierdo)

Secciones plegables: **Instrumentos** (nuevo, duplicar, renombrar, borrar), **Patrones** (nuevo, clonar, renombrar,
borrar), **Presets** (instrumentos de ejemplo de BitMusic: bombo, caja, charles, bajo, melodía; insertar copia uno
en la canción) y **Recientes**. Renombrar un instrumento o un patrón actualiza todas sus referencias (pistas y
`Order`) en una sola operación deshacible. Borrar uno en uso pregunta antes (`Fx::BeginDialog`).

### 4.8 Menús

- **Archivo**: Nuevo, Abrir, Recientes ▸, Guardar, Guardar como, Importar ▸ (MIDI, XM, IT, MOD), Exportar ▸ (WAV,
  MIDI, XM, IT), Salir. Los ▸ necesitan **submenús** (faltan en JFxStyle).
- **Editar**: Deshacer, Rehacer, Cortar, Copiar, Pegar, Seleccionar todo, Transponer ▸.
- **Ver**: las vistas (F5–F10), Tema oscuro, Seguir la reproducción.
- **Canción**: propiedades (Bpm, Rows, Volume, Loop), limpiar patrones sin usar.
- **Instrumento**: nuevo desde preset ▸, duplicar, probar. **Ayuda**: atajos, acerca de.
- Textos en `lang/es.json` y `lang/en.json`.

## 5. Lo que hace falta en BitMotor

Todo esto se propone y se hace en BitMotor antes de usarlo en BitMusic.

### 5.1 Audio (API, sin tocar el formato)

| Qué | Para qué | Propuesta |
|---|---|---|
| Renderizar una canción | Exportar WAV, pruebas con WAV de referencia | Hacer pública `Detail::RenderSong`: `Song::Render(bool repeat)` → muestras estéreo |
| Empezar en una posición | Reproducir desde el cursor, modo patrón | `Music::Play(song, {.Order = i, .Row = r})` |
| Saber dónde va | Seguir la reproducción, cursor de la lista | `Music::SongPosition()` → `{order, row}` |
| Silencio / solo | Botones M y S sin reiniciar | `Music::SetChannelMute(mask)` |
| Vúmetros | Mezclador | `Music::ChannelLevel(n)` |
| Tocar una nota suelta | Probar instrumentos, teclado, piano roll | `Music::PreviewNote(instrument, note, volume)` / `PreviewOff()` |

### 5.2 UI (`UI::JFxStyle`)

| Qué | Para qué |
|---|---|
| Recorte (*scissor*) en la capa `Hud` | Rejillas y listas con scroll suave y filas a medias |
| `ScrollArea` con barras horizontal y vertical, y zoom | Piano roll, tracker, lista de reproducción |
| Submenús | Recientes, Importar, Exportar, Transponer |
| `Spinner` | BPM, filas por pulso, longitud, octava |
| `Knob` (perilla) | Pan y parámetros del mezclador (si no, `Slider`) |
| `Grid` genérico: celdas, selección rectangular, arrastre, celda bajo el ratón, y el dibujo de cada celda lo hace quien llama | Pasos, tracker, piano roll... y la rejilla de píxeles de BitSprite (pregunta 3) |
| Arrastrar y soltar entre widgets | Explorador → rack / lista |

La rejilla y el scroll son genéricos y van en BitMotor; lo que es solo de música (cómo se dibuja una nota, un paso,
un bloque de patrón) se queda en BitMusic, encima de `Fx::Grid` y las formas de JFxStyle.

### 5.3 Formato (opcional, pregunta 2)

- `Beats` en la canción: pulsos por compás (4). No cambia cómo suena; lo usan las líneas de compás y la exportación
  a MIDI/XM (que necesitan el compás). Sigue siendo versión 1: si falta vale 4.

## 6. Estado solo del editor

- **Por sesión**: selección, cursor, pestaña, zoom, scroll, octava, paso de edición, silencio/solo.
- **Por canción** (en `Files::SavePath("songs/<hash de la ruta>.json")`, no en el `.bitmusic`): colores de patrón y
  de canal, nombres de canal, nota por defecto de cada canal, vista y zoom.
- **Global** (`Settings`): tema, proporciones de los paneles, recientes, idioma.
- **Deshacer/rehacer ilimitado**: una lista de copias de `Song` (las canciones son pequeñas). Un arrastre se junta
  en un solo paso. Sin "comandos" ni gestor: `Push(history, song)`, `Undo(history)`, `Redo(history)`.
- **Autoguardado** cada minuto en `Files::SavePath("autosave/")` con `Files::Write` atómico.

## 7. Aspecto

- `UI::JFxStyle` en todo: tema oscuro Modena por defecto, Inter para la interfaz.
- Las celdas de tracker y los nombres de nota usan una fuente de ancho fijo: la de píxeles 5×7 de BitMotor a 2×,
  que además da a BitMusic el aire de la familia Bit*.
- Colores de patrón: una paleta fija de 12 tonos sobre el fondo oscuro; cada patrón nuevo toma el siguiente.
- Pulsos y compases: fondo un poco más claro cada pulso y una línea cada compás.

## 8. Fases

0. **BitMotor**: API de audio de 5.1, scissor + ScrollArea + Spinner + submenús + Grid de 5.2, `Beats` si se acepta.
1. **Biblioteca sin ventana** en BitMusic: ediciones puras (pasos, notas del piano roll, canales, renombrar,
   transponer), historial, y sus pruebas (ida y vuelta de carga/guardado idéntica).
2. **CLI**: `bitmusic export cancion.bitmusic --wav salida.wav` + WAV de referencia comparados muestra a muestra.
3. **Editor, armazón**: menú, transporte, explorador, lista de reproducción, barra de estado, abrir/guardar/recientes.
4. **Pasos y tracker**; 5. **Piano roll**; 6. **Instrumento y mezclador**; 7. **Importar/exportar** (MIDI, XM, IT, MOD).

## 9. Preguntas abiertas (con recomendación)

1. **Lista de reproducción con una o varias pistas.** En FL los patrones se apilan en varias pistas a la vez; `Order`
   es una sola secuencia. *Recomiendo una sola pista en v1* (cabe tal cual); varias pistas sería un formato v2
   (`Order` con listas de patrones que suenan juntos) que se puede proponer después.
2. **Dónde van el compás, los colores y los nombres de canal.** *Recomiendo `Beats` en el formato* (es música y lo
   necesita la exportación MIDI/XM) *y lo cosmético en el estado del editor* (se pierde al abrir la canción en otro
   ordenador, pero el formato sigue limpio).
3. **Dónde van las rejillas.** *Recomiendo un `Fx::Grid` y un `Fx::ScrollArea` genéricos en BitMotor*, que también
   servirán a BitSprite, y el dibujo musical en BitMusic. Además, `Detail::Interact` es interno de BitMotor, así que
   un widget hecho fuera no tendría las mismas reglas de foco y clic.
