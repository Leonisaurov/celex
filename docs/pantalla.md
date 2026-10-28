# La pantalla

Todo lo que se ve se escribe con secuencias ANSI sobre un flujo de bytes, en las
dos plataformas. Lo que hay en cada fila, de arriba a abajo.

## El reparto

```
 doc edits style view run                         <- la barra de las herramientas (1 fila)
      A    B    C    D    E    ...                <- la cabecera de las letras (1 fila)
     ┼────┼────┼────┼────┼────
   1 │  1 │A1 +│    │    │                        <- la rejilla
     │    │ A2 │    │    │
     ┼────┼────┼────┼────┼────
   2 │    │    │    │    │
     ┼────┼────┼────┼────┼────
Sheet1|gastos                                     <- la fila de las pestañas (1 fila)
 d07d1b6    A1  NORMAL  SRC  4x2   notas.clex     <- la barra de estado (1 fila)
```

Encima de todo eso, y solo cuando algo lo pide, hay una **ventana flotante**: el
desplegable de una sección de la barra, la lista de los temas o el campo de texto
para guardar con otro nombre. No es una fila más — se pinta dentro del fotograma,
tapando la rejilla (`TUI/popup.c`, `TUI/cell_manager.c:1539`).

| Parte | Cuánto | Dónde se decide |
|---|---|---|
| margen de los números de fila | 5 columnas: cuatro para el número y una de aire | `HEADER_WIDTH` en `TUI/cell_manager.c:41` |
| barra de las herramientas | 1, si está puesta | `MENU_HEIGHT`, `menu_row` |
| cabecera de las letras | 1 fila | `HEADER_HEIGHT` |
| cada celda | `cellWidth` columnas y `cellHeight` filas, más una raya | `STRIDE_X` y `STRIDE_Y` |
| fila de las pestañas | 1 | `SHEET_BAR_HEIGHT` |
| barra de estado | 1 | `STATUS_HEIGHT` |

El margen de arriba mide una fila o dos, según la barra esté puesta
(`header_height`, `TUI/cell_manager.c:58`). La rejilla acaba donde empieza lo
demás:

```
grid_bottom = alto - 1 - 1
```

Escondiendo la barra la rejilla gana la fila de arriba, y lo mismo pasa con las
letras, que suben un sitio (`TUI/cell_manager.c:58-66`). Con una pantalla muy
corta se fingen como mínimo 20 columnas y 6 filas, para que siempre quepa algo
(`TUI/cell_manager.c:1679-1680`).

## La rejilla

- **Las celdas no se tocan**: entre una y otra hay una columna de raya, y entre
  filas una raya entera. Eso es lo que hace que una celda cortada por el borde de
  la ventana se vea cortada y no pegada a la siguiente.
- **Los números de fila van alineados a la derecha** dentro del margen, y se pegan
  arriba en cuanto la fila asoma: aunque la celda esté medio salida por abajo, el
  número se lee (`TUI/cell_manager.c:1798-1806`).
- **Las letras de columna** van en la fila de arriba, repetidas por toda la
  cabecera.
- **La celda del cursor enseña su fuente**, aunque estén puestos los valores: así
  la fórmula que tiene siempre se ve (`TUI/cell_manager.c:1828-1837`).
- **La celda copiada** se marca con un borde punteado, como las hormigas
  marchantes (`┄` y `┆`, `TUI/cell_manager.c:86-87`).
- **Lo que no cabe en la celda no pasa a la vecina**: se corta por la celda y se
  reparte en líneas de `cellWidth` caracteres. Cuando la celda se está editando se
  ve el trozo donde está el cursor de texto; si no, los primeros caracteres
  (`TUI/cell_manager.c:1859-1877`). Mientras se escribe, la barra de estado muestra
  el texto entero.

## Deslizarse en vez de saltar

