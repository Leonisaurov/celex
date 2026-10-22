# Teclas, clics y la fila de herramientas

Dos formas de mandar lo mismo, escritas una vez. La fila de botones no tiene
ninguna orden propia: cada botón **es** la tecla que hace lo mismo
(`TUI/tools.c:4-6`), así que no pueden contradecirse.

## Teclas

La columna de código es lo que llega de la terminal; los controles son los bytes
de siempre (Ctrl+B es 2).

### Moverse

| Tecla | Código | Qué hace |
|---|---|---|
| `h` `a` `←` | 104/97/258 | cursor a la izquierda |
| `l` `d` `→` | 108/100/259 | cursor a la derecha |
| `k` `w` `↑` | 107/119/256 | cursor arriba |
| `j` `s` `↓` | 106/115/257 | cursor abajo |
| rueda del ratón | 64 / 65 | una fila arriba, o abajo |

Los topes son la columna y la fila 65536; llegar al tope no avisa, simplemente no
se mueve.

### Escribir en una celda

| Tecla | Código | Qué hace |
|---|---|---|
| `i` o `Enter` | 105 / 10 | entra a editar **conservando** lo que había, con el cursor de texto al final |
| cualquier letra o signo imprimible | 32..126 | entra a editar **reemplazando** el contenido |
| doble clic | | lo mismo que `i` |
| `Esc` | 27 | termina la edición y recalcula |
| `Enter` | 10 | termina, recalcula y el cursor baja una fila |
| `↑` `↓` | | terminan la edición y mueven el cursor arriba o abajo |
| `←` `→` | | mueven el **cursor de texto**, no la celda |
| `Home` `End` | 260 / 261 | al principio o al final del contenido |
| `Backspace` | 127 (o 8) | borra el carácter de delante, carácter a carácter UTF-8 |
| `Supr` | 262 | borra el de detrás |
| `Tab` | 9 | autocompletado |
| `Ctrl+C` `Ctrl+V` | 3 / 22 | copia / pega **el texto** mientras se edita |
| `Ctrl+Z` `Ctrl+Y` | 26 / 25 | deshace / rehace también mientras se edita |
| `Ctrl+B` `Ctrl+U` `Ctrl+O` | 2 / 21 / 15 | el formato de la celda, también mientras se edita |
| `Ctrl+K` | 11 | pone o quita la fila de las herramientas |

### Vaciar y mirar

| Tecla | Qué hace | Mensaje |
|---|---|---|
| `x` o `Supr` (en la rejilla) | vacía la celda y recalcula | el del recálculo |
| `v` | alterna valores y fuentes | ninguno |
| `]` y `[` | celda más ancha o más estrecha (de 2 a 40) | la barra lo muestra (`8x2`) |
| `}` y `{` | celda más alta o más baja (de 1 a 8) | ídem |
| `Ctrl+T` | el siguiente tema de colores | el nombre del tema |

### Formato de la celda

| Tecla | Qué hace | Mensaje |
|---|---|---|
| `Ctrl+B` | negrita, va y vuelve | `bold on` / `bold off` |
| `Ctrl+U` | subrayado, va y vuelve | `underline on` / `underline off` |
| `Ctrl+O` | recorre la alineación: por defecto → izquierda → centro → derecha | `align default` / `align left` / `align center` / `align right` |

Poner formato a un sitio vacío hace que la celda exista, y cuenta como un cambio de
la hoja (si no, el archivo no se guardaría después de formatear). No hay tecla para
los colores propios: se ponen en el documento, ver [temas-y-formatos.md](temas-y-formatos.md).

### Hojas

| Tecla | Qué hace | Mensaje |
|---|---|---|
| `Ctrl+A` | una hoja nueva, detrás de la que se ve | el nombre nuevo, o `cannot add a sheet` |
| `Ctrl+N` / `Ctrl+P` | la hoja siguiente / anterior, dando la vuelta | el nombre de la que queda |
| `Ctrl+D` | quita la que se ve | `sheet removed`, o `the last sheet stays` |
| `Ctrl+F` | escribe el nombre de la que se ve | `new name: Enter to set, Esc to leave` |
| clic en una pestaña | esa hoja | su nombre |

### Archivo

| Tecla | Qué hace | Mensaje |
|---|---|---|
| `Ctrl+S` | guarda el **documento** `.clex` | `saved`, `no file to save`, `cannot write file` |
| `Ctrl+E` | exporta como **guion** `.cle` la hoja que se ve | `exported <ruta>`, `nothing to export`, `the name does not fit`, `cannot write the script` |
| `Ctrl+L` | vuelve a leer el archivo | `reloaded`, `no file to load`, y si hay cambios `unsaved changes: press Ctrl+L again to reload` |
| `Ctrl+R` | evalúa la hoja | `script executed`, o el aviso del fallo |
| `q` | salir | si hay cambios: `unsaved changes: press q again to quit` |

