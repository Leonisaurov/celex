# Ideas y camino

Notas de lo que queda por hacer y de como se piensa atacarlo, para que no se
pierdan entre commits. Lo que hay hecho esta en el README; aqui vive lo que esta
en la cabeza.

## Formatos de celda

Lo hecho: alineacion, negrita y subrayado por tecla (`Ctrl+O`, `Ctrl+B`,
`Ctrl+U`), colores propios por numero de paleta, y todo ello en el `.clex` con
una linea `format REF tokens...` por celda. La alineacion de empezar es la de una
hoja de calculo: numero a la derecha, lo demas a la izquierda. El fuente de la
celda no se toca, y el `.cle` sale sin formatos.

Lo que se deja para despues, y por que:

- **Los colores por botones o por ventana.** Hoy un color propio solo se pone
  editando el `.clex`. Hace falta un item mas en `style` que abra una ventana con
  la paleta, como la de los temas: dieciseis numeros pintados con su propio numero,
  y una tecla `Ctrl+W` que pase el color de delante y `Ctrl+X` el de detras para
  quien viva a teclado. Las dos teclas estan libres hoy (el 8 no: es el retroceso).
- **Formatos de numero** (`#,##0.00`, porcentajes, fechas). Hace falta decidir
  como se ven los numeros —el lenguaje solo tiene enteros— y eso es un cambio del
  lenguaje, no del editor. Mejor despues de los numeros decimales.
- **Bordes** entre celdas (la rejilla es un dibujo, no bordes por celda). Haria
  falta una segunda capa de dibujo por encima de la rejilla.
- **Copiar el formato con la celda** (`Ctrl+C` lleva ahora solo el fuente). Se
  puede juntar con pegar-formato (`Ctrl+Shift+V`) cuando haya seleccion de rango.
- **Deshacer un formato**: el historial guarda fuentes, no formatos, y
  formatear si marca la hoja como cambiada.

## Herramientas a la vista, y raton

Lo hecho: la barra arriba de todo, partida en secciones (`doc`, `edits`, `style`,
`view`, `run`) con sus items de nombre y tecla en `TUI/tools.c`, y `Ctrl+K` (o un
clic en el nombre del archivo) para quitarla y devolver la fila a la rejilla. Ningun
boton tiene acciones propias: cada uno *es* su tecla, asi que la barra y el teclado
no pueden contradecirse. Lo que no cabe en el ancho se cae por detras, sin pintar y
sin clica, como las pestanas. En las preferencias del usuario se guarda `tools off`
(y en el documento, en la primera version del formato), y sin esa linea la barra
esta puesta.

Cada seccion cuelga una **ventana flotante**, y con ellas han llegado los unicos
sitios donde antes no habia mas que la rejilla: la lista de los temas, donde cada
nombre se escribe con su propio tema y se prueba al instante al moverse
(`Ctrl+T`), y el campo de texto para guardar con otro nombre (`Ctrl+G`) o abrir un
archivo (`Ctrl+Q`). La ventana es de la pantalla, no del documento: no se guarda.

Quitar la barra quita el boton de quitarla, asi que quedan dos tiradores para
volver: el chip `[tools]` al final de la fila de las pestanas, que solo se pinta
mientras la barra esta fuera, y el nombre del archivo al final de la barra de
estado, que esta siempre. El aviso que deja es `tools off`, corto a proposito: la
barra va justa y un aviso largo se tapa el nombre, que es justamente lo que hay
que poder ver y clica.

El raton tambien esta: la terminal manda `ESC[<boton;columna;fila M` (o `m` al
soltar) y eso es lo que lee `TUI/input.c`. En Windows, la consola da `MOUSE_EVENT`
en vez de secuencias, y `TUI/tui_platform.c` los convierte en la misma secuencia
para que solo haya un camino despues. El reporte se pide al entrar y se apaga al
salir, no sea que la terminal se quede mandando clics a lo que venga detras.

Un clic en una celda mueve el cursor, dos clics encima de la misma abren la celda
a escribir, el boton derecho copia, la rueda pasa de fila, una pestana cambia de
hoja y un boton manda su tecla. Escribiendo en una celda, un clic en otra termina
la escritura primero, como el Escape: no se pierde lo escrito por llevar la mano
al raton.

Lo que queda:

- **Seleccion de rango con el arrastre.** Pide pedir tambien los movimientos con
  el boton apretado (`ESC[?1002h`) y un principio y un final en la hoja. Va junto
  con la seleccion de rango por teclado, que es la pieza que desbloquea mas cosas.
- **Los colores por boton o por ventana.** Hoy un color propio solo se pone
  editando el `.clex`. Una paleta de 16 numeros no cabe en una fila de botones,
  pero una ventana flotante con las opciones si, como la de los temas: cada numero
  escrito con su propio color, y elegirlo deja la celda pintada al instante.
- **El tiempo del doble clic** esta fijado en 500 ms (`CLICK_DOUBLE_MS`). Si algun
  dia se quisiera configurable, seria una linea mas en el documento.
- **Donde no hay raton** (una tuberia, las pruebas) todo sigue siendo de teclado,
  y las pruebas mandan los clics como bytes en el archivo de teclas: asi se
  comprueban los dos caminos en la misma prueba.

