# Hojas, portapapeles, deshacer y autocompletado

## Las hojas

Un archivo tiene hojas, y cada hoja es una hoja de cálculo entera por su cuenta.

**Es propio de cada hoja** (`SheetState`, `TUI/cell_manager.h:45-61`):

| Qué | Nota |
|---|---|
| sus celdas | un mapa con clave `x,y` |
| su cursor | dónde estaba, al volver la encuentra |
| su deslizamiento | `originX` e `originY`, cuántas columnas y filas de pantalla había movida la vista |
| su historial | lo que se puede deshacer y rehacer, sin mezclar con otras hojas |
| si estaba editando una celda | al cambiar de hoja se empieza sin editar |

**Es de la pantalla, y no de una hoja** (`CellManager`): lo que se está viendo, el
tamaño de la terminal, si se muestran valores o fuentes, el mensaje de la barra, el
portapapeles, lo que ofrece el tabulador, si se está escribiendo un nombre de hoja,
y si la fila de herramientas está puesta.

| Tecla | Qué hace | Lo que dice la barra |
|---|---|---|
| `Ctrl+A` | añade una hoja detrás de la que se ve, y se pone a verla | su nombre, o `cannot add a sheet` |
| `Ctrl+N` / `Ctrl+P` | la siguiente / la anterior, dando la vuelta | el nombre de la que queda |
| `Ctrl+D` | quita la que se ve | `sheet removed`, o `the last sheet stays` |
| `Ctrl+F` | escribe el nombre de la que se ve; `Enter` lo fija, `Esc` lo deja | `new name: Enter to set, Esc to leave` |
| clic en una pestaña | va a esa hoja | su nombre |

**Nombres.** Hasta 23 caracteres (el sitio mide 24 con el final de cadena) y no se
repiten: una hoja no recibe un nombre que ya tiene otra, y `sheet_rename` no hace
nada si el nombre está tomado — se queda como estaba, sin avisar. Si se crea una
hoja sin nombre se pone el primero libre de la serie `Sheet1`, `Sheet2`, `Sheet3`,
saltando los que ya estén usados.

**Al quitar una hoja** las de detrás corren un puesto, y la que se está viendo se
mueve con ellas: si se quitó una de delante, baja uno; si la que se veía desaparece,
se ve la que quedó en su sitio. La última no se quita nunca: una hoja de cálculo sin
hojas no es una hoja de cálculo.

**Al cambiar de hoja** se olvida la marca de la celda copiada (la marca es de una
hoja; lo copiado es del archivo entero), se apaga la oferta del tabulador y se
redibuja la pantalla entera. El cursor, la vista y el historial de la hoja que se
deja se conservan donde estaban.

**"Hay cambios sin guardar"** mira todas las hojas, no la que se ve. Al guardar
pasan todas a estar guardadas a la vez. Guardar cambios es lo que hace pedir
confirmación al salir con `q` y al recargar con `Ctrl+L`.

**Lo que se guarda de cada hoja** es todo: su nombre, dónde estaba el cursor,
cuánto deslizada la vista, sus celdas y sus formatos. Al abrir un documento se ve
la primera hoja, como al abrir un archivo.

## El portapapeles

Es del editor y uno solo para todas las hojas, como en una hoja de cálculo de
verdad. Guarda el texto, el valor y de qué celda viene.

| Acción | Qué entra | Marca | La barra dice |
|---|---|---|---|
| `Ctrl+C` con la celda | el fuente **y** el valor | sí: borde punteado sobre la celda | `copied A1` |
| `Ctrl+C` mientras se edita | solo el texto | no: la marca se quita | `copied` |
| la terminal pega un texto | ese texto | no, a menos que sea el mismo que ya estaba | `pasted` |

Al copiar se escribe también en el portapapeles del sistema (en Windows por su API;
fuera se prueban `xclip`, `wl-copy` y `pbcopy`), para que el Windows Terminal pegue
la celda copiada con su propio `Ctrl+V`. Solo viaja el fuente, nunca el valor.

`Ctrl+V` hace dos cosas distintas según dónde se esté:

- **en una celda de la rejilla**: entra la fórmula entera y se recalcula la hoja;
- **mientras se edita**: el texto entra donde está el cursor de texto, y no se
  recalcula hasta que se sale de la edición.