La vista no se mueve de celda en celda: `originX` e `originY` cuentan **columnas y
filas de pantalla desplazadas**, no celdas (`TUI/cell_manager.h:48-52`). Al llegar
al borde, la vista se desliza carácter a carácter y quedan celdas cortadas por los
dos lados: la que entra asomando y la que sale todavía medio visible.

Cuando hay que deslizar para seguir al cursor, el movimiento se parte en
fotogramas intermedios (`TUI/cell_manager.c:2070-2088`):

| Cuanto se mueve | Fotogramas | Por qué |
|---|---|---|
| 1 | ninguno | ya está en su sitio de uno |
| 2 | 1 | |
| 3 o más | 2 (el máximo) | más sería perder las teclas |
| más de 8 | ninguno, de golpe | animarlo dejaría al editor detrás de lo que teclea (`PAN_MAX_DISTANCE`) |

Cada fotograma espera 9 ms (`PAN_FRAME_MICROSECONDS`), que es el ritmo de una
terminal de sesenta por segundo: más junto se perdería. En Windows ese espera se
hace mirando el reloj de alto rendimiento en vez de dormir, porque el `Sleep` de
Windows no baja de unos 15 ms y sería demasiado lento para un deslizamiento
(`TUI/cell_manager.c:2042-2062`).

## El fotograma y por qué solo se escribe lo que cambia

Cada cuadro se construye entero en un buffer y luego se compara con el anterior,
por columnas y por filas; solo se escriben a la terminal los trozos que cambiaron.
Cada sitio del buffer ocupa 8 bytes: hasta 4 para un carácter UTF-8 completo, uno
para el **número de estilo** y tres de relleno para que comparar dos fotogramas
sea comparar bytes (`TUI/cell_manager.c:74-78`).

De ahí dos cosas que importan:

- **Cambiar de tema obliga a olvidar el fotograma** (`invalidate_screen`,
  `TUI/cell_manager.c:2026-2031`). El buffer guarda números de estilo, no
  secuencias: si cambia el tema y no cambia el número, la comparación no ve
  diferencia y no se repintaría nada. Lo mismo al cambiar el tamaño de las celdas,
  al mudar o quitar una hoja, al renombrarla y al poner o quitar la barra de
  herramientas. Por eso también la vista previa de un tema al navegar la lista
  olvida el cuadro: los números del fotograma son de los estilos del tema viejo.
- **Las secuencias se agrupan por texto, no por número.** Dos estilos que pintan
  igual se escriben como una sola secuencia, porque cada secuencia empieza apagando
  lo que hubiera (`TUI/cell_manager.c:1339-1362`). En un tema sin color casi todos
  los estilos son el mismo apagado, y eso es lo que mantiene un fotograma sin
  color tan pequeño como antes de existir los colores.

## La barra de las herramientas

Arriba de todo, una sección detrás de otra: `doc`, `edits`, `style`, `view`,
`run`. Van con un aire a la izquierda (`MENU_MARGIN`, una columna), para que no
empiecen pegadas al borde, y la fila entera lleva el color de la barra del tema
(`STYLE_TOOLS`), como la barra de estado. Lo que mide cada una y dónde empieza lo
dice `menu_fit`, y es el mismo cálculo el que dibuja y el que clica
(`TUI/tools.c:81-114`), así que lo que se toca es lo que se ve. Lo que no cabe en
el ancho se queda sin pintar, no se corta: se caen las secciones de detrás, que
son las de menos uso.

La barra no tiene ninguna orden propia: cada opción de cada desplegable es la
tecla de siempre escrita (`TUI/tools.h:4-8`). Quitar la barra con
`Ctrl+K` deja una fila más de rejilla; lo que queda para volver a ponerla es el
chip `[tools]` de las pestañas y el nombre del archivo de la barra de estado.

## Las dos barras de abajo

Fuera del fotograma se escriben solo estas dos, cada una con su memoria: se salta
su repintado si no cambió nada, guardando lo que escribió la vez anterior. La de
arriba —la barra de las herramientas— no necesita memoria propia: vive dentro del
fotograma y la alcanza su comparación por diferencias.