## Lo que falta alrededor

- **Seleccion de rango** (mayusculas + flechas, o arrastre). Sin ella no tiene
  sentido rellenar, borrar un bloque, ni copiar formatos en grupo. Es la pieza
  que desbloquea mas cosas.
- **Rellenar** hacia abajo desde la seleccion. Ojo con la tecla: `Ctrl+D` ya esta
  ocupada, es la que quita la hoja, asi que habra que buscar una libre cuando exista
  la seleccion de rango. Y **autocompletar por serie** (1,2,3 → sigue la serie).
- **Numeros decimales y fechas** en el lenguaje, que es lo que abre los formatos
  de numero.
- **Barra de formulas**: un campo encima de la rejilla donde se ve y se edita el
  fuente de la celda, como en una hoja de calculo de verdad.
- **Busqueda y reemplazo** en la hoja.
- **Proteger una hoja** o una celda, para que el formato no se mueva.

## Como se comprueba

La referencia esta en [pruebas.md](pruebas.md). Lo que hay hecho se comprueba en el
ida y vuelta del documento (`tests/editor_test`), en las teclas y los clics
(`tests/keys_test`), en las secuencias que llegan de la terminal
(`tests/input_test`) y en el editor entero corrido con un archivo de teclas
(`tests/app_test`) y dentro de una consola de verdad (`tests/console_test`), que es
donde se ve que el `.cle` exportado sale limpio y el `.clex` guarda formatos, fila
de herramientas y clics.

Lo que queda tiene su prueba pensada: la seleccion de rango, con el borde pintado y
el borrado en bloque; rellenar, con el ida y vuelta del documento; y los numeros
decimales, con un caso nuevo en `tests/cases`.

## Lo que se vio al escribir la documentacion

Escribir `docs/` fue pasar fichero por fichero, y salieron cosas que estan metidas
y no hacen nada, o que no cuadran. Ninguna es un fallo por el lado de usar el
editor; es limpieza, y cada una merece su commit. Ya se quitaron algunas (`run_external_script`
y su temporal, `current_sheet`, una comprobacion escrita dos veces en el pintado, y el
`sprintf` sin limite de `ref_to_string`, ahora `snprintf`). Quedan:

- **`copy_cell_content` no la usa el programa**: el `Ctrl+C` compone el
  portapapeles a mano en `TUI/keys.c`, sin pasar por esta ayuda
  (`TUI/cell_manager.c:810`). Solo la llaman las pruebas. O se usa desde el
  `Ctrl+C`, o se quita y se mueve al fichero de pruebas.
- **`clipboard_value` no se lee nunca**: el valor de la celda copiada se guarda en el
  portapapeles y no se pega jamas (`TUI/clipboard.c:96`). Si algun dia se quiere
  pegar solo el valor, ahi esta; si no, ese `value` es peso muerto.
- **Tokens y opcodes que no llegan a existir**: `TOKEN_TYPE`, `TOKEN_DEF` y
  `TOKEN_BOOL` los enumera el escaner pero no los produce, y `OP_GOTO`, `OP_GOTOB`,
  `OP_UPV`, `OP_IF`, `OP_DEF` y `OP_ASIGN` estan en el chunk sin que el analizador
  emita ninguno. Andamio de algo que no se construyo.
- **Un error de ejecucion sale con 0 y un error de analisis sale con error (255)**.
  `print 1 / 0` avisa, para el programa y termina bien; `A1 = 3 +` no. Hay que
  unificarlos: que la maquina avise y que el CLI devuelva error. Los casos de ciclo
  del runner no lo pillan porque esos fallan en el reordenado, que si devuelve
  error.
- **El aviso de una celda que no se pudo calcular se trunca al copiarlo**: se compone
  en un sitio de 512 (`Sheet/sheet.c:144`) y se guarda en uno de 128
  (`TUI/keys.c:19`, el mensaje de `recalculate_sheet`). Con varias celdas malas en un
  fuente largo se pierde el final del aviso. Es solo el aviso, no el calculo.
- **Una comprobacion escrita dos veces, pegada**: `if (content[0] == '\0') continue;`
  en `TUI/cell_manager.c:1659-1665`.
- **`sprintf` sin limite** en `ref_to_string` (`Common/ref.c:30`) y en los `print`
  del lenguaje: el sitio se calcula antes de escribir, asi que hoy cabe, pero no lo
  comprueba nadie.
- **Cuantas secciones caben en la barra** depende del ancho, y esta dicho en el
  codigo: `menu_fit` deja las que caben enteras y se caen las de detras, que son las
  de menos uso. En 80 columnas caben las cinco. Si la lista de items crece, lo que
  toca engordar es una seccion y no la barra: el desplegable mide lo que mida la
  fila mas larga.
- **La ventana flotante no se puede agrandar con la pantalla**: el alto de una lista
  se recorta a lo que cabe, y las filas de mas no se ven ni se clica. Con temas de
  sobra (`POPUP_LIMIT`) haria falta una lista con desplazamiento propio.

Volver al indice: [docs/README.md](README.md).