Al abrir, lo que dice la barra depende de lo que era el archivo:
`imported: saves as <ruta>` si se abrió un `.cle` (guardará al lado como `.clex`),
`file loaded` si era un `.clex`, `loaded (N lines ignored)` si tenía líneas que no
eran celdas, `cannot open file` si no se pudo leer, y `new sheet` si el archivo no
existía.

La confirmación de salir se cancela con cualquier otra tecla que no sea `q`, y la de
recargar con cualquiera que no sea `Ctrl+L`.

## El ratón

Llega a la terminal como `ESC[<botón;columna;fila M` (pulsar) o `m` (soltar), y es
lo único que entiende el editor: en Windows la consola da eventos de ratón y la
capa de sistema los convierte en esa misma secuencia, así que solo hay un camino
después. Los números empiezan en uno al llegar y en cero dentro del editor.

| Dónde | Qué hace |
|---|---|
| una celda de la rejilla | mueve el cursor allí |
| la misma celda dos veces, en menos de 500 ms | entra a editarla |
| cualquier celda, con el botón derecho | pone el cursor y copia: `copied A1` |
| una raya, la cabecera de las letras, el margen de los números o las filas de abajo | nada: no son celdas |
| una pestaña | cambia de hoja |
| el chip `[tools]` al final de la fila de las pestañas | vuelve a poner la fila de las herramientas |
| el nombre del archivo, al final de la barra de estado | lo mismo |
| un botón de la fila de herramientas | manda su tecla |
| la rueda | una fila arriba o abajo |

Soltar el botón, o moverse sin apretar nada, no es ninguna orden. El botón del
medio tampoco.

**Clic mientras se escribe en una celda**: si el clic cae en otra parte, primero se
termina la escritura como con `Esc` (y se recalcula) y luego hace lo que toque. Así
no se pierde lo escrito por llevar la mano al ratón. **Clic mientras se escribe el
nombre de una hoja**: corta la escritura del nombre y la descarta, y hace lo que
toque.

## La fila de herramientas

Una sola lista, en `TUI/tools.c`, de nombre y tecla; de ahí salen y el dibujo y el
clic. Delante lo que más se usa, para que lo que no quepa en una pantalla corta sea
lo de menos:

| # | Botón | Manda | # | Botón | Manda |
|---|---|---|---|---|---|
| 0 | `bold` | `Ctrl+B` | 7 | `save` | `Ctrl+S` |
| 1 | `line` | `Ctrl+U` | 8 | `hide` | `Ctrl+K` |
| 2 | `align` | `Ctrl+O` | 9 | `run` | `Ctrl+R` |
| 3 | `copy` | `Ctrl+C` | 10 | `value` | `v` |
| 4 | `paste` | `Ctrl+V` | 11 | `sheet` | `Ctrl+N` |
| 5 | `undo` | `Ctrl+Z` | 12 | `theme` | `Ctrl+T` |
| 6 | `redo` | `Ctrl+Y` | | | |

En una pantalla de ochenta columnas caben diez: hasta `run`. `value`, `sheet` y
`theme` esperan una pantalla más ancha, y siguen alcanzables por teclado.

**Quitar la fila.** El botón `hide` (o `Ctrl+K`) la quita y le devuelve una fila a
la rejilla. No cuenta como un cambio del archivo, como no lo cuenta cambiar el
tamaño de las celdas; se guarda con el archivo la próxima vez que se guarde, en la
línea `tools off`.

**Volver a ponerla.** Quitar la fila quita el botón que la quita, así que quedan
dos sitios visibles que tocar: el chip `[tools]` al final de la fila de las
pestañas, que solo se pinta mientras la fila está fuera, y el nombre del archivo al
final de la barra de estado, que está siempre. `Ctrl+K` hace lo mismo desde el
teclado. El aviso que deja la barra es `tools off`, corto a propósito: un aviso
largo se comería el nombre, que es justo lo que hay que ver y tocar.

## La barra de estado como menú

No hay menús desplegables ni barras laterales: lo que se puede tocar son los
botones de la fila de herramientas, las pestañas y el nombre del archivo. Todo lo
demás se hace con las teclas de arriba. Lo que la barra dice es el menú de
resultados: `saved`, `copied D12`, `tools off`, `align center`, `4x2`, `VAL` o
`SRC`, y el número de compilación con el que se está trabajando.

## Donde encaja con lo demas

- Donde esta cada barra y cuanto mide: [pantalla.md](pantalla.md).
- Lo que hacen `Ctrl+S` y `Ctrl+E` sobre los ficheros: [archivos.md](archivos.md).
- Lo que hace cada tecla de formato: [temas-y-formatos.md](temas-y-formatos.md).

Volver al indice: [docs/README.md](README.md).