### La barra de estado

De izquierda a derecha, separados por dos espacios:

| Campo | Ancho | Ejemplo |
|---|---|---|
| número de compilación | relleno a 8 | `d07d1b6 ` |
| referencia de la celda del cursor | sin relleno | `A1`, `AB1204` |
| modo | relleno a 6 | `NORMAL`, `EDIT` |
| valores o fuentes | relleno a 3 | `VAL`, `SRC` |
| tamaño de celda | relleno a 4 | `4x2` |
| celda copiada | relleno a 8 | `CP D12` |

Después, en el sitio que queda, una sola cosa, por este orden de prioridad: lo que
ofrece el tabulador, un mensaje, el texto de la celda si se está editando, o el
valor de la celda si están puestos los valores. Y a la derecha del todo, el nombre
del archivo (no la ruta entera), o `[no file]` si no se abrió ninguno.

**El nombre se omite cuando no cabe después del mensaje**: el mensaje es lo que
pasa y el nombre es lo que está. Las columnas que ocupa se apuntan al pintarlo,
para que un clic encima caiga justo donde se ve.

El modo lo marca solo una cosa: si se está escribiendo dentro de una celda. El
tema no se escribe nunca en la barra: en una pantalla de ochenta columnas va justa
de sitio, y el nombre del tema se dice en el mensaje al cambiarlo.

### La fila de las pestañas

Un nombre por hoja, separados por una raya. La que se está viendo va marcada, como
la celda seleccionada. Cuando no caben todas se cortan, pero nunca la que se está
viendo: se queda en medio de la fila y una `<` a la izquierda avisa de que quedan
hojas por allí (`TUI/cell_manager.c:2245-2286`).

Mientras se escribe un nombre con `Ctrl+F`, la pestaña que se está viendo muestra
lo que se va tecleando.

Con la barra de las herramientas quitada se escribe además, al final de la fila, el
chip `[tools]`, que es lo que queda para tocar y volver a ponerla.

## Las ventanas flotantes

Una sola a la vez, y dentro del fotograma: se pinta después de la rejilla y antes
de comparar con el cuadro anterior (`TUI/cell_manager.c:1961-1962`), así que el
redibujado por diferencias la alcanza igual que a las celdas y puede taparlas sin
romper nada. Tres formas (`TUI/popup.h`):

| Forma | Qué es | Cómo se cierra |
|---|---|---|
| desplegable | las opciones de una sección de la barra | Enter manda la tecla de la fila marcada; Escape o un clic fuera lo recoge |
| lista | los temas, cada nombre escrito con su propio tema | las flechas o la rueda mueven la selección y ponen el tema al instante; Enter confirma, Escape deshace la prueba |
| campo | guardar con otro nombre (`Ctrl+G`) y abrir (`Ctrl+Q`) | Enter escribe o lee; Escape cancela |

Al cancelarse, lo que estaba puesto antes de abrir vuelve a su sitio: la vista
previa de un tema no manda por sí sola (`popup_cancel`, `TUI/popup.c:210`).

Con la ventana delante, la tecla y el clic son de la ventana: las teclas que no
son de ella la cierran y se hacen lo de siempre, y un clic fuera de un desplegable
se absorbe, para no mover el cursor detrás de la cortina (`TUI/keys.c:362`).

## Al cambiar el tamaño de la terminal

El bucle mira el tamaño en cada vuelta; si cambió, se olvida el fotograma, se
limpia la pantalla y se pinta todo entero, incluidas las dos barras de abajo
(`TUI/cell_manager.c:1967-1974`).

## Donde encaja con lo demas

- Las teclas y los clics que cambian cada una de esas partes: [teclas-y-raton.md](teclas-y-raton.md).
- Los estilos que lleva cada celda del fotograma: [temas-y-formatos.md](temas-y-formatos.md).
- Donde se guarda el tamano de celda y la barra de herramientas: [archivos.md](archivos.md).

Volver al indice: [docs/README.md](README.md).