Pegar con el portapapeles vacío dice `the clipboard is empty`. Y un texto pegado no
puede llevar saltos de línea: donde había un salto, un tabulador o un retorno de
carro queda un espacio, porque en la hoja hay una línea por celda.

Lo pegado desde la terminal (el pegado con marcadores, `ESC[200~ ... ESC[201~`, y
también una ráfaga de caracteres que llegan juntos) ocupa el portapapeles del
editor. Si el texto es el mismo que ya estaba, el portapapeles no cambia y la celda
copiada sigue marcada.

## Deshacer y rehacer

`Ctrl+Z` deshace, `Ctrl+Y` rehace (y `Ctrl+Z` con la hoja vacía dice
`nothing to undo`, `Ctrl+Y` sin nada que rehacer `nothing to redo`). Después de
cualquiera de los dos se recalcula la hoja.

- **Qué se guarda**: el fuente de una celda, el antes y el después. Nada más.
- **Escribir una palabra es un solo paso**: si el cambio nuevo es de la misma celda
  que el último guardado, se actualiza ese último en vez de añadir otra entrada.
- **Un cambio nuevo descarta lo que se iba a rehacer**.
- **No hay tope de pasos**: crece mientras haya memoria. Si un `realloc` falla, el
  cambio no se guarda y se sigue trabajando, sin avisar.
- **Por hoja**: cada una lleva su pila. Cambiar de hoja no borra nada.
- **Se limpia** al vaciar la hoja (`Ctrl+L`, importar, cargar un documento) y al
  quitar la hoja.

**Lo que NO se puede deshacer**, hoy: los formatos de celda (`Ctrl+B`, `Ctrl+U`,
`Ctrl+O` marcan la hoja como cambiada pero no dejan rastro), crear, quitar, mover o
renombrar hojas, cambiar el tamaño de las celdas, el tema, los valores o fuentes, y
la posición del cursor. Está apuntado en [ideas.md](ideas.md).

## El autocompletado

`Tab` mientras se edita una celda. Ofrece, por este orden, las **palabras del
lenguaje** (`if`, `else`, `while`, `for`, `in`, `print`, `input`, `return`, `true`,
`false`) y las **palabras que ya hay en la hoja**, troceando el contenido de cada
celda. Como mucho 16 candidatas, de hasta 64 bytes, sin repetir, comparando sin
importar mayúsculas. Lo que ya está escrito igual exacto no se ofrece.

Cómo se recorre:

- **una sola candidata**: se escribe entera y se acabó;
- **varias**: el primer `Tab` escribe lo que todas tienen al principio, y cada `Tab`
  siguiente va pasando por las candidatas; al dar la vuelta a la lista se vuelve a
  lo que estaba escrito y la oferta se apaga.

Mientras hay oferta, la barra de estado la enseña: las candidatas separadas por
espacios, y la que está puesta ahora entre llaves. Escrito `pr` con dos candidatas
(`print` y `prueba`), la barra dice ` print  prueba` y luego, al pasar,
` {print}  prueba`. La oferta tiene prioridad sobre el mensaje y sobre el valor de
la celda, porque es lo que se está eligiendo.

Se apaga al escribir, al borrar, al empezar a editar y al cambiar de hoja: la
palabra que se está escribiendo ya no es la que se ofreció.

## Entrar a editar, y lo que se ve

Al entrar con `i` o con `Enter` el cursor de texto se pone al final del contenido
que había. Con una letra se entra **borrando** el contenido. Mientras se edita, la
celda del cursor enseña siempre su fuente y la barra de estado enseña el texto
entero, porque en la rejilla solo cabe el trozo que entra en la celda. El hueco del
cursor de texto se queda sin estilo, para que se vea dentro del bloque de la celda
seleccionada.

El contenido de una celda no tiene tope de longitud. El único tope al escribir es
el del archivo: al cargar un documento cada fuente cabe en 1023 caracteres, y lo que
pasa de ahí se corta en silencio.

## Donde encaja con lo demas

- Que se guarda de cada hoja y que se queda fuera: [archivos.md](archivos.md).
- Las teclas de hoja y de portapapeles, y los clics en las pestanas: [teclas-y-raton.md](teclas-y-raton.md).
- Como se pinta la fila de las pestanas y cuanto cabe: [pantalla.md](pantalla.md).

Volver al indice: [docs/README.md](README.md).
