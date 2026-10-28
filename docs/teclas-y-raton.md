# Teclas, clics y la barra de herramientas

Dos formas de mandar lo mismo, escritas una vez. La barra y sus desplegables no
tienen ninguna orden propia: cada botón **es** la tecla que hace lo mismo
(`TUI/tools.c:11-39`, y el clic que manda esa tecla en `TUI/keys.c:469-505`),
así que no pueden contradecirse.

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
| `Ctrl+K` | 11 | pone o quita la barra de las herramientas |

### Vaciar y mirar

| Tecla | Qué hace | Mensaje |
|---|---|---|
| `x` o `Supr` (en la rejilla) | vacía la celda y recalcula | el del recálculo |
| `v` | alterna valores y fuentes | ninguno |
| `]` y `[` | celda más ancha o más estrecha (de 2 a 40) | la barra lo muestra (`8x2`) |
| `}` y `{` | celda más alta o más baja (de 1 a 8) | ídem |
| `Ctrl+T` | abre la ventana de los temas: cada nombre escrito con su propio tema | el nombre del tema |

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
| `Ctrl+G` | ventana flotante con un campo: guardar con otro nombre | `saved <nombre>`, `nothing to write`, `cannot write file` |
| `Ctrl+Q` | ventana flotante con un campo: abrir otro archivo | `opened <nombre>`, `new sheet: saves as <ruta>`, `cannot open file`, y con cambios `unsaved changes: Ctrl+S before opening` |
| `Ctrl+E` | exporta como **guion** `.cle` la hoja que se ve | `exported <ruta>`, `nothing to export`, `the name does not fit`, `cannot write the script` |
| `Ctrl+L` | vuelve a leer el archivo | `reloaded`, `no file to load`, y si hay cambios `unsaved changes: press Ctrl+L again to reload` |
| `Ctrl+R` | evalúa la hoja | `script executed`, o el aviso del fallo |
| `q` | salir | si hay cambios: `unsaved changes: press q again to quit` |

Al abrir, lo que dice la barra depende de lo que era el archivo:
`imported: saves as <ruta>` si se abrió un `.cle` (guardará al lado como `.clex`),
`file loaded` si era un `.clex`, `loaded (N lines ignored)` si tenía líneas que no
eran celdas, `cannot open file` si no se pudo leer, y `new sheet` si el archivo no
existía. `Ctrl+Q` hace lo mismo con el archivo que se escriba en la ventana, y se
queda en ese sitio para los `Ctrl+S` de después.

Guardar con otro nombre sale con el campo **vacío** y el nombre actual en el aviso:
escribir detras de un nombre viejo nunca dio un nombre. Abrir sale con la carpeta
del archivo que se ve delante, para cambiar solo el nombre de lo de al lado.

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
| el chip `[tools]` al final de la fila de las pestañas | vuelve a poner la barra de las herramientas |
| el nombre del archivo, al final de la barra de estado | el tirador de esa barra: la pone si no estaba y la quita si estaba |
| una sección de la barra de herramientas | cuelga su desplegable; otra vez en la misma, lo recoge |
| una fila de un desplegable o de una lista | la marca y manda su tecla (o pone el tema) |
| fuera de un desplegable | lo recoge, y el clic se absorbe: no mueve el cursor de detrás |
| fuera de una lista o de un campo | nada: son una ventana con trabajo dentro, no se cierran por error |
| la rueda | una fila arriba o abajo, o recorrer la lista si hay una delante |

Soltar el botón, o moverse sin apretar nada, no es ninguna orden. El botón del
medio tampoco.

**Clic mientras se escribe en una celda**: si el clic cae en otra celda de la rejilla,
primero se termina la escritura como con `Esc` (y se recalcula) y luego se mueve el
cursor alli. Asi no se pierde lo escrito por llevar la mano al raton. Los sitios que
no son celdas de la rejilla —la barra de herramientas, las pestanas, el chip, el nombre
del archivo— no terminan la escritura: hacen lo suyo, y cambiar de hoja por clic
descarta lo que se estaba escribiendo sin recalcular. **Clic mientras se escribe el
nombre de una hoja**: corta la escritura del nombre y la descarta, y hace lo que
toque.

## La barra de las herramientas

Cinco secciones, cada una con sus items de nombre y tecla, en una sola tabla
(`TUI/tools.c:11-39`); de ahí salen y el dibujo y el clic. Delante lo que más se usa
en cada sitio, para que lo que no quepa en una pantalla corta sea lo de menos:

| Sección | Items | Teclas |
|---|---|---|
| `doc` | `save`, `save as`, `open`, `new sheet` | `Ctrl+S`, `Ctrl+G`, `Ctrl+Q`, `Ctrl+A` |
| `edits` | `copy`, `paste`, `undo`, `redo` | `Ctrl+C`, `Ctrl+V`, `Ctrl+Z`, `Ctrl+Y` |
| `style` | `bold`, `line`, `align` | `Ctrl+B`, `Ctrl+U`, `Ctrl+O` |
| `view` | `value`, `theme` | `v`, `Ctrl+T` |
| `run` | `run` | `Ctrl+R` |

En una pantalla de ochenta columnas caben las cinco; en una mas corta se caen las de
detras, y todo sigue alcanzable por tecla.

**Quitar la barra.** La barra no lleva botón para quitarse a sí misma: por tecla es
`Ctrl+K`, y por ratón el nombre del archivo al final de la barra de estado, que es
un tirador en los dos sentidos —la barra estaba fuera, la pone; estaba puesta, la
quita—. No cuenta como un cambio del archivo, como no lo cuenta cambiar el tamaño
de las celdas: es una preferencia del usuario, y se guarda al momento en
`$HOME/.config/celex/config`, en la línea `tools off`.

**Volver a ponerla.** Los mismos dos sitios la devuelven, y además el chip `[tools]`
al final de la fila de las pestañas, que solo se pinta mientras la barra está fuera.
`Ctrl+K` hace lo mismo desde el teclado. El aviso que deja la barra es `tools off`,
corto a propósito: un aviso largo se comería el nombre, que es justo lo que hay que
ver y tocar.

**Con una ventana delante.** La tecla y el clic pasan a ser de la ventana
(`TUI/keys.c:601`): las flechas, la rueda, `j` y `k` recorren la lista, `Enter`
confirma y `Esc` cancela. Las teclas que no son de ella la cierran y se hacen lo de
siempre, para no quedarse encerrado dentro; un clic fuera de un desplegable lo
recoge sin mover nada de detrás.

## La barra de estado como aviso

Lo que se puede tocar son los sitios de arriba: la barra con sus desplegables, las
pestañas, el chip y el nombre del archivo. Lo que la barra de estado dice es el
menú de resultados: `saved`, `copied D12`, `tools off`, `align center`, `4x2`, `VAL`
o `SRC`, el nombre del tema que se está probando en la ventana, y el número de
compilación con el que se está trabajando.

## Donde encaja con lo demas

- Donde esta cada barra y cuanto mide: [pantalla.md](pantalla.md).
- Lo que hacen `Ctrl+S` y `Ctrl+E` sobre los ficheros: [archivos.md](archivos.md).
- Lo que hace cada tecla de formato: [temas-y-formatos.md](temas-y-formatos.md).

Volver al indice: [docs/README.md](README.md).
