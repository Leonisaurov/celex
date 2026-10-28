# Cómo se comprueba

Una orden en cada plataforma corre todo:

```
build.bat test          Windows, con MSVC
make test               POSIX, con gcc
```

Salen cinco grupos y un runner de casos, en este orden; si alguno falla se para: los
casos pinchan marcando `FALLO`, y el aviso final `Fallaron las pruebas.` lo da solo
`build.bat` (a `make` le basta con cortar). Cada grupo imprime una sola línea de
resumen: `9 casos, 0 fallos`, `edicion: todo correcto`, `teclas: ...`, `entrada:
...`, `editor: ...`, `consola: ...`. Los avisos de cada prueba son en español.

## Los casos del lenguaje

`tests/runner.c` corre el intérprete sobre cada caso y compara.

- `tests/cases.txt` dice los casos, uno por línea, sin extensión. Las líneas vacías
  y las que empiezan por `#` se ignoran.
- `tests/cases/<nombre>.cle` es el programa.
- `tests/expected/<nombre>.txt` es lo esperado. **Su primera línea es el código de
  salida**: `# exit: 0`. Si además lleva la palabra `programa` el runner ejecuta sin
  `--values` y compara **lo que el programa imprime**; si no, ejecuta con `--values`
  y compara **las celdas exportadas**. El resto del fichero es la salida esperada,
  línea a línea.
- La entrada está cerrada siempre, y el error se tira: un `input` no depende de la
  consola, y un mensaje de depuración no ensucia la comparación.

Los casos de hoy:

| Caso | Qué mira |
|---|---|
| `hoja` | una hoja con números, una suma y una cadena |
| `dependencias` | una celda que usa a otra |
| `cadena` | tres celdas en cadena: `A1 ← B1 ← C1` |
| `programa` | variables, `for` y `print`: en modo `--values` no deja celdas |
| `circulo` | `A1 ↔ B1`: `referencia circular entre celdas: A1, B1`, y sale con error |
| `autorreferencia` | `A1 = A1 + 1`: lo mismo, con una sola celda |
| `vacias` | cadena, booleano y producto |
| `vacia` | una asignación sin valor: esa celda no se escribe |
| `lenguaje` | todo el lenguaje, corrido como programa |

## Las cinco pruebas del editor

**`editor_test`** — el gestor de celdas y el documento, sin terminal. Llama a las
funciones y mira lo que sale: editar una celda, cursor UTF-8, deshacer y rehacer,
copiar y pegar, invalidar el valor, evaluar celda a celda, los fotogramas del
deslizamiento, los límites del tamaño, las hojas y sus nombres, cuanto cabe en la
fila de las pestañas, el ida y vuelta del `.clex` (cabecera, hojas, cursor,
formatos, campos que no se conocen, una línea de formato rota), las preferencias
del usuario (escribir, leer, y que una línea rara no rompa el archivo), la
alineación, la barra de herramientas, las ventanas flotantes y los caminos de las dos extensiones.

**`keys_test`** — las teclas y los clics, sin terminal: se llama a `handle_key` y a
`handle_mouse`, las mismas funciones del programa. Cubre escribir y confirmar,
copiar y pegar (celda y texto), el pegado de la terminal, los tamaños, los temas,
el formato por tecla y por pintado (`\x1b[1m`, `\x1b[4m`, `\x1b[32m`), la fila de
herramientas y su `Ctrl+K`, todos los clics (celda, raya, cabecera, doble clic,
derecho, rueda, botón, pestaña, chip, nombre del archivo) y el autocompletado. Para
mirar lo que se pinta redirige la salida estándar a un archivo temporal y busca las
secuencias dentro; el portapapeles del sistema está apagado, para no tocar lo de la
máquina.

**`input_test`** — el analizador de la entrada, alimentándole bytes: teclas sueltas,
los controles, las secuencias de escape, el Escape solo (que no se resuelve hasta
saber que no llega nada más), las secuencias del ratón —botón, coordenadas,
pulsar/soltar, cifras de varias, y una secuencia partida en dos lecturas—, el escape
raruno que al final era un Escape, y los pegados: la ráfaga, la tecla repetida que
no es pegado, y el pegado con marcadores partido en trozos.

**`app_test`** — el editor entero, con un archivo de teclas. Fabrica un `.keys` con
los bytes exactos (incluidas las secuencias del ratón y del pegado) y corre el
binario con la entrada en ese archivo y la salida en otro. Después mira lo que quedó
en los archivos (el `.cle` exportado y el `.clex` guardado) y lo que se dibujó.
Cubre: copiar, pegar, exportar y guardar; el pegado desde la terminal; que lo que
está fuera de la vista no se dibuja y que la vista se desliza hasta la celda del
cursor; los formatos y la fila escondida, que ahora queda en las preferencias del
usuario; los colores del tema; y las dos hojas con sus pestañas. Cada corrida
estrena una casa temporal, para que lo que elija una no se lo deje a la siguiente
ni toque la del usuario; al terminar la prueba borra las casas que creó, con el
archivo de las preferencias y la plantilla de los temas dentro. En Windows lee
además el portapapeles del sistema, para comprobar que lo copiado llega ahí.

**`console_test`** — lo mismo dentro de una **consola de verdad**: en Windows una
pseudoconsola (`CreatePseudoConsole`, 80×24) y en POSIX un dispositivo pseudo-terminal.
Es la prueba de lo que se usa todos los días: las teclas con Ctrl se mandan como las
manda una terminal, y el editor tiene que leerlas, dibujar y salir sin dejar la
terminal rota. Comprueba que el editor contesta y sale, que el clic mueve el cursor,
que se guardan los dos archivos, que lo pegado aparece, y que el pegado no pone la
pantalla en modo edición.

## Dos cosas que se miran y no se comprueban

No forman parte de `test`: se construyen con `build.bat tools` o `make tools`.

**`build/screen <editor> <teclas> [hoja]`** lee lo que el editor escribió y lo
monta como una pantalla de texto, moviendo el cursor e interpretando los colores.
Flags: `-ancho` y `-alto`, `-hasta N` (se corta a los N primeros bytes, para ver un
fotograma en medio de un deslizamiento), `-cuadros` (cuántos bytes y cuántos saltos
de cursor costó) y `-color` (marca cada celda con lo que lleva puesto:
`<3 n u>` es color 3 de delante, negrita y subrayado; `<b4>` color 4 de fondo; `<i>`
inverso). Sirve para mirar con los propios ojos un layout: dónde cae el resaltado,
cómo se corta una celda en el borde, qué secuencias salen.

**Ojo con `-ancho` y `-alto`**: al editor la entrada y la salida le llegan por
archivo, sin consola, y sin consola no hay tamaño que leer — se dibuja a 80×24
siempre. Los dos números solo recortan lo que se mira. Para ver otro tamaño de
pantalla de verdad hay que mirarlo en la terminal, o correr `console_test`, que sí
pone el tamaño en la pseudoconsola.

**`build/bench [ancho alto]`** mueve el cursor por una hoja llena y dice cuanto
cuesta un fotograma, en microsegundos y en bytes escritos. Mide tres cosas: moverse
a la celda de al lado (la cámara quieta), moverse con la cámara detrás (cambia el
cuadro entero) y escribir. Es lo que hay que mirar antes de decir que algo va a
saltos: mientras el editor dibuja no mira el teclado, así que un fotograma lento es
una tecla que contesta tarde.

## La costumbre de comprobar la comprobación

Ninguna afirmación vale nada si no falla cuando debe. Cada prueba nueva se
comprueba rompiendo a propósito lo que mira, y viendo que pincha: quitar la línea
esperada de un caso, no invalidar la pantalla al cambiar de tema, hacer
siempre-planos los fotogramas del deslizamiento, aceptar colores fuera de la
paleta, borrar el dibujo de la barra de herramientas, o hacer que el sitio del
nombre del archivo conteste siempre sí y siempre no. Si la prueba sigue pasando, la
prueba no decía nada.

## Donde encaja con lo demas

- Como se compila cada binario de prueba y con que: [organizacion.md](organizacion.md).
- Lo que miran `screen` y `bench`, que sale de ese pintado: [pantalla.md](pantalla.md).
- Que comprueba cada grupo: las cinco pruebas del editor estan partidas por tema en [teclas-y-raton.md](teclas-y-raton.md), [hojas-historial-y-entrada.md](hojas-historial-y-entrada.md) y [archivos.md](archivos.md).

Volver al indice: [docs/README.md](README.md).
